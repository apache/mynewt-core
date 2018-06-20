/**
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

#include <assert.h>
#include "os/mynewt.h"
#include "hal/hal_gpio.h"
#include "mcu/apollo2.h"
#include "mcu/cmsis_nvic.h"
#include "am_mcu_apollo.h"
#include "am_hal_pin.h"
#include "am_hal_gpio.h"

/* GPIO interrupts */
#define HAL_GPIO_MAX_IRQ        8

/* Storage for GPIO callbacks. */
struct hal_gpio_irq {
    int pin_num;
    hal_gpio_irq_handler_t func;
    void *arg;
};

static struct hal_gpio_irq hal_gpio_irqs[HAL_GPIO_MAX_IRQ];

int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    uint32_t cfg;

    cfg = AM_HAL_PIN_INPUT;

    switch (pull)  {
        case HAL_GPIO_PULL_UP:
            cfg |= AM_HAL_GPIO_PULLUP;
            break;
        default:
            break;
    }
    am_hal_gpio_pin_config(pin, cfg);

    return (0);
}

int
hal_gpio_init_out(int pin, int val)
{
    am_hal_gpio_pin_config(pin, AM_HAL_GPIO_OUTPUT);
    hal_gpio_write(pin, val);

    return (0);
}


void
hal_gpio_write(int pin, int val)
{
    if (val) {
        am_hal_gpio_out_bit_set(pin);
    } else {
        am_hal_gpio_out_bit_clear(pin);
    }
}

int
hal_gpio_read(int pin)
{
    int state;

    state = am_hal_gpio_input_bit_read(pin);

    return state;
}

int
hal_gpio_toggle(int pin)
{
    am_hal_gpio_out_bit_toggle(pin);

    return (0);
}

/*
 * GPIO irq handler
 *
 * Handles the gpio interrupt attached to a gpio pin.
 *
 * @param index
 */
static void
hal_gpio_irq_handler(void)
{
    const struct hal_gpio_irq *irq;
    uint64_t status;
    int i;

    os_trace_isr_enter();

    /* Read and clear the GPIO interrupt status. */
    status = am_hal_gpio_int_status_get(false);
    am_hal_gpio_int_clear(status);

    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        irq = hal_gpio_irqs + i;
        if (irq->func != NULL) {
            if (status & AM_HAL_GPIO_BIT(irq->pin_num)) {
                irq->func(irq->arg);
            }
        }
    }

    os_trace_isr_exit();
}

/*
 * Register IRQ handler for GPIOTE, and enable it.
 * Only executed once, during first registration.
 */
static void
hal_gpio_irq_setup(void)
{
    static uint8_t irq_setup;

    if (!irq_setup) {
        NVIC_SetVector(GPIO_IRQn, (uint32_t)hal_gpio_irq_handler);
        NVIC_SetPriority(GPIO_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
        NVIC_ClearPendingIRQ(GPIO_IRQn);
        NVIC_EnableIRQ(GPIO_IRQn);
        irq_setup = 1;
    }
}

/*
 * Find out whether we have an GPIOTE pin event to use.
 */
static int
hal_gpio_find_empty_slot(void)
{
    int i;

    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].func == NULL) {
            return i;
        }
    }
    return -1;
}

static int
hal_gpio_sdk_trig(hal_gpio_irq_trig_t trig)
{
    switch (trig) {
        case HAL_GPIO_TRIG_FALLING: return AM_HAL_GPIO_FALLING;
        case HAL_GPIO_TRIG_RISING:  return AM_HAL_GPIO_RISING;
        default:                    return -1;
    }
}

/**
 * gpio irq init
 *
 * Initialize an external interrupt on a gpio pin
 *
 * @param pin       Pin number to enable gpio.
 * @param handler   Interrupt handler
 * @param arg       Argument to pass to interrupt handler
 * @param trig      Trigger mode of interrupt
 * @param pull      Push/pull mode of input.
 *
 * @return int
 */
int
hal_gpio_irq_init(int pin, hal_gpio_irq_handler_t handler, void *arg,
                  hal_gpio_irq_trig_t trig, hal_gpio_pull_t pull)
{
    int sdk_trig;
    int slot;

    sdk_trig = hal_gpio_sdk_trig(trig);
    if (sdk_trig == -1) {
        return SYS_EINVAL;
    }

    slot = hal_gpio_find_empty_slot();
    if (slot < 0) {
        return SYS_ENOMEM;
    }
    hal_gpio_init_in(pin, pull);

    am_hal_gpio_int_polarity_bit_set(pin, sdk_trig);
    am_hal_gpio_int_clear(AM_HAL_GPIO_BIT(pin));
    am_hal_gpio_int_enable(AM_HAL_GPIO_BIT(pin));

    hal_gpio_irqs[slot].pin_num = pin;
    hal_gpio_irqs[slot].func = handler;
    hal_gpio_irqs[slot].arg = arg;

    hal_gpio_irq_setup();

    return 0;
}

/**
 * gpio irq release
 *
 * No longer interrupt when something occurs on the pin. NOTE: this function
 * does not change the GPIO push/pull setting.
 * It also does not disable the NVIC interrupt enable setting for the irq.
 *
 * @param pin
 */
void
hal_gpio_irq_release(int pin)
{
    /* XXX: Unimplemented. */
}

void
hal_gpio_irq_enable(int pin)
{
    am_hal_gpio_int_enable(AM_HAL_GPIO_BIT(pin));
}

void
hal_gpio_irq_disable(int pin)
{
    am_hal_gpio_int_disable(AM_HAL_GPIO_BIT(pin));
}
