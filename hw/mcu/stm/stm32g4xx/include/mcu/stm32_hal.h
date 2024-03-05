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

#ifndef STM32_HAL_H
#define STM32_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mcu/cortex_m4.h>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx.h"
#include "stm32g4xx_hal_dma.h"
#include "stm32g4xx_hal_spi.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_gpio_ex.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_hal_iwdg.h"
#include "stm32g4xx_hal_i2c.h"
#include "stm32g4xx_hal_uart.h"
#include "mcu/stm32g4_bsp.h"
#include "stm32g4xx_hal_tim.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_tim.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx_hal_flash.h"
#include "stm32g4xx_hal_flash_ex.h"
#include "stm32g4xx_mynewt_hal.h"

#define STM32_HAL_WATCHDOG_CUSTOM_INIT(x)           \
    do {                                            \
        (x)->Init.Window = IWDG_WINDOW_DISABLE;     \
    } while (0)

#define STM32_HAL_FLASH_REMAP()

struct stm32_hal_spi_cfg {
    int ss_pin;                     /* for slave mode */
    int sck_pin;
    int miso_pin;
    int mosi_pin;
    int irq_prio;
};

#define STM32_HAL_TIMER_MAX     (3)

#define STM32_HAL_TIMER_TIM1_IRQ    TIM1_UP_TIM16_IRQn
#define STM32_HAL_TIMER_TIM6_IRQ    TIM6_DAC_IRQn
#define STM32_HAL_TIMER_TIM8_IRQ    TIM8_UP_IRQn

#define STM32_HAL_FLASH_INIT()        \
    do {                              \
        HAL_FLASH_Unlock();           \
    } while (0)
#define FLASH_PROGRAM_TYPE FLASH_TYPEPROGRAM_DOUBLEWORD
#define STM32_HAL_FLASH_CLEAR_ERRORS()

#ifdef __cplusplus
}
#endif

#endif /* STM32_HAL_H */
