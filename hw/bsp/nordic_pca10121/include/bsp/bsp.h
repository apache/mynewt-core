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
#define LED_1               31
#define LED_2               32
#define LED_3               33
#define RGB_LED_1_RED       7
#define RGB_LED_1_GREEN     25
#define RGB_LED_1_BLUE      26
#define RGB_LED_2_RED       28
#define RGB_LED_2_GREEN     29
#define RGB_LED_2_BLUE      30
#define LED_BLINK_PIN       (LED_1)

/* Buttons */
#define BUTTON_1            2
#define BUTTON_2            3
#define BUTTON_3            4
#define BUTTON_4            6
#define BUTTON_5            5
#define BUTTON_VOL_DOWN     BUTTON_1
#define BUTTON_VOL_UP       BUTTON_2
#define BUTTON_PLAY         BUTTON_3

/* Arduino pins */
#define ARDUINO_PIN_D0      41
#define ARDUINO_PIN_D1      40
#define ARDUINO_PIN_D2      31
#define ARDUINO_PIN_D3      32
#define ARDUINO_PIN_D4      33
#define ARDUINO_PIN_D5      46
#define ARDUINO_PIN_D6      39
#define ARDUINO_PIN_D7      43
#define ARDUINO_PIN_D8      42
#define ARDUINO_PIN_D9      45
#define ARDUINO_PIN_D10     44
#define ARDUINO_PIN_D11     9
#define ARDUINO_PIN_D12     10
#define ARDUINO_PIN_D13     8
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

/* Other on board pin selection */

#define PCA100121_SDCARD_CS_PIN         11
#define PCA100121_SDCARD_SCK_PIN        8
#define PCA100121_SDCARD_MOSI_PIN       9
#define PCA100121_SDCARD_MISO_PIN       10
#define PCA100121_CS47L63_CS_PIN        17
#define PCA100121_CS47L63_SCK_PIN       8
#define PCA100121_CS47L63_MOSI_PIN      9
#define PCA100121_CS47L63_MISO_PIN      10
#define PCA100121_CS47L63_RESET_PIN     18
#define PCA100121_HW_CODEC_ON_BOARD_PIN 21

#ifdef __cplusplus
}
#endif

#endif /* _BSP_H_ */
