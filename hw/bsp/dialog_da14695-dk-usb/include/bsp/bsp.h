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
#define RAM_SIZE            0x80000

/* LED pins */
#define LED_1               (33)    /* P1_1 */
#define LED_BLINK_PIN       LED_1

/* Button pins */
#define BUTTON_1            (6)     /* P0_6 */

/* MikroBUS(R) pins */
#define MIKROBUS_1_PIN_AN   (41)  /* P1_9 */
#define MIKROBUS_1_PIN_CS   (20)  /* P0_20 */
#define MIKROBUS_1_PIN_SCK  (21)  /* P0_21 */
#define MIKROBUS_1_PIN_MISO (24)  /* P0_24 */
#define MIKROBUS_1_PIN_MOSI (26)  /* P0_26 */
#define MIKROBUS_1_PIN_PWM  (33)  /* P1_1 */
#define MIKROBUS_1_PIN_INT  (27)  /* P1_27 */
#define MIKROBUS_1_PIN_RX   (28)  /* P1_28 */
#define MIKROBUS_1_PIN_TX   (29)  /* P1_29 */
#define MIKROBUS_1_PIN_SCL  (30)  /* P1_30 */
#define MIKROBUS_1_PIN_SDA  (31)  /* P1_31 */

#define MIKROBUS_2_PIN_AN   (25)  /* P0_25 */
#define MIKROBUS_2_PIN_CS   (34)  /* P1_2 */
#define MIKROBUS_2_PIN_SCK  (35)  /* P1_3 */
#define MIKROBUS_2_PIN_MISO (36)  /* P1_4 */
#define MIKROBUS_2_PIN_MOSI (37)  /* P1_5 */
#define MIKROBUS_2_PIN_PWM  (38)  /* P1_6 */
#define MIKROBUS_2_PIN_INT  (39)  /* P1_7 */
#define MIKROBUS_2_PIN_RX   (40)  /* P1_8 */
#define MIKROBUS_2_PIN_TX   (17)  /* P0_17 */
#define MIKROBUS_2_PIN_SCL  (18)  /* P0_18 */
#define MIKROBUS_2_PIN_SDA  (19)  /* P0_19 */

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
