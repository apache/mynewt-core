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

#ifdef __cplusplus
extern "C" {
#endif

#define SVC_IRQ_NUMBER SVC_IRQn

#if defined(STM32L010x4) || defined(STM32L010x6) || defined(STM32L010x8) || defined(STM32L010xB) || \
    defined(STM32L011xx) || \
    defined(STM32L021xx) || \
    defined(STM32L031xx) || \
    defined(STM32L041xx) || \
    defined(STM32L051xx) || defined(STM32L052xx) || defined(STM32L053xx) || \
    defined(STM32L061xx) || defined(STM32L062xx) || defined(STM32L063xx) || \
    defined(STM32L071xx) || defined(STM32L072xx) || defined(STM32L073xx) || \
    defined(STM32L081xx) || defined(STM32L082xx) || defined(STM32L083xx)
#define STM32_SYSTEM_MEMORY     0x1FF00000
#endif

/*
 * Defines for naming GPIOs.
 */
#define MCU_GPIO_PORTA(pin)	((0 * 16) + (pin))
#define MCU_GPIO_PORTB(pin)	((1 * 16) + (pin))
#define MCU_GPIO_PORTC(pin)	((2 * 16) + (pin))
#define MCU_GPIO_PORTD(pin)	((3 * 16) + (pin))
#define MCU_GPIO_PORTE(pin)	((4 * 16) + (pin))
#define MCU_GPIO_PORTF(pin)	((5 * 16) + (pin))
#define MCU_GPIO_PORTG(pin)	((6 * 16) + (pin))
#define MCU_GPIO_PORTH(pin)	((7 * 16) + (pin))
#define MCU_GPIO_PORTI(pin)	((8 * 16) + (pin))

#ifdef __cplusplus
}
#endif

#endif /* __MCU_MCU_H_ */
