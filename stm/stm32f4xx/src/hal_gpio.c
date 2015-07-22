/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hal/hal_gpio.h"
#include "stm32f4xx/stm32f4xx.h"
#include "stm32f4xx/stm32f4xx_hal_gpio.h"
#include "stm32f4xx/stm32f4xx_hal_rcc.h"
#include <assert.h>

/* 
 * GPIO pin mapping
 *
 * The stm32F4xx processors have 16 gpio pins per port. We map the logical pin
 * numbers (from 0 to N) in the following manner:
 *      Port A: PA0-PA15 map to pins 0 - 15.
 *      Port B: PB0-PB15 map to pins 16 - 31.
 *      Port C: PBC0-PC15 map to pins 32 - 47.
 * 
 *      To convert a gpio to pin number, do the following:
 *          - Convert port label to its numeric value (A=0, B=1, C=2, etc).
 *          - Multiply by 16.
 *          - Add port pin number.
 * 
 *      Ex: PD11 = (4 * 16) + 11 = 75.
 *          PA0 = (0 * 16) + 0 = 0
 */ 

/* Define the number of ports on processor */
#if defined GPIOK_BASE
#define HAL_GPIO_NUM_PORTS  (11)
#elif defined GPIOJ_BASE
#define HAL_GPIO_NUM_PORTS  (10)
#elif defined GPIOI_BASE
#define HAL_GPIO_NUM_PORTS  (9)
#elif defined GPIOH_BASE
#define HAL_GPIO_NUM_PORTS  (8)
#elif defined GPIOG_BASE
#define HAL_GPIO_NUM_PORTS  (7)
#elif defined GPIOF_BASE
#define HAL_GPIO_NUM_PORTS  (6)
#else
#define HAL_GPIO_NUM_PORTS  (5)
#endif

/* Port index to port address map */
static GPIO_TypeDef * const portmap[HAL_GPIO_NUM_PORTS] =
{
    GPIOA,
    GPIOB,
    GPIOC,
    GPIOD,
    GPIOE,
#if defined GPIOF_BASE
    GPIOF,
#endif
#if defined GPIOG_BASE
    GPIOG,
#endif
#if defined GPIOH_BASE
    GPIOH,
#endif
#if defined GPIOI_BASE
    GPIOI
#endif
#if defined GPIOJ_BASE
    GPIOJ
#endif
#if defined GPIOK_BASE
    GPIOK
#endif
};

/* XXX: should we just assert here or return an error if the pin # is
   too high? */

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
            __HAL_RCC_GPIOA_CLK_ENABLE();
            break;
        case 1:
            __HAL_RCC_GPIOB_CLK_ENABLE();
            break;
        case 2:
            __HAL_RCC_GPIOC_CLK_ENABLE();
            break;
        case 3:
            __HAL_RCC_GPIOD_CLK_ENABLE();
            break;
        case 4:
            __HAL_RCC_GPIOE_CLK_ENABLE();
            break;
#if defined GPIOF_BASE
        case 5:
            __HAL_RCC_GPIOF_CLK_ENABLE();
            break;
#endif
#if defined GPIOG_BASE
        case 6:
            __HAL_RCC_GPIOG_CLK_ENABLE();
            break;
#endif
#if defined GPIOH_BASE
        case 7:
            __HAL_RCC_GPIOH_CLK_ENABLE();
            break;
#endif
#if defined GPIOI_BASE
        case 8:
            __HAL_RCC_GPIOI_CLK_ENABLE();
            break;
#endif
#if defined GPIOJ_BASE
        case 9:
            __HAL_RCC_GPIOJ_CLK_ENABLE();
            break;
#endif
#if defined GPIOK_BASE
        case 10:
            __HAL_RCC_GPIOK_CLK_ENABLE();
            break;
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
static int
hal_gpio_init(int pin, GPIO_InitTypeDef *cfg)
{
    int port;
    uint32_t mcu_pin;

    /* Is this a valid pin? */
    port = pin / 16;
    if (port >= HAL_GPIO_NUM_PORTS) {
        return -1;
    }

    mcu_pin = (1 << (pin - (port * 16)));
    cfg->Pin = mcu_pin;

    /* Enable the GPIO clockl */
    hal_gpio_clk_enable(port);

    /* Initialize pin as an input, setting proper mode */
    HAL_GPIO_Init(portmap[port], cfg);

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
gpio_init_in(int pin, gpio_pull_t pull)
{
    int rc;
    GPIO_InitTypeDef init_cfg;

    init_cfg.Mode = GPIO_MODE_INPUT;
    init_cfg.Pull = pull;

    rc = hal_gpio_init(pin, &init_cfg);
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
int gpio_init_out(int pin, int val)
{
    int rc;
    GPIO_InitTypeDef init_cfg;

    init_cfg.Mode = GPIO_MODE_OUTPUT_PP;
    init_cfg.Pull = GPIO_NOPULL;
    init_cfg.Speed = GPIO_SPEED_HIGH;
    init_cfg.Alternate = 0;

    rc = hal_gpio_init(pin, &init_cfg);
    return rc;
}

/**
 * gpio set 
 *  
 * Sets specified pin to 1 (high) 
 * 
 * @param pin 
 */
void gpio_set(int pin)
{
    int port;
    uint32_t mcu_pin;

    port = pin / 16;
    mcu_pin = (1 << (pin - (port * 16)));
    HAL_GPIO_WritePin(portmap[port], mcu_pin, GPIO_PIN_SET);
}

/**
 * gpio clear
 *  
 * Sets specified pin to 0 (low). 
 * 
 * @param pin 
 */
void gpio_clear(int pin)
{
    int port;
    uint32_t mcu_pin;

    port = pin / 16;
    mcu_pin = (1 << (pin - (port * 16)));
    HAL_GPIO_WritePin(portmap[port], mcu_pin, GPIO_PIN_RESET);
}

/**
 * gpio write 
 *  
 * Write a value (either high or low) to the specified pin.
 * 
 * @param pin Pin to set
 * @param val Value to set pin (0:low 1:high)
 */
void gpio_write(int pin, int val)
{
    if (val) {
        gpio_set(pin);
    } else {
        gpio_clear(pin);
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
int gpio_read(int pin)
{
    int port;
    uint32_t mcu_pin;

    port = pin / 16;
    mcu_pin = (1 << (pin - (port * 16)));
    return HAL_GPIO_ReadPin(portmap[port], mcu_pin);
}

/**
 * gpio toggle 
 *  
 * Toggles the specified pin
 * 
 * @param pin Pin number to toggle
 */
void gpio_toggle(int pin)
{
    if (gpio_read(pin)) {
        gpio_clear(pin);
    } else {
        gpio_set(pin);
    }
}

/* XXX: make sure turn on clocks, or that gpio init does this */

