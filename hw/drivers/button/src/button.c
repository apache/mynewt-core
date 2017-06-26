/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#include <os/os.h>
#include <os/os_dev.h>
#include <assert.h>
#include <button/button.h>
#include <hal/hal_gpio.h>

static void
button_timer_exp(void *arg)
{
    struct button_dev *dev;
    int gpio_state;

    dev = (struct button_dev *) arg;
    assert(dev != NULL);

    /* Timer expired, this means that we should change the state of the
     * button.
     */
    gpio_state = hal_gpio_read(dev->bd_cfg->bc_pin);

    if ((dev->bd_cfg->bc_invert == 1 && gpio_state == 0) ||
        (dev->bd_cfg->bc_invert == 0 && gpio_state == 1)) {
        dev->bd_state = BUTTON_STATE_PRESSED;
    } else {
        dev->bd_state = BUTTON_STATE_NOT_PRESSED;
    }

    /* Handle notifications */
    if (dev->bd_notify_cb) {
        dev->bd_notify_cb(dev, dev->bd_notify_arg, dev->bd_state);
    }
}

static void
button_irq_handler(void *arg)
{
    struct button_dev *dev;

    dev = (struct button_dev *) arg;
    assert(dev != NULL);

    /* Continuously reschedule the timer for the future, state will only change
     * once the timer fires.
     */
    os_cputime_timer_relative(&dev->bd_timer, dev->bd_cfg->bc_debounce_time_us);
}

/**
 * Called through OS device initialization, this function initializes a
 * button device.
 *
 * @param The button device to initialize
 * @param A pointer to the button configuration to initialize this device with
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
button_init(struct os_dev *dev, void *arg)
{
    struct button_dev *bd;
    struct button_cfg *cfg;
    int rc;

    bd = (struct button_dev *) dev;
    cfg = (struct button_cfg *) arg;

    bd->bd_cfg = cfg;

    rc = hal_gpio_irq_init(cfg->bc_pin, button_irq_handler, bd,
        HAL_GPIO_TRIG_BOTH, cfg->bc_pull);
    if (rc != 0) {
        goto err;
    }
    os_cputime_timer_init(&bd->bd_timer, button_timer_exp, (void *) bd);

    /* Enable the GPIO IRQ now that we're watching this button */
    hal_gpio_irq_enable(bd->bd_cfg->bc_pin);

    return (0);
err:
    return (rc);
}

/**
 * Call the notify callback when the button is pressed.
 *
 * @param The button device to set the callback on.
 * @param The notify callback to call
 * @param The argument to pass to the notify callback
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
button_notify(struct button_dev *bd, button_notify_cb_t notify_cb, void *arg)
{
    bd->bd_notify_cb = notify_cb;
    bd->bd_notify_arg = arg;

    return (0);
}

/**
 * Read the button state, either pressed or not pressed.
 *
 * @param The button device to read
 *
 * @return button state, either pressed or not pressed.
 */
uint8_t
button_read(struct button_dev *bd)
{
    return (bd->bd_state);
}
