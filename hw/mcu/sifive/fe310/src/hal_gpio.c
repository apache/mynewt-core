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

#include <stdlib.h>
#include <hal/hal_gpio.h>
#include <env/freedom-e300-hifive1/platform.h>
#include <bsp/bsp.h>
#include <mcu/plic.h>

/* GPIO interrupts */
#define HAL_GPIO_MAX_IRQ        24

/* Storage for GPIO callbacks. */
struct hal_gpio_irq {
    hal_gpio_irq_handler_t func;
    void *arg;
    hal_gpio_irq_trig_t trig;
};

static struct hal_gpio_irq hal_gpio_irqs[HAL_GPIO_MAX_IRQ];

/**
 * gpio init in
 *
 * Initializes the specified pin as an input
 *
 * @param pin   Pin number to set as input
 * @param pull  pull type
 *
 * @return int  0: no error; -1 otherwise.
 */
int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    uint32_t mask = 1 << pin;

    GPIO_REG(GPIO_INPUT_EN) |= mask;
    GPIO_REG(GPIO_OUTPUT_EN) &= ~mask;
    GPIO_REG(GPIO_IOF_EN) &= ~mask;
    GPIO_REG(GPIO_IOF_SEL) &= ~mask;

    switch (pull) {
    case HAL_GPIO_PULL_UP:
        GPIO_REG(GPIO_PULLUP_EN) |= mask;
        break;
    case HAL_GPIO_PULL_DOWN:
        /* Pull down not supported */
    case HAL_GPIO_PULL_NONE:
    default:
        GPIO_REG(GPIO_PULLUP_EN) &= mask;
        break;
    }

    return 0;
}

/**
 * gpio init out
 *
 * Initialize the specified pin as an output, setting the pin to the specified
 * value.
 *
 * @param pin Pin number to set as output
 * @param val Value to set pin
 *
 * @return int  0: no error; -1 otherwise.
 */
int
hal_gpio_init_out(int pin, int val)
{
    uint32_t mask = 1 << pin;

    GPIO_REG(GPIO_OUTPUT_EN) |= mask;
    GPIO_REG(GPIO_INPUT_EN) &= ~mask;
    if (val) {
        GPIO_REG(GPIO_OUTPUT_VAL) |= mask;
    } else {
        GPIO_REG(GPIO_OUTPUT_VAL) &= ~mask;
    }
    return 0;
}

/**
 * gpio write
 *
 * Write a value (either high or low) to the specified pin.
 *
 * @param pin Pin to set
 * @param val Value to set pin (0:low 1:high)
 */
void
hal_gpio_write(int pin, int val)
{
    uint32_t mask = 1 << pin;

    if (val) {
        GPIO_REG(GPIO_OUTPUT_VAL) |= mask;
    } else {
        GPIO_REG(GPIO_OUTPUT_VAL) &= ~mask;
    }
}

/**
 * gpio read
 *
 * Reads the specified pin.
 *
 * @param pin Pin number to read
 *
 * @return int 0: low, 1: high
 */
int
hal_gpio_read(int pin)
{
    return (GPIO_REG(GPIO_INPUT_VAL) & (1 << pin)) ? 1 : 0;
}

/**
 * gpio toggle
 *
 * Toggles the specified pin
 *
 * @param pin Pin number to toggle
 *
 * @return current pin state int 0: low, 1: high
 */
int
hal_gpio_toggle(int pin)
{
    uint32_t mask = 1 << pin;
    uint32_t val = GPIO_REG(GPIO_OUTPUT_VAL) ^ mask;
    GPIO_REG(GPIO_OUTPUT_VAL) = val;
    return !!(val & mask);
}

static void
fe310_gpio_irq_handler(int num)
{
    struct hal_gpio_irq *irq = hal_gpio_irqs + num - INT_GPIO_BASE;
    uint32_t gpio_bit_mask = 1 << (num - INT_GPIO_BASE);

    /* Turn off this pin pending interrupts one at a time */
    if (GPIO_REG(GPIO_RISE_IE) & GPIO_REG(GPIO_RISE_IP) & gpio_bit_mask) {
        GPIO_REG(GPIO_RISE_IP) = gpio_bit_mask;
    } else if (GPIO_REG(GPIO_FALL_IE) & GPIO_REG(GPIO_FALL_IP) & gpio_bit_mask) {
        GPIO_REG(GPIO_FALL_IP) = gpio_bit_mask;
    } else if (GPIO_REG(GPIO_HIGH_IE) & GPIO_REG(GPIO_HIGH_IP) & gpio_bit_mask) {
        GPIO_REG(GPIO_HIGH_IP) = gpio_bit_mask;
    } else if (GPIO_REG(GPIO_LOW_IE) & GPIO_REG(GPIO_LOW_IP) & gpio_bit_mask) {
        GPIO_REG(GPIO_LOW_IP) = gpio_bit_mask;
    }
    irq->func(irq->arg);
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
    uint32_t mask = 1 << pin;

    plic_set_handler(INT_GPIO_BASE + pin, fe310_gpio_irq_handler, 3);
    hal_gpio_init_in(pin, pull);

    /* Disable interrupt till trigger is checked and enabled */
    GPIO_REG(GPIO_RISE_IE) &= ~mask;
    GPIO_REG(GPIO_FALL_IE) &= ~mask;
    GPIO_REG(GPIO_LOW_IE)  &= ~mask;
    GPIO_REG(GPIO_HIGH_IE) &= ~mask;

    switch (trig) {
    case HAL_GPIO_TRIG_RISING:
    case HAL_GPIO_TRIG_FALLING:
    case HAL_GPIO_TRIG_BOTH:
    case HAL_GPIO_TRIG_LOW:
    case HAL_GPIO_TRIG_HIGH:
        break;
    default:
        return -1;
    }

    hal_gpio_irqs[pin].func = handler;
    hal_gpio_irqs[pin].arg = arg;
    hal_gpio_irqs[pin].trig = trig;

    return 0;
}

/**
 * gpio irq release
 *
 * No longer interrupt when something occurs on the pin. NOTE: this function
 * does not change the GPIO push/pull setting.
 *
 * @param pin
 */
void
hal_gpio_irq_release(int pin)
{
    hal_gpio_irq_disable(pin);

    hal_gpio_irqs[pin].func = NULL;
    hal_gpio_irqs[pin].arg = NULL;
}

/**
 * gpio irq enable
 *
 * Enable the irq on the specified pin
 *
 * @param pin
 */
void
hal_gpio_irq_enable(int pin)
{
    uint32_t mask = 1 << pin;

    switch (hal_gpio_irqs[pin].trig) {
    case HAL_GPIO_TRIG_RISING:
        GPIO_REG(GPIO_RISE_IE) |=  mask;
        break;
    case HAL_GPIO_TRIG_FALLING:
        GPIO_REG(GPIO_FALL_IE) |=  mask;
        break;
    case HAL_GPIO_TRIG_BOTH:
        GPIO_REG(GPIO_RISE_IE) |=  mask;
        GPIO_REG(GPIO_FALL_IE) |=  mask;
        break;
    case HAL_GPIO_TRIG_LOW:
        GPIO_REG(GPIO_LOW_IE)  |=  mask;
        break;
    default:
        break;
    }
    plic_enable_interrupt(INT_GPIO_BASE + pin);
}

/**
 * gpio irq disable
 *
 *
 * @param pin
 */
void
hal_gpio_irq_disable(int pin)
{
    uint32_t mask = 1 << pin;

    plic_disable_interrupt(INT_GPIO_BASE + pin);

    GPIO_REG(GPIO_RISE_IE) &= ~mask;
    GPIO_REG(GPIO_FALL_IE) &= ~mask;
    GPIO_REG(GPIO_LOW_IE)  &= ~mask;
    GPIO_REG(GPIO_HIGH_IE) &= ~mask;
}
