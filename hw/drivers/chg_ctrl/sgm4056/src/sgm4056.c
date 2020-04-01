/*
 * Copyright 2020 Casper Meijn <casper@meijn.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sgm4056/sgm4056.h"
#include "hal/hal_gpio.h"

#if MYNEWT_VAL(SGM4056_USE_CHARGE_CONTROL)

static int
sgm4056_chg_ctrl_get_status(struct charge_control *chg_ctrl, int *status)
{
    struct sgm4056_dev *dev;
    int rc;

    dev = (struct sgm4056_dev *)CHARGE_CONTROL_GET_DEVICE(chg_ctrl);
    if (dev == NULL) {
        rc = SYS_ENODEV;
        goto err;
    }

    rc = sgm4056_get_charger_status(dev, (charge_control_status_t *)status);

err:
    return rc;
}

static int
sgm4056_chg_ctrl_read(struct charge_control *chg_ctrl,
                      charge_control_type_t type,
                      charge_control_data_func_t data_func,
                      void *data_arg, uint32_t timeout)
{
    int rc = 0;
    int status;

    if (type & CHARGE_CONTROL_TYPE_STATUS) {
        rc = sgm4056_chg_ctrl_get_status(chg_ctrl, &status);
        if (rc) {
            goto err;
        }

        if (data_func) {
            data_func(chg_ctrl, data_arg, (void *)&status, CHARGE_CONTROL_TYPE_STATUS);
        }
    }

err:
    return rc;
}

static const struct charge_control_driver sgm4056_chg_ctrl_driver = {
    .ccd_read = sgm4056_chg_ctrl_read,
    .ccd_get_config = NULL,
    .ccd_set_config = NULL,
    .ccd_get_status = sgm4056_chg_ctrl_get_status,
    .ccd_get_fault = NULL,
    .ccd_enable = NULL,
    .ccd_disable = NULL,
};

static void
sgm4056_interrupt_event_handler(struct os_event *ev)
{
    struct sgm4056_dev *dev = (struct sgm4056_dev *)ev->ev_arg;
    assert(dev);

    charge_control_read(&dev->chg_ctrl, CHARGE_CONTROL_TYPE_STATUS,
            NULL, NULL, OS_TIMEOUT_NEVER);
}

static void
sgm4056_irq_handler(void *arg)
{
    struct sgm4056_dev *dev = (struct sgm4056_dev *)arg;
    assert(dev);

    os_eventq_put(os_eventq_dflt_get(), &dev->interrupt_event);
}
#endif

int
sgm4056_dev_init(struct os_dev *odev, void *arg)
{
    struct sgm4056_dev *dev = (struct sgm4056_dev *)odev;
    const struct sgm4056_dev_config *cfg = arg;
    int rc = 0;

    if (!dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    dev->config = *cfg;

#if MYNEWT_VAL(SGM4056_USE_CHARGE_CONTROL)
    dev->interrupt_event.ev_cb = sgm4056_interrupt_event_handler;
    dev->interrupt_event.ev_arg = dev;

    rc = hal_gpio_irq_init(dev->config.power_presence_pin, sgm4056_irq_handler, dev, HAL_GPIO_TRIG_BOTH,
                           HAL_GPIO_PULL_NONE);
    hal_gpio_irq_enable(dev->config.power_presence_pin);
#else
    rc = hal_gpio_init_in(dev->config.power_presence_pin, HAL_GPIO_PULL_NONE);
#endif
    if (rc) {
        goto err;
    }

    rc = hal_gpio_init_in(dev->config.charge_indicator_pin, HAL_GPIO_PULL_NONE);
    if (rc) {
        goto err;
    }

#if MYNEWT_VAL(SGM4056_USE_CHARGE_CONTROL)
    rc = charge_control_init(&dev->chg_ctrl, odev);
    if (rc) {
        goto err;
    }

    rc = charge_control_set_driver(&dev->chg_ctrl, CHARGE_CONTROL_TYPE_STATUS,
                                   (struct charge_control_driver *)&sgm4056_chg_ctrl_driver);
    if (rc) {
        goto err;
    }

    rc = charge_control_set_type_mask(&dev->chg_ctrl, CHARGE_CONTROL_TYPE_STATUS);
    if (rc) {
        goto err;
    }

    rc = charge_control_mgr_register(&dev->chg_ctrl);
    if (rc) {
        goto err;
    }
#endif

err:
    return rc;
}

int
sgm4056_get_power_presence(struct sgm4056_dev *dev, int *power_present)
{
    int gpio_value = hal_gpio_read(dev->config.power_presence_pin);

    *power_present = (gpio_value == 0);
    return 0;
}

int
sgm4056_get_charge_indicator(struct sgm4056_dev *dev, int *charging)
{
    int gpio_value = hal_gpio_read(dev->config.charge_indicator_pin);

    *charging = (gpio_value == 0);
    return 0;
}

int
sgm4056_get_charger_status(struct sgm4056_dev *dev,
                           charge_control_status_t *charger_status)
{
    int power_present;
    int charging;
    int rc;

    rc = sgm4056_get_power_presence(dev, &power_present);
    if (rc) {
        goto err;
    }

    rc = sgm4056_get_charge_indicator(dev, &charging);
    if (rc) {
        goto err;
    }

    if (!power_present) {
        *charger_status = CHARGE_CONTROL_STATUS_NO_SOURCE;
    } else {
        if (charging) {
            *charger_status = CHARGE_CONTROL_STATUS_CHARGING;
        } else {
            *charger_status = CHARGE_CONTROL_STATUS_CHARGE_COMPLETE;
        }
    }

err:
    return rc;
}
