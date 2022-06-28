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
#define RAM_SIZE        0x80000

/* LED pins */
#define LED_1           (28)
#define LED_2           (29)
#define LED_3           (30)
#define LED_4           (31)
#define LED_BLINK_PIN   (LED_1)

/* Buttons */
#define BUTTON_1        (23)
#define BUTTON_2        (24)
#define BUTTON_3        (8)
#define BUTTON_4        (9)

/* Arduino pins */
#define ARDUINO_PIN_D0      MYNEWT_VAL(ARDUINO_PIN_D0)
#define ARDUINO_PIN_D1      MYNEWT_VAL(ARDUINO_PIN_D1)
#define ARDUINO_PIN_D2      MYNEWT_VAL(ARDUINO_PIN_D2)
#define ARDUINO_PIN_D3      MYNEWT_VAL(ARDUINO_PIN_D3)
#define ARDUINO_PIN_D4      MYNEWT_VAL(ARDUINO_PIN_D4)
#define ARDUINO_PIN_D5      MYNEWT_VAL(ARDUINO_PIN_D5)
#define ARDUINO_PIN_D6      MYNEWT_VAL(ARDUINO_PIN_D6)
#define ARDUINO_PIN_D7      MYNEWT_VAL(ARDUINO_PIN_D7)
#define ARDUINO_PIN_D8      MYNEWT_VAL(ARDUINO_PIN_D8)
#define ARDUINO_PIN_D9      MYNEWT_VAL(ARDUINO_PIN_D9)
#define ARDUINO_PIN_D10     MYNEWT_VAL(ARDUINO_PIN_D10)
#define ARDUINO_PIN_D11     MYNEWT_VAL(ARDUINO_PIN_D11)
#define ARDUINO_PIN_D12     MYNEWT_VAL(ARDUINO_PIN_D12)
#define ARDUINO_PIN_D13     MYNEWT_VAL(ARDUINO_PIN_D13)
#define ARDUINO_PIN_A0      MYNEWT_VAL(ARDUINO_PIN_A0)
#define ARDUINO_PIN_A1      MYNEWT_VAL(ARDUINO_PIN_A1)
#define ARDUINO_PIN_A2      MYNEWT_VAL(ARDUINO_PIN_A2)
#define ARDUINO_PIN_A3      MYNEWT_VAL(ARDUINO_PIN_A3)
#define ARDUINO_PIN_A4      MYNEWT_VAL(ARDUINO_PIN_A4)
#define ARDUINO_PIN_A5      MYNEWT_VAL(ARDUINO_PIN_A5)

#define ARDUINO_PIN_RX      ARDUINO_PIN_D0
#define ARDUINO_PIN_TX      ARDUINO_PIN_D1

#define ARDUINO_PIN_SCL     MYNEWT_VAL(ARDUINO_PIN_SCL)
#define ARDUINO_PIN_SDA     MYNEWT_VAL(ARDUINO_PIN_SDA)

#define ARDUINO_PIN_SCK     ARDUINO_PIN_D13
#define ARDUINO_PIN_MOSI    ARDUINO_PIN_D11
#define ARDUINO_PIN_MISO    ARDUINO_PIN_D12

#ifdef __cplusplus
}
#endif

#endif /* _BSP_H_ */
