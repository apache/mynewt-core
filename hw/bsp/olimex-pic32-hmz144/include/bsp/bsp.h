/**
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
#include <xc.h>
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

extern uint8_t _ram_start[];

#define RAM_SIZE        (512 * 1024)

/* LED pins */
#define LED_1           MCU_GPIO_PORTH(2)
#define LED_BLINK_PIN   LED_1

/* Buttons */
#define BUTTON_1        MCU_GPIO_PORTB(12)

/* UEXT connector pins UEXT_1-VCC, UEXT_2-GND */
#define UEXT_3  MCU_GPIO_PORTE(8)
#define UEXT_4  MCU_GPIO_PORTE(9)
#define UEXT_5  MCU_GPIO_PORTA(2)
#define UEXT_6  MCU_GPIO_PORTA(3)
#define UEXT_7  MCU_GPIO_PORTD(14)
#define UEXT_8  MCU_GPIO_PORTD(15)
#define UEXT_9  MCU_GPIO_PORTD(1)
#define UEXT_10 MCU_GPIO_PORTB(15)

#define UEXT_3_U2TX  UEXT_3
#define UEXT_4_U2RX  UEXT_4
#define UEXT_5_SCL2  UEXT_5
#define UEXT_6_SDA2  UEXT_6
#define UEXT_7_MISO1 UEXT_7
#define UEXT_8_MOSI1 UEXT_8
#define UEXT_9_SCK1  UEXT_9
#define UEXT_10_SS1  UEXT_10

/* Micro SD */
#define MICROSD_SPI_NUM      1
#define MICROSD_SPI_MISO_PIN MCU_GPIO_PORTD(7)
#define MICROSD_SPI_MOSI_PIN MCU_GPIO_PORTG(8)
#define MICROSD_SPI_SCK_PIN  MCU_GPIO_PORTG(6)
#define MICROSD_SPI_CS_PIN   MCU_GPIO_PORTB(14)
#define MICROSD_SPI_CD_PIN   MCU_GPIO_PORTJ(5)

/* UART */
#define UART_CNT        (6)

/* SPI */
#define SPI_CNT         (6)

/* I2C */
#define I2C_CNT         (5)

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
