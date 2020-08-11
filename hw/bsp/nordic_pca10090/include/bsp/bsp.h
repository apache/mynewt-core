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

#include "os/mynewt.h"

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
#define RAM_SIZE        0x40000

/* LED pins */
#define LED_1           (2)
#define LED_2           (3)
#define LED_3           (4)
#define LED_4           (5)
#define LED_BLINK_PIN   (LED_1)

/* Buttons */
#define BUTTON_1 (6)
#define BUTTON_2 (7)
#define SWITCH_1 (8)
#define SWITCH_2 (9)

/* Arduino pins */
#define ARDUINO_PIN_D0      31
#define ARDUINO_PIN_D1      30
#define ARDUINO_PIN_D2      20
#define ARDUINO_PIN_D3      13
#define ARDUINO_PIN_D4      12
#define ARDUINO_PIN_D5      11
#define ARDUINO_PIN_D6      10
#define ARDUINO_PIN_D7       9
#define ARDUINO_PIN_D8       8
#define ARDUINO_PIN_D9       7
#define ARDUINO_PIN_D10      6
#define ARDUINO_PIN_D11      5
#define ARDUINO_PIN_D12      4
#define ARDUINO_PIN_D13      3
#define ARDUINO_PIN_D14      2
#define ARDUINO_PIN_D15      1
#define ARDUINO_PIN_D16      0
#define ARDUINO_PIN_D17     28
#define ARDUINO_PIN_D18     27
#define ARDUINO_PIN_D19     26
#define ARDUINO_PIN_D20     25
#define ARDUINO_PIN_D21     24
#define ARDUINO_PIN_D22     23
#define ARDUINO_PIN_D23     22
#define ARDUINO_PIN_D24     21

#define ARDUINO_PIN_A0      14
#define ARDUINO_PIN_A1      15
#define ARDUINO_PIN_A2      16
#define ARDUINO_PIN_A3      17
#define ARDUINO_PIN_A4      18
#define ARDUINO_PIN_A5      19

#define ARDUINO_PIN_RX      ARDUINO_PIN_D5
#define ARDUINO_PIN_TX      ARDUINO_PIN_D4

#define ARDUINO_PIN_SCL     ARDUINO_PIN_D18
#define ARDUINO_PIN_SDA     ARDUINO_PIN_D19

#define ARDUINO_PIN_SCK     ARDUINO_PIN_D20
#define ARDUINO_PIN_MOSI    ARDUINO_PIN_D22
#define ARDUINO_PIN_MISO    ARDUINO_PIN_D21

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
