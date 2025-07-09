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

#define RAM_SIZE        (384 * 1024)

/* UART */
#define UART_CNT        (2)

/* LED pins */
#define LED_1         MYNEWT_VAL_LED_1_PIN
#define LED_2         MYNEWT_VAL_LED_2_PIN
#define LED_3         MYNEWT_VAL_LED_3_PIN
#define LED_4         MYNEWT_VAL_LED_4_PIN
#define LED_BLINK_PIN MYNEWT_VAL_LED_0_PIN

/* Buttons */
#define BUTTON_1 MYNEWT_VAL_BUTTON_0_PIN
#define BUTTON_2 MYNEWT_VAL_BUTTON_1_PIN
#define BUTTON_3 MYNEWT_VAL_BUTTON_2_PIN

/* Arduino pins */
#define ARDUINO_PIN_D0      23
#define ARDUINO_PIN_D1      22
#define ARDUINO_PIN_D2      26
#define ARDUINO_PIN_D3      49
#define ARDUINO_PIN_D4      48
#define ARDUINO_PIN_D5      47
#define ARDUINO_PIN_D6      46
#define ARDUINO_PIN_D7      45
#define ARDUINO_PIN_D8      44
#define ARDUINO_PIN_D9      43
#define ARDUINO_PIN_D10     42
#define ARDUINO_PIN_D11     7
#define ARDUINO_PIN_D12     6
#define ARDUINO_PIN_D13     5
#define ARDUINO_PIN_A0      13
#define ARDUINO_PIN_A1      29
#define ARDUINO_PIN_A2      11
#define ARDUINO_PIN_A3      31
#define ARDUINO_PIN_A4      32
#define ARDUINO_PIN_A5      33

#define ARDUINO_PIN_RX      ARDUINO_PIN_D0
#define ARDUINO_PIN_TX      ARDUINO_PIN_D1

#define ARDUINO_PIN_SCL     8
#define ARDUINO_PIN_SDA     9

#define ARDUINO_PIN_SCK     ARDUINO_PIN_D13
#define ARDUINO_PIN_MOSI    ARDUINO_PIN_D11
#define ARDUINO_PIN_MISO    ARDUINO_PIN_D12

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
