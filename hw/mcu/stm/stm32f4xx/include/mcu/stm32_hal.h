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

/* hal_watchdog */
#include "stm32f4xx_hal_iwdg.h"
#define STM32_HAL_WATCHDOG_CUSTOM_INIT(x)

/* hal_system_start */
#define STM32_HAL_FLASH_REMAP()                  \
    do {                                         \
        SYSCFG->MEMRMP = 0;                      \
        __DSB();                                 \
    } while (0)

/* stm32_hw_id
 *
 * STM32F4 has a unique 96-bit id at address 0x1FFF7A10.
 * See ref manual chapter 39.1.
 */
#define STM32_HW_ID_ADDR 0x1FFF7A10

#ifdef __cplusplus
}
#endif

#endif /* STM32_HAL_H */
