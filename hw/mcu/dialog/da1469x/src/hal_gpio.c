
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

#include "syscfg/syscfg.h"
#include "mcu/da1469x_hal.h"
#include <mcu/mcu.h>
#include "mcu/cmsis_nvic.h"
#include "hal/hal_gpio.h"
#include "bsp/bsp.h"
#include "DA1469xAB.h"

/* GPIO interrupts */
#define HAL_GPIO_MAX_IRQ        (4)

#define GPIO_REG(name) ((__IO uint32_t *)(GPIO_BASE + offsetof(GPIO_Type, name)))
#define WAKEUP_REG(name) ((__IO uint32_t *)(WAKEUP_BASE + offsetof(WAKEUP_Type, name)))
#define CRG_TOP_REG(name) ((__IO uint32_t *)(CRG_TOP_BASE + offsetof(CRG_TOP_Type, name)))

#define GPIO_PORT(pin) (((unsigned)(pin)) >> 5)

#define GPIO_PIN_DATA_REG_ADDR(pin)        (GPIO_REG(P0_DATA_REG) + GPIO_PORT(pin))
#define GPIO_PIN_DATA_REG(pin)             *GPIO_PIN_DATA_REG_ADDR(pin)
#define GPIO_PIN_SET_DATA_REG_ADDR(pin)    (GPIO_REG(P0_SET_DATA_REG) + GPIO_PORT(pin))
#define GPIO_PIN_SET_DATA_REG(pin)         *GPIO_PIN_SET_DATA_REG_ADDR(pin)
#define GPIO_PIN_RESET_DATA_REG_ADDR(pin)  (GPIO_REG(P0_RESET_DATA_REG) + GPIO_PORT(pin))
#define GPIO_PIN_RESET_DATA_REG(pin)       *GPIO_PIN_RESET_DATA_REG_ADDR(pin)
#define GPIO_PIN_MODE_REG_ADDR(pin)        (GPIO_REG(P0_00_MODE_REG) + (pin))
#define GPIO_PIN_MODE_REG(pin)             *GPIO_PIN_MODE_REG_ADDR(pin)
#define GPIO_PIN_PADPWR_CTRL_REG_ADDR(pin) (GPIO_REG(P0_PADPWR_CTRL_REG) + GPIO_PORT(pin))
#define GPIO_PIN_PADPWR_CTRL_REG(pin)      *GPIO_PIN_PADPWR_CTRL_REG_ADDR(pin)
#define GPIO_PIN_UNLATCH_ADDR(pin)         (CRG_TOP_REG(P0_SET_PAD_LATCH_REG) + GPIO_PORT(pin) * 3)
#define GPIO_PIN_LATCH_ADDR(pin)           (CRG_TOP_REG(P0_RESET_PAD_LATCH_REG) + GPIO_PORT(pin) * 3)
#define GPIO_PIN_UNLATCH(pin)              do { *GPIO_PIN_UNLATCH_ADDR(pin) = 1 << ((pin) & 31); } while (0)
#define GPIO_PIN_LATCH(pin)                do { *GPIO_PIN_LATCH_ADDR(pin) = 1 << ((pin) & 31); } while (0)

#define WKUP_CTRL_REG_ADDR              (WAKEUP_REG(WKUP_CTRL_REG))
#define WKUP_RESET_IRQ_REG_ADDR         (WAKEUP_REG(WKUP_RESET_IRQ_REG))
#define WKUP_SELECT_PX_REG_ADDR(pin)    (WAKEUP_REG(WKUP_SELECT_P0_REG) + GPIO_PORT(pin))
#define WKUP_SELECT_PX_REG(pin)         *(WKUP_SELECT_PX_REG_ADDR(pin))
#define WKUP_POL_PX_REG_ADDR(pin)       (WAKEUP_REG(WKUP_POL_P0_REG) + GPIO_PORT(pin))
#define WKUP_POL_PX_SET_FALLING(pin)    do { *(WKUP_POL_PX_REG_ADDR(pin)) |= (1 << ((pin) & 31)); } while (0)
#define WKUP_POL_PX_SET_RISING(pin)     do { *(WKUP_POL_PX_REG_ADDR(pin)) &= ~(1 << ((pin) & 31)); } while (0)
#define WKUP_STAT_PX_REG_ADDR(pin)      (WAKEUP_REG(WKUP_STATUS_P0_REG) + GPIO_PORT(pin))
#define WKUP_STAT(pin)                  ((*(WKUP_STAT_PX_REG_ADDR(pin)) >> ((pin) & 31)) & 1)
#define WKUP_CLEAR_PX_REG_ADDR(pin)     (WAKEUP_REG(WKUP_CLEAR_P0_REG) + GPIO_PORT(pin))
#define WKUP_CLEAR_PX(pin)              do { (*(WKUP_CLEAR_PX_REG_ADDR(pin)) = (1 << ((pin) & 31))); } while (0)
#define WKUP_SEL_GPIO_PX_REG_ADDR(pin)  (WAKEUP_REG(WKUP_SEL_GPIO_P0_REG) + GPIO_PORT(pin))
#define WKUP_SEL_GPIO_PX_REG(pin)       *(WKUP_SEL_GPIO_PX_REG_ADDR(pin))

/* Storage for GPIO callbacks. */
struct hal_gpio_irq {
    int pin;
    hal_gpio_irq_handler_t func;
    void *arg;
};

static struct hal_gpio_irq hal_gpio_irqs[HAL_GPIO_MAX_IRQ];

int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    volatile uint32_t *px_xx_mod_reg = GPIO_PIN_MODE_REG_ADDR(pin);

    switch (pull) {
    case HAL_GPIO_PULL_UP:
        *px_xx_mod_reg = MCU_GPIO_FUNC_GPIO | MCU_GPIO_MODE_INPUT_PULLUP;
        break;
    case HAL_GPIO_PULL_DOWN:
        *px_xx_mod_reg = MCU_GPIO_FUNC_GPIO | MCU_GPIO_MODE_INPUT_PULLDOWN;
        break;
    case HAL_GPIO_PULL_NONE:
        *px_xx_mod_reg = MCU_GPIO_FUNC_GPIO | MCU_GPIO_MODE_INPUT;
        break;
    default:
        return -1;
    }
    GPIO_PIN_UNLATCH(pin);

    return 0;
}

int
hal_gpio_init_out(int pin, int val)
{
    GPIO_PIN_MODE_REG(pin) = MCU_GPIO_MODE_OUTPUT;

    if (val) {
        GPIO_PIN_SET_DATA_REG(pin) = (1 << (pin & 31));
    } else {
        GPIO_PIN_RESET_DATA_REG(pin) = (1 << (pin & 31));
    }
    GPIO_PIN_UNLATCH(pin);

    return 0;
}

void
hal_gpio_write(int pin, int val)
{
    if (val) {
        GPIO_PIN_SET_DATA_REG(pin) = 1 << (pin & 31);
    } else {
        GPIO_PIN_RESET_DATA_REG(pin) = 1 << (pin & 31);
    }
}

int
hal_gpio_read(int pin)
{
    return (GPIO_PIN_DATA_REG(pin) >> (pin & 31)) & 1;
}

int
hal_gpio_toggle(int pin)
{
    int new_value = hal_gpio_read(pin) == 0;

    hal_gpio_write(pin, new_value);

    return new_value;
}

static void
hal_gpio_irq_handler(void)
{
    struct hal_gpio_irq *irq;
    uint32_t stat;
    int i;

    *WKUP_RESET_IRQ_REG_ADDR = 1;
    NVIC_ClearPendingIRQ(KEY_WKUP_GPIO_IRQn);

    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        irq = &hal_gpio_irqs[i];

        /* Read latched status value from relevant GPIO port */
        stat = WKUP_STAT(irq->pin);

        if (irq->func && stat) {
            irq->func(irq->arg);
        }

        WKUP_CLEAR_PX(irq->pin);
    }
}

static void
hal_gpio_irq_setup(void)
{
    static uint8_t irq_setup;
    int sr;

    if (!irq_setup) {
        __HAL_DISABLE_INTERRUPTS(sr);

        irq_setup = 1;

        NVIC_ClearPendingIRQ(GPIO_P0_IRQn);
        NVIC_ClearPendingIRQ(GPIO_P1_IRQn);
        NVIC_SetVector(GPIO_P0_IRQn, (uint32_t)hal_gpio_irq_handler);
        NVIC_SetVector(GPIO_P1_IRQn, (uint32_t)hal_gpio_irq_handler);
        WAKEUP->WKUP_CTRL_REG = 0;
        WAKEUP->WKUP_CLEAR_P0_REG = 0xFFFFFFFF;
        WAKEUP->WKUP_CLEAR_P1_REG = 0x007FFFFF;
        WAKEUP->WKUP_SELECT_P0_REG = 0;
        WAKEUP->WKUP_SELECT_P1_REG = 0;
        WAKEUP->WKUP_SEL_GPIO_P0_REG = 0;
        WAKEUP->WKUP_SEL_GPIO_P1_REG = 0;
        WAKEUP->WKUP_RESET_IRQ_REG = 0;

        CRG_TOP->CLK_TMR_REG |= CRG_TOP_CLK_TMR_REG_WAKEUPCT_ENABLE_Msk;

        __HAL_ENABLE_INTERRUPTS(sr);
        NVIC_EnableIRQ(GPIO_P0_IRQn);
        NVIC_EnableIRQ(GPIO_P1_IRQn);
    }
}

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

int
hal_gpio_irq_init(int pin, hal_gpio_irq_handler_t handler, void *arg,
                  hal_gpio_irq_trig_t trig, hal_gpio_pull_t pull)
{
    int i;

    hal_gpio_irq_setup();

    i = hal_gpio_find_empty_slot();
    if (i < 0) {
        return -1;
    }

    hal_gpio_init_in(pin, pull);

    switch (trig) {
    case HAL_GPIO_TRIG_RISING:
        WKUP_POL_PX_SET_RISING(pin);
        break;
    case HAL_GPIO_TRIG_FALLING:
        WKUP_POL_PX_SET_FALLING(pin);
        break;
    case HAL_GPIO_TRIG_BOTH:
        /* Not supported */
    default:
        return -1;
    }

    hal_gpio_irqs[i].pin = pin;
    hal_gpio_irqs[i].func = handler;
    hal_gpio_irqs[i].arg = arg;

    return 0;
}

void
hal_gpio_irq_release(int pin)
{
    int i;

    hal_gpio_irq_disable(pin);

    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].pin == pin && hal_gpio_irqs[i].func) {
            hal_gpio_irqs[i].pin = -1;
            hal_gpio_irqs[i].arg = NULL;
            hal_gpio_irqs[i].func = NULL;
        }
    }
}

void
hal_gpio_irq_enable(int pin)
{
    WKUP_SEL_GPIO_PX_REG(pin) |= (1 << (pin & 31));
}

void
hal_gpio_irq_disable(int pin)
{
    WKUP_SEL_GPIO_PX_REG(pin) &= ~(1 << (pin & 31));
    WKUP_CLEAR_PX(pin);
}

void
mcu_gpio_set_pin_function(int pin, int mode, mcu_gpio_func func)
{
    GPIO_PIN_MODE_REG(pin) = (func & GPIO_P0_00_MODE_REG_PID_Msk) |
        (mode & (GPIO_P0_00_MODE_REG_PUPD_Msk | GPIO_P0_00_MODE_REG_PPOD_Msk));
    GPIO_PIN_UNLATCH(pin);
}
