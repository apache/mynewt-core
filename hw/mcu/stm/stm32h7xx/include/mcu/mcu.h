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

/*
 * System memory starts at 0x1FF00000, bootloader start address (vector table)
 * is at offset 0xA000 for STM32H7A3/7B3/7B0 and 0x9800 for other devices.
 */
#if defined(STM32H7A3xx) || defined(STM32H7A3xxQ) || \
    defined(STM32H7B3xx) || defined(STM32H7B3xxQ) || \
    defined(STM32H7B0xx) || defined(STM32H7B0xxQ)
#define STM32_SYSTEM_MEMORY     0x1FF0A000
#else
#define STM32_SYSTEM_MEMORY     0x1FF09800
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MCU_MCU_H_ */
