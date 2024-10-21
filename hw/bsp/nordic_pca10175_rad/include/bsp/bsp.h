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

#ifndef _BSP_H_
#define _BSP_H_

#include <inttypes.h>

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
#define RAM_SIZE        0x10000

/* LED pins */
#define LED_1           (28)
#define LED_2           (29)
#define LED_3           (30)
#define LED_4           (31)
#define LED_BLINK_PIN   (LED_2)

/* Buttons */
#define BUTTON_1        (23)
#define BUTTON_2        (24)
#define BUTTON_3        (8)
#define BUTTON_4        (9)

/* Arduino pins */
#define ARDUINO_PIN_D0      32
#define ARDUINO_PIN_D1      33
#define ARDUINO_PIN_D2      36
#define ARDUINO_PIN_D3      37
#define ARDUINO_PIN_D4      38
#define ARDUINO_PIN_D5      39
#define ARDUINO_PIN_D6      40
#define ARDUINO_PIN_D7      41
#define ARDUINO_PIN_D8      42
#define ARDUINO_PIN_D9      43
#define ARDUINO_PIN_D10     44
#define ARDUINO_PIN_D11     45
#define ARDUINO_PIN_D12     46
#define ARDUINO_PIN_D13     47
#define ARDUINO_PIN_A0      4
#define ARDUINO_PIN_A1      5
#define ARDUINO_PIN_A2      6
#define ARDUINO_PIN_A3      7
#define ARDUINO_PIN_A4      25
#define ARDUINO_PIN_A5      26

#define ARDUINO_PIN_RX      ARDUINO_PIN_D0
#define ARDUINO_PIN_TX      ARDUINO_PIN_D1

#define ARDUINO_PIN_SCL     35
#define ARDUINO_PIN_SDA     34

#define ARDUINO_PIN_SCK     ARDUINO_PIN_D13
#define ARDUINO_PIN_MOSI    ARDUINO_PIN_D11
#define ARDUINO_PIN_MISO    ARDUINO_PIN_D12

#ifdef __cplusplus
}
#endif

#endif /* _BSP_H_ */
