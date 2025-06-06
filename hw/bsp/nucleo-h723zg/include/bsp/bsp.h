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
#include <mcu/mcu.h>

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
extern uint8_t _dtcm_start;
extern uint8_t _itcm_start;
extern uint8_t _ram2_start;

#define RAM_SIZE        (320 * 1024)
#define RAM2_SIZE       (32 * 1024)
#define RAM3_SIZE       (16 * 1024)
#define DTCMRAM_SIZE    (128 * 1024)
#define ITCMRAM_SIZE    (64 * 1024)

/* LED pins */
#define LED_1           MCU_GPIO_PORTB(0)
#define LED_2           MCU_GPIO_PORTE(1)
#define LED_3           MCU_GPIO_PORTB(14)

#define LED_green       LED_1
#define LED_blue        LED_2
#define LED_red         LED_3

#define LED_BLINK_PIN   LED_1

/* BUTTON pins */
#define BTN_USER_1      MCU_GPIO_PORTC(13)

/* Button pin */
#define BUTTON_1        BTN_USER_1

/* Arduino pins */
#define ARDUINO_PIN_D0      MCU_GPIO_PORTB(7)
#define ARDUINO_PIN_D1      MCU_GPIO_PORTB(6)
#define ARDUINO_PIN_D2      MCU_GPIO_PORTG(14)
#define ARDUINO_PIN_D3      MCU_GPIO_PORTE(13)
#define ARDUINO_PIN_D4      MCU_GPIO_PORTE(14)
#define ARDUINO_PIN_D5      MCU_GPIO_PORTE(11)
#define ARDUINO_PIN_D6      MCU_GPIO_PORTE(9)
#define ARDUINO_PIN_D7      MCU_GPIO_PORTG(12)
#define ARDUINO_PIN_D8      MCU_GPIO_PORTF(3)
#define ARDUINO_PIN_D9      MCU_GPIO_PORTD(15)
#define ARDUINO_PIN_D10     MCU_GPIO_PORTD(14)
#define ARDUINO_PIN_D11     MCU_GPIO_PORTB(5)
#define ARDUINO_PIN_D12     MCU_GPIO_PORTA(6)
#define ARDUINO_PIN_D13     MCU_GPIO_PORTA(5)
#define ARDUINO_PIN_A0      MCU_GPIO_PORTA(3)
#define ARDUINO_PIN_A1      MCU_GPIO_PORTC(0)
#define ARDUINO_PIN_A2      MCU_GPIO_PORTC(3)
#define ARDUINO_PIN_A3      MCU_GPIO_PORTB(1)
#define ARDUINO_PIN_A4      MCU_GPIO_PORTC(2)
#define ARDUINO_PIN_A5      MCU_GPIO_PORTF(10)

#define ARDUINO_PIN_RX      ARDUINO_PIN_D0
#define ARDUINO_PIN_TX      ARDUINO_PIN_D1

#define ARDUINO_PIN_SCL     MCU_GPIO_PORTB(8)
#define ARDUINO_PIN_SDA     MCU_GPIO_PORTB(9)

#define ARDUINO_PIN_SCK     ARDUINO_PIN_D13
#define ARDUINO_PIN_MOSI    ARDUINO_PIN_D11
#define ARDUINO_PIN_MISO    ARDUINO_PIN_D12

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
