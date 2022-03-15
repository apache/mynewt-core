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
#include "mcu/apollo3.h"
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

int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    am_hal_gpio_pincfg_t cfg =
    {
        .uFuncSel       = 3,
        .eGPOutcfg      = AM_HAL_GPIO_PIN_OUTCFG_DISABLE,
        .eGPInput       = AM_HAL_GPIO_PIN_INPUT_ENABLE,
        .eGPRdZero      = AM_HAL_GPIO_PIN_RDZERO_READPIN
    };

    switch (pull) {
    case HAL_GPIO_PULL_UP:
        cfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_WEAK;
        break;

    case HAL_GPIO_PULL_DOWN:
        cfg.ePullup = AM_HAL_GPIO_PIN_PULLDOWN;
        break;

    default:
        cfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_NONE;
        break;
    }
    am_hal_gpio_pinconfig(pin, cfg);

    return (0);
}

int
hal_gpio_init_out(int pin, int val)
{
    am_hal_gpio_pinconfig(pin, g_AM_HAL_GPIO_OUTPUT);
    hal_gpio_write(pin, val);

    return (0);
}


void
hal_gpio_write(int pin, int val)
{
    if (val) {
        am_hal_gpio_state_write(pin, AM_HAL_GPIO_OUTPUT_SET);
    } else {
        am_hal_gpio_state_write(pin, AM_HAL_GPIO_OUTPUT_CLEAR);
    }
}

int
hal_gpio_read(int pin)
{
    uint32_t state;

    am_hal_gpio_state_read(pin, AM_HAL_GPIO_INPUT_READ, &state);

    return (int)state;
}

int
hal_gpio_toggle(int pin)
{
    am_hal_gpio_state_write(pin, AM_HAL_GPIO_OUTPUT_TOGGLE);

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
    uint64_t status;

    os_trace_isr_enter();

    /* Read and clear the GPIO interrupt status. */
    am_hal_gpio_interrupt_status_get(false, &status);
    am_hal_gpio_interrupt_clear(status);
    am_hal_gpio_interrupt_service(status);

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
    am_hal_gpio_pincfg_t cfg =
    {
        .uFuncSel       = 3,
        .eGPOutcfg      = AM_HAL_GPIO_PIN_OUTCFG_DISABLE,
        .eGPInput       = AM_HAL_GPIO_PIN_INPUT_ENABLE,
        .eGPRdZero      = AM_HAL_GPIO_PIN_RDZERO_READPIN
    };

    switch (pull) {
    case HAL_GPIO_PULL_UP:
        cfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_WEAK;
        break;

    case HAL_GPIO_PULL_DOWN:
        cfg.ePullup = AM_HAL_GPIO_PIN_PULLDOWN;
        break;

    default:
        cfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_NONE;
        break;
    }

    switch (trig) {
    case HAL_GPIO_TRIG_NONE:
        cfg.eIntDir = AM_HAL_GPIO_PIN_INTDIR_NONE;
        break;

    case HAL_GPIO_TRIG_RISING:
        cfg.eIntDir = AM_HAL_GPIO_PIN_INTDIR_LO2HI;
        break;

    case HAL_GPIO_TRIG_FALLING:
        cfg.eIntDir = AM_HAL_GPIO_PIN_INTDIR_HI2LO;
        break;

    case HAL_GPIO_TRIG_BOTH:
        cfg.eIntDir = AM_HAL_GPIO_PIN_INTDIR_BOTH;
        break;
        
    default:
        /* Unsupported */
        return -1;
    }
    
    am_hal_gpio_interrupt_register_adv(pin, handler, arg);
    am_hal_gpio_pinconfig(pin, cfg);
    AM_HAL_GPIO_MASKCREATE(GpioIntMask);
    am_hal_gpio_interrupt_clear(AM_HAL_GPIO_MASKBIT(pGpioIntMask, pin));
    am_hal_gpio_interrupt_enable(AM_HAL_GPIO_MASKBIT(pGpioIntMask, pin));

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
    am_hal_gpio_interrupt_enable(AM_HAL_GPIO_BIT(pin));
}

void
hal_gpio_irq_disable(int pin)
{
    am_hal_gpio_interrupt_disable(AM_HAL_GPIO_BIT(pin));
}
