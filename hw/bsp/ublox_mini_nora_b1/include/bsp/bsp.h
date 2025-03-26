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
#define LED_RED         (28)
#define LED_GREEN       (30)
#define LED_BLUE        (43)
#define LED_1           (LED_GREEN)
#define LED_2           (LED_RED)
#define LED_3           (LED_BLUE)
#define LED_BLINK_PIN   (LED_1)

/* Buttons */
#define BUTTON_1        (4)
#define BUTTON_2        (22)

/* MikroBUS(R) pins */
#define MIKROBUS_1_PIN_AN   (26)  /* P0_26 */
#define MIKROBUS_1_PIN_CS   (11)  /* P0_11 */
#define MIKROBUS_1_PIN_SCK  (8)   /* P0_08 */
#define MIKROBUS_1_PIN_MISO (10)  /* P0_10 */
#define MIKROBUS_1_PIN_MOSI (9)   /* P0_09 */
#define MIKROBUS_1_PIN_PWM  (12)  /* P0_12 */
#define MIKROBUS_1_PIN_INT  (25)  /* P0_25 */
#define MIKROBUS_1_PIN_RX   (6)   /* P0_06 */
#define MIKROBUS_1_PIN_TX   (27)  /* P0_27 */
#define MIKROBUS_1_PIN_SCL  (35)  /* P1_03 */
#define MIKROBUS_1_PIN_SDA  (34)  /* P1_02 */

#define MIKROBUS_2_PIN_AN   (5)   /* P0_05 */
#define MIKROBUS_2_PIN_CS   (37)  /* P1_05 */
#define MIKROBUS_2_PIN_SCK  (24)  /* P0_24 */
#define MIKROBUS_2_PIN_MISO (38)  /* P1_06 */
#define MIKROBUS_2_PIN_MOSI (39)  /* P1_07 */
#define MIKROBUS_2_PIN_PWM  (29)  /* P0_29 */
#define MIKROBUS_2_PIN_INT  (32)  /* P1_00 */
#define MIKROBUS_2_PIN_RX   (33)  /* P1_01 */
#define MIKROBUS_2_PIN_TX   (31)  /* P0_31 */
#define MIKROBUS_2_PIN_SCL  (35)  /* P1_03 */
#define MIKROBUS_2_PIN_SDA  (34)  /* P1_02 */

/* QWIIC */
#define QWICC_PIN_SCL  (35)  /* P1_03 */
#define QWICC_PIN_SDA  (34)  /* P1_02 */

/* UART CP2105 ECI */
#define CP2105_ECI_RXD 44
#define CP2105_ECI_TXD 45
#define CP2105_ECI_CTS 46
#define CP2105_ECI_RTS 47

/* UART CP2105 SCI */
#define CP2105_SCI_RXD 20
#define CP2105_SCI_TXD 23
#define CP2105_SCI_RTS 19
#define CP2105_SCI_CTS 21

#ifdef __cplusplus
}
#endif

#endif /* _BSP_H_ */
