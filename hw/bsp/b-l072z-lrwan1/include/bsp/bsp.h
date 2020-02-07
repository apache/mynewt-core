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
#ifndef H_BSP_H
#define H_BSP_H

#include <inttypes.h>
#include <mcu/mcu.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Define special stack sections */
#define sec_data_core   __attribute__((section(".data.core")))
#define sec_bss_core    __attribute__((section(".bss.core")))
#define sec_bss_nz_core __attribute__((section(".bss.core.nz")))

/* More convenient section placement macros. */
#define bssnz_t         sec_bss_nz_core

extern uint8_t _ram_start;

#define RAM_SIZE        (20 * 1024)

#define LED_BLINK_PIN   MCU_GPIO_PORTB(5)  /* LD1 - green */
#define LED_2           MCU_GPIO_PORTB(6)  /* LD3 - blue */
#define LED_3           MCU_GPIO_PORTB(7)  /* LD4 - red */
#define LED_4           MCU_GPIO_PORTA(5)  /* LD2 - green */

/* SX1276 pins */
#define SX1276_DIO0         MCU_GPIO_PORTB(4)
#define SX1276_DIO1         MCU_GPIO_PORTB(1)
#define SX1276_DIO2         MCU_GPIO_PORTB(0)
#define SX1276_DIO3         MCU_GPIO_PORTC(13)
#define SX1276_DIO4         MCU_GPIO_PORTA(5)
/* NOTE: DIO5 is not used, but must be defined */
#define SX1276_DIO5         (-1)
#define SX1276_NRESET       MCU_GPIO_PORTC(0)

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
