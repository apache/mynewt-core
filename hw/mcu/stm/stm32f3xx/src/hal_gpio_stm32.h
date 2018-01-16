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

#ifndef H_HAL_GPIO_STM32_
#define H_HAL_GPIO_STM32_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f3xx.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_gpio_ex.h"

#if defined GPIOL
#   error "GPIOL defined - MCU not supported!"
#endif

#define HAL_GPIO_PORT_COUNT_MAX (11)

#if defined GPIOK_BASE
#   define  HAL_GPIO_PORT_COUNT   (11)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF,GPIOG,GPIOH,GPIOI,GPIOJ,GPIOK
#elif defined GPIOJ_BASE
#   define  HAL_GPIO_PORT_COUNT   (10)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF,GPIOG,GPIOH,GPIOI,GPIOJ
#elif defined GPIOI_BASE
#   define  HAL_GPIO_PORT_COUNT   (9)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF,GPIOG,GPIOH,GPIOI
#elif defined GPIOH_BASE
#   define  HAL_GPIO_PORT_COUNT   (8)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF,GPIOG,GPIOH
#elif defined GPIOG_BASE
#   define  HAL_GPIO_PORT_COUNT   (7)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF,GPIOG
#elif defined GPIOF_BASE
#   define  HAL_GPIO_PORT_COUNT   (6)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF
#   ifndef GPIOE_BASE /* imagine a processor that has GPIOE but no GPIOF ... */
#       define GPIOE 0
#   endif
#elif defined GPIOE_BASE
#   define  HAL_GPIO_PORT_COUNT   (5)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE
#elif defined GPIOD_BASE
#   define  HAL_GPIO_PORT_COUNT   (4)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD
#elif defined GPIOC_BASE
#   define  HAL_GPIO_PORT_COUNT   (3)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC
#elif defined GPIOB_BASE
#   define  HAL_GPIO_PORT_COUNT   (2)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB
#elif defined GPIOA_BASE
#   define  HAL_GPIO_PORT_COUNT   (1)
#   define  HAL_GPIO_PORT_LIST    GPIOA
#else
#   error "No GPIO ports found - MCU not supported!"
#endif

#ifdef __cplusplus
}
#endif

#endif /* H_HAL_GPIO_STM32_ */
