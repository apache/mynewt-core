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

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "os/mynewt.h"

/* Define special stackos sections */
#define sec_data_core   __attribute__((section(".data.core")))
#define sec_bss_core    __attribute__((section(".bss.core")))

/* More convenient section placement macros. */
#define bssnz_t

extern uint8_t _ram_start;
#define RAM_SIZE        0x4000

/* LED pins */
#define LED_BLINK_PIN   (-1)

#define LED_COL1   (4)
#define LED_COL2   (5)
#define LED_COL3   (6)
#define LED_COL4   (7)
#define LED_COL5   (8)
#define LED_COL6   (9)
#define LED_COL7   (10)
#define LED_COL8   (11)
#define LED_COL9   (12)

#define LED_ROW1   (13)
#define LED_ROW2   (14)
#define LED_ROW3   (15)

#define BUTTON_A_PIN   (17)
#define BUTTON_B_PIN   (16)

#define CALLIOPE_PIN_MOTOR_SLEEP (28)
#define CALLIOPE_PIN_MOTOR_IN1  (29)
#define CALLIOPE_PIN_MOTOR_IN2  (30)

#define CALLIOPE_MICROPHONE (3)

#ifdef __GNUC__
#define FUNCTION_IS_NOT_USED __attribute__ ((unused))
#else
#define FUNCTION_IS_NOT_USED
#endif

/* UART info */
#define CONSOLE_UART    "uart0"

#define NFFS_AREA_MAX   (8)

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
