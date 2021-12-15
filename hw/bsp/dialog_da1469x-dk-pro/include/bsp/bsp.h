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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Define special stackos sections */
#define sec_data_core   __attribute__((section(".data.core")))
#define sec_bss_core    __attribute__((section(".bss.core")))
#define sec_bss_nz_core __attribute__((section(".bss.core.nz")))

/* More convenient section placement macros. */
#define bssnz_t         sec_bss_nz_core

extern uint8_t _ram_start;
#define RAM_SIZE        0x80000

/* LED pins */
#define LED_1           (33)    /* P1_1 */
#define LED_2           (43)    /* P1_11 */
#define LED_3           (42)    /* P1_10 */
#define LED_BLINK_PIN   LED_1

/* Button pin */
#define BUTTON_1        (6)     /* P0_6 */

/* Arduino pins */
#if MYNEWT_VAL_CHOICE(DA1469X_DK_PRO_REV, 331_O7_B)
#define ARDUINO_PIN_D0      0
#define ARDUINO_PIN_D1      1
#define ARDUINO_PIN_D2      2
#define ARDUINO_PIN_D3      3
#define ARDUINO_PIN_D4      4
#define ARDUINO_PIN_D5      5
#else
#define ARDUINO_PIN_D0      34
#define ARDUINO_PIN_D1      35
#define ARDUINO_PIN_D2      36
#define ARDUINO_PIN_D3      37
#define ARDUINO_PIN_D4      39
#define ARDUINO_PIN_D5      40
#endif
#define ARDUINO_PIN_D6      17
#define ARDUINO_PIN_D7      18
#define ARDUINO_PIN_D8      19
#define ARDUINO_PIN_D9      20
#define ARDUINO_PIN_D10     21
#define ARDUINO_PIN_D11     24
#define ARDUINO_PIN_D12     26
#define ARDUINO_PIN_D13     27
#define ARDUINO_PIN_A0      41
#define ARDUINO_PIN_A1      25
#define ARDUINO_PIN_A2      8
#define ARDUINO_PIN_A3      9
#define ARDUINO_PIN_A4      45
#define ARDUINO_PIN_A5      44

#define ARDUINO_PIN_RX      ARDUINO_PIN_D0
#define ARDUINO_PIN_TX      ARDUINO_PIN_D1

#define ARDUINO_PIN_SCL     29
#define ARDUINO_PIN_SDA     28

#define ARDUINO_PIN_SCK     ARDUINO_PIN_D13
#define ARDUINO_PIN_MOSI    ARDUINO_PIN_D11
#define ARDUINO_PIN_MISO    ARDUINO_PIN_D12

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
