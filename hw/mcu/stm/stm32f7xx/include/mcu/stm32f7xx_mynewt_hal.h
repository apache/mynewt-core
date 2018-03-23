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

#ifndef __MCU_STM32F7_MYNEWT_HAL_H
#define __MCU_STM32F7_MYNEWT_HAL_H

#include "stm32f7xx.h"
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_gpio.h"
#include "stm32f7xx_hal_i2c.h"
#include "stm32f7xx_hal_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Helper functions to enable/disable interrupts. */
#define __HAL_DISABLE_INTERRUPTS(x)                     \
    do {                                                \
        x = __get_PRIMASK();                            \
        __disable_irq();                                \
    } while(0);

#define __HAL_ENABLE_INTERRUPTS(x)                      \
    do {                                                \
        if (!x) {                                       \
            __enable_irq();                             \
        }                                               \
    } while(0);


int hal_gpio_init_stm(int pin, GPIO_InitTypeDef *cfg);
int hal_gpio_deinit_stm(int pin, GPIO_InitTypeDef *cfg);

struct stm32_hal_i2c_cfg {
    I2C_TypeDef *hic_i2c;
    volatile uint32_t *hic_rcc_reg;      /* RCC register to modify */
    uint32_t hic_rcc_dev;                /* RCC device ID */
    uint8_t hic_pin_sda;
    uint8_t hic_pin_scl;
    uint8_t hic_pin_af;
    uint8_t hic_10bit;
    uint32_t hic_timingr;               /* TIMINGR register */
};

#ifdef __cplusplus
}
#endif

#endif /* __MCU_STM32F4_MYNEWT_HAL_H */
