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

/* More convenient section placement macros. */
#define bssnz_t

extern uint8_t _ram_start;
#define RAM_SIZE        0x8000

/* LED pins */
#define LED_1           (17)
#define LED_2           (18)
#define LED_3           (19)
#define LED_4           (20)
#define LED_BLINK_PIN   (LED_1)

/* Buttons */
#define BUTTON_1 (13)
#define BUTTON_2 (14)
#define BUTTON_3 (15)
#define BUTTON_4 (16)

/* Arduino board mappings */
#define ARDUINO_SCL_PIN  (27)
#define ARDUINO_SDA_PIN  (26)
#define ARDUINO_AREF_PIN (2)
#define ARDUINO_13_PIN   (25)
#define ARDUINO_12_PIN   (24)
#define ARDUINO_11_PIN   (23)
#define ARDUINO_10_PIN   (22)
#define ARDUINO_9_PIN    (20)
#define ARDUINO_8_PIN    (19)

#define ARDUINO_7_PIN    (18)
#define ARDUINO_6_PIN    (17)
#define ARDUINO_5_PIN    (16)
#define ARDUINO_4_PIN    (15)
#define ARDUINO_3_PIN    (14)
#define ARDUINO_2_PIN    (13)
#define ARDUINO_1_PIN    (12)
#define ARDUINO_0_PIN    (11)

#define ARDUINO_A0_PIN   (3)
#define ARDUINO_A1_PIN   (4)
#define ARDUINO_A2_PIN   (28)
#define ARDUINO_A3_PIN   (29)
#define ARDUINO_A4_PIN   (30)
#define ARDUINO_A5_PIN   (31)

#define NFFS_AREA_MAX    (8)


#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
