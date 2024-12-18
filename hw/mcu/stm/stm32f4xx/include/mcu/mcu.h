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

#if defined(STM32F401xC) || defined(STM32F401xE) || \
    defined(STM32F405xx) || \
    defined(STM32F407xx) || \
    defined(STM32F410Cx) || defined(STM32F410Rx) || defined(STM32F410Tx) || \
    defined(STM32F411xE) || \
    defined(STM32F412Cx) || defined(STM32F412Rx) || defined(STM32F412Vx) || defined(STM32F412Zx) || \
    defined(STM32F415xx) || \
    defined(STM32F417xx) || \
    defined(STM32F427xx) || \
    defined(STM32F429xx) || \
    defined(STM32F437xx) || \
    defined(STM32F439xx) || \
    defined(STM32F446xx) || \
    defined(STM32F469xx) || \
    defined(STM32F479xx)
#define STM32_SYSTEM_MEMORY     0x1FFF0000
#elif defined(STM32F413xx) || \
    defined(STM32F423xx)
#define STM32_SYSTEM_MEMORY     0x1FF00000
#endif

/* FSMC pins are dedicated */
#define STM32_FSMC_NE1  MCU_GPIO_PORTD(7)
#define STM32_FSMC_NE2  MCU_GPIO_PORTG(9)
#define STM32_FSMC_NE3  MCU_GPIO_PORTG(10)
#define STM32_FSMC_NE4  MCU_GPIO_PORTG(12)
#define STM32_FSMC_NOE  MCU_GPIO_PORTD(4)
#define STM32_FSMC_NWE  MCU_GPIO_PORTD(5)
#define STM32_FSMC_D0   MCU_GPIO_PORTD(14)
#define STM32_FSMC_D1   MCU_GPIO_PORTD(15)
#define STM32_FSMC_D2   MCU_GPIO_PORTD(0)
#define STM32_FSMC_D3   MCU_GPIO_PORTD(1)
#define STM32_FSMC_D4   MCU_GPIO_PORTE(7)
#define STM32_FSMC_D5   MCU_GPIO_PORTE(8)
#define STM32_FSMC_D6   MCU_GPIO_PORTE(9)
#define STM32_FSMC_D7   MCU_GPIO_PORTE(10)
#define STM32_FSMC_D8   MCU_GPIO_PORTE(11)
#define STM32_FSMC_D9   MCU_GPIO_PORTE(12)
#define STM32_FSMC_D10  MCU_GPIO_PORTE(13)
#define STM32_FSMC_D11  MCU_GPIO_PORTE(14)
#define STM32_FSMC_D12  MCU_GPIO_PORTE(15)
#define STM32_FSMC_D13  MCU_GPIO_PORTD(8)
#define STM32_FSMC_D14  MCU_GPIO_PORTD(9)
#define STM32_FSMC_D15  MCU_GPIO_PORTD(10)

#ifdef __cplusplus
}
#endif

#endif /* __MCU_MCU_H_ */
