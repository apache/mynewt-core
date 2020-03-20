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

#define SVC_IRQ_NUMBER SVCall_IRQn

#if defined(STM32F100xB) || defined(STM32F100xE) || \
    defined(STM32F101x6) || defined(STM32F101xB) || defined(STM32F101xE) || \
    defined(STM32F102x6) || defined(STM32F102xB) || \
    defined(STM32F103x6) || defined(STM32F103xB) || defined(STM32F103xE)
#define STM32_SYSTEM_MEMORY     0x1FFFF000
#elif defined(STM32F105xC) || \
    defined(STM32F107xC)
#define STM32_SYSTEM_MEMORY     0x1FFFB000
#elif defined(STM32F101xG) || \
    defined(STM32F103xG)
#define STM32_SYSTEM_MEMORY     0x1FFFE000
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MCU_MCU_H_ */
