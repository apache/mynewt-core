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

#ifndef __MCU_MCU_H_
#define __MCU_MCU_H_

#include <stm32_common/mcu.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SVC_IRQ_NUMBER SVC_IRQn

#if defined(STM32L412xx) || \
    defined(STM32L422xx) || \
    defined(STM32L431xx) || defined(STM32L432xx) || defined(STM32L433xx) || \
    defined(STM32L442xx) || defined(STM32L443xx) || \
    defined(STM32L451xx) || defined(STM32L452xx) || \
    defined(STM32L462xx) || \
    defined(STM32L471xx) || defined(STM32L475xx) || defined(STM32L476xx) || \
    defined(STM32L485xx) || defined(STM32L486xx) || \
    defined(STM32L496xx) || \
    defined(STM32L4A6xx) || \
    defined(STM32L4R5xx) || defined(STM32L4R7xx) || defined(STM32L4R9xx) || \
    defined(STM32L4S5xx) || defined(STM32L4S7xx) || defined(STM32L4S9xx)
#define STM32_SYSTEM_MEMORY     0x1FFF0000
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MCU_MCU_H_ */
