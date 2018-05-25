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

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"

#include "stm32f4xx_mynewt_hal.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_dma.h"

/* hal_watchdog */
#include "stm32f4xx_hal_iwdg.h"
#define STM32_HAL_WATCHDOG_CUSTOM_INIT(x)

/* hal_system_start */
#define STM32_HAL_FLASH_REMAP()                  \
    do {                                         \
        SYSCFG->MEMRMP = 0;                      \
        __DSB();                                 \
    } while (0)

/* hal_spi */
#include "stm32f4xx_hal_spi.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_gpio_ex.h"
#include "stm32f4xx_hal_rcc.h"

struct stm32_hal_spi_cfg {
    int ss_pin;                     /* for slave mode */
    int sck_pin;
    int miso_pin;
    int mosi_pin;
    int irq_prio;
};

/* hal_i2c */
#include "stm32f4xx_hal_i2c.h"
#include "mcu/stm32f4xx_mynewt_hal.h"

/* hal_uart */
#include "stm32f4xx_hal_uart.h"
#include "mcu/stm32f4_bsp.h"

/* hal_timer */
#include "stm32f4xx_hal_tim.h"

#define STM32_HAL_TIMER_MAX     (3)

/* hal_trng */
#include "stm32f4xx_hal_rng.h"

#ifdef __cplusplus
}
#endif

#endif /* STM32_HAL_H */
