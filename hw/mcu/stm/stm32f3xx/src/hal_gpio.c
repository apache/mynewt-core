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

/*
 * Currently no support for
 * - interrupts
 * - pin output speed
 */

#include "hal_gpio_stm32.h"
#include "hal/hal_gpio.h"
#include "bsp/cmsis_nvic.h"
#include "stm32f3xx.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"
#include "mcu/stm32f3xx_mynewt_hal.h"
#include <assert.h>

/*
 * GPIO pin mapping
 *
 * The stm32F3xx processors have 16 gpio pins per port. We map the logical pin
 * numbers (from 0 to N) in the following manner:
 *      Port A: PA0-PA15 map to pins 0 - 15.
 *      Port B: PB0-PB15 map to pins 16 - 31.
 *      Port C: PC0-PC15 map to pins 32 - 47.
 *
 *      To convert a gpio to pin number, do the following:
 *          - Convert port label to its numeric value (A=0, B=1, C=2, etc).
 *          - Multiply by 16.
 *          - Add port pin number.
 *
 *      Ex: PD11 = (3 * 16) + 11 = 59.
 *          PA0 = (0 * 16) + 0 = 0
 */
#define HAL_GPIO_PIN_NUM(pin)   ((pin) & 0x0F)
#define HAL_GPIO_PORT(pin)      (((pin) >> 4) & 0x0F)
#define HAL_GPIO_PIN(pin)       (1 << HAL_GPIO_PIN_NUM(pin))

/* Port index to port address map */
static GPIO_TypeDef * const portmap[HAL_GPIO_PORT_COUNT] =
{
    HAL_GPIO_PORT_LIST
};

/**
 * hal gpio clk enable
 *
 * Enable the port peripheral clock
 *
 * @param port_idx
 */
static void
hal_gpio_clk_enable(uint32_t port_idx)
{
    switch (port_idx) {
        case 0:
            if (!__HAL_RCC_GPIOA_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOA_CLK_ENABLE();
            }
            break;
#if HAL_GPIO_PORT_COUNT > 1
        case 1:
            if (!__HAL_RCC_GPIOB_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOB_CLK_ENABLE();
            }
            break;
#if HAL_GPIO_PORT_COUNT > 2
        case 2:
            if (!__HAL_RCC_GPIOC_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOC_CLK_ENABLE();
            }
            break;
#if HAL_GPIO_PORT_COUNT > 3
        case 3:
            if (!__HAL_RCC_GPIOD_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOD_CLK_ENABLE();
            }
            break;
#if HAL_GPIO_PORT_COUNT > 4 && defined GPIOE_BASE
        case 4:
            if (!__HAL_RCC_GPIOE_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOE_CLK_ENABLE();
            }
            break;
#if HAL_GPIO_PORT_COUNT > 5
        case 5:
            if (!__HAL_RCC_GPIOF_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOF_CLK_ENABLE();
            }
            break;
#if HAL_GPIO_PORT_COUNT > 6
        case 6:
            if (!__HAL_RCC_GPIOG_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOG_CLK_ENABLE();
            }
            break;
#if HAL_GPIO_PORT_COUNT > 7
        case 7:
            if (!__HAL_RCC_GPIOH_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOH_CLK_ENABLE();
            }
            break;
#if HAL_GPIO_PORT_COUNT > 8
        case 8:
            if (!__HAL_RCC_GPIOI_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOI_CLK_ENABLE();
            }
            break;
#if HAL_GPIO_PORT_COUNT > 9
        case 9:
            if (!__HAL_RCC_GPIOJ_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOJ_CLK_ENABLE();
            }
            break;
#if HAL_GPIO_PORT_COUNT > 10
        case 10:
            if (!__HAL_RCC_GPIOK_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOK_CLK_ENABLE();
            }
            break;
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
        default:
            assert(0);
            break;
    }
}


/**
 * hal gpio init
 *
 * Called to initialize a gpio.
 *
 * @param pin
 * @param cfg
 *
 * @return int
 */
int
hal_gpio_init_stm(int pin, GPIO_InitTypeDef *cfg)
{
    int port;
    uint32_t mcu_gpio_pin;

    /* Is this a valid pin? */
    port = HAL_GPIO_PORT(pin);
    if (port >= HAL_GPIO_PORT_COUNT) {
        return -1;
    }

    mcu_gpio_pin = HAL_GPIO_PIN(pin);
    cfg->Pin = mcu_gpio_pin;

    /* Enable the GPIO clock */
    hal_gpio_clk_enable(port);

    /* Initialize pin as an input, setting proper mode */
    HAL_GPIO_Init(portmap[port], cfg);

    return 0;
}

/**
 * hal gpio deinit
 *
 * Called to deinitialize a gpio.
 *
 * @param pin
 * @param cfg
 *
 * @return int
 */
int
hal_gpio_deinit_stm(int pin, GPIO_InitTypeDef *cfg)
{
    int port;
    uint32_t mcu_gpio_pin;

    /* Is this a valid pin? */
    port = HAL_GPIO_PORT(pin);
    if (port >= HAL_GPIO_PORT_COUNT) {
        return -1;
    }

    mcu_gpio_pin = HAL_GPIO_PIN(pin);
    cfg->Pin = mcu_gpio_pin;

    /* Initialize pin as an input, setting proper mode */
    HAL_GPIO_DeInit(portmap[port], cfg->Pin);

    return 0;
}

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
    int rc;
    GPIO_InitTypeDef init_cfg;

    init_cfg.Mode = GPIO_MODE_INPUT;
    init_cfg.Pull = pull;

    rc = hal_gpio_init_stm(pin, &init_cfg);
    return rc;
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
int hal_gpio_init_out(int pin, int val)
{
    int rc;
    GPIO_InitTypeDef cfg;

    cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    cfg.Pull  = GPIO_NOPULL;
    cfg.Speed = GPIO_SPEED_FREQ_HIGH;

    rc = hal_gpio_init_stm(pin, &cfg);
    if (HAL_OK == rc) {
        hal_gpio_write(pin, val);
    }

    return rc;
}

/**
 * gpio enumeration translation
 *
 * Convert hal pull types to mcu pull type
 *
 * @param pull
 *
 * @return uint32_t
 */
static uint32_t
hal_gpio_pull_to_stm_pull(enum hal_gpio_pull pull)
{
    switch (pull) {
        case HAL_GPIO_PULL_NONE:  return GPIO_NOPULL;
        case HAL_GPIO_PULL_UP:    return GPIO_PULLUP;
        case HAL_GPIO_PULL_DOWN:  return GPIO_PULLDOWN;
    }
    /* never reached */
    return GPIO_NOPULL;
}

/**
 * gpio init af
 *
 * Configure the specified pin for AF.
 */
int
hal_gpio_init_af(int pin, uint8_t af_type, enum hal_gpio_pull pull, uint8_t od)
{
    GPIO_InitTypeDef cfg;

    cfg.Mode      = od ? GPIO_MODE_AF_OD : GPIO_MODE_AF_PP;
    cfg.Speed     = GPIO_SPEED_FREQ_HIGH;
    cfg.Pull      = hal_gpio_pull_to_stm_pull(pull);
    cfg.Alternate = af_type;

    return hal_gpio_init_stm(pin, &cfg);
}

/**
 * gpio write
 *
 * Write a value (either high or low) to the specified pin.
 *
 * @param pin Pin to set
 * @param val Value to set pin (0:low 1:high)
 */
void hal_gpio_write(int pin, int val)
{
    int port;
    uint32_t mcu_gpio_pin;
    GPIO_PinState state;

    port         = HAL_GPIO_PORT(pin);
    mcu_gpio_pin = HAL_GPIO_PIN(pin);
    state        = val ? GPIO_PIN_SET : GPIO_PIN_RESET;

    HAL_GPIO_WritePin(portmap[port], mcu_gpio_pin, state);
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
int hal_gpio_read(int pin)
{
    int port;
    uint32_t mcu_gpio_pin;

    port = HAL_GPIO_PORT(pin);
    mcu_gpio_pin = HAL_GPIO_PIN(pin);
    return HAL_GPIO_ReadPin(portmap[port], mcu_gpio_pin);
}

/**
 * gpio toggle
 *
 * Toggles the specified pin
 *
 * @param pin Pin number to toggle
 *
 * @return current pin state int 0: low 1 : high
 */
int hal_gpio_toggle(int pin)
{
    int pin_state = (hal_gpio_read(pin) != 1);
    hal_gpio_write(pin, pin_state);
    return pin_state;
}

/**
 * gpio irq init
 *
 * Initialize an external interrupt on a gpio pin
 *
 * irq support not implemented yet.
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
    (void)pin;
    (void)handler;
    (void)arg;
    (void)trig;
    (void)pull;

    return -1;
}

/**
 * gpio irq release
 *
 * irq support not implemented yet.
 *
 * @param pin
 */
void
hal_gpio_irq_release(int pin)
{
    (void)pin;
}

/**
 * gpio irq enable
 *
 * irq support not implemented yet.
 *
 * @param pin
 */
void
hal_gpio_irq_enable(int pin)
{
    (void)pin;
}

/**
 * gpio irq disable
 *
 * irq support not implemented yet.
 *
 * @param pin
 */
void
hal_gpio_irq_disable(int pin)
{
    (void)pin;
}
