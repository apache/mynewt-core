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
extern uint8_t _ccram_start;

#define RAM_SIZE        (128 * 1024)
#define CCRAM_SIZE      (64 * 1024)

/* LED pins */
#define LED_1           MCU_GPIO_PORTA(6)
#define LED_2           MCU_GPIO_PORTA(7)
#define LED_BLINK_PIN   LED_1

/* Button pins */
#define BUTTON_K0       MCU_GPIO_PORTE(4)
#define BUTTON_K1       MCU_GPIO_PORTE(3)
#define BUTTON_1        BUTTON_K0
#define BUTTON_2        BUTTON_K1

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
