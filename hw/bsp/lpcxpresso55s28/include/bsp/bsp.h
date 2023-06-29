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
#include <mcu/mcu.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t __DATA_ROM;
#define SRAM0_BASE          0x20000000
#define SRAM1_BASE          0x20010000
#define SRAM2_BASE          0x20020000
#define SRAM4_BASE          0x20040000
#define SRAM0_SIZE          0x10000
#define SRAM1_SIZE          0x10000
#define SRAM2_SIZE          0x10000
#define SRAM4_SIZE          0x4000
#define SRAMX_BASE          0x04000000
#define SRAMX_SIZE          0x8000

#define RAM_SIZE            (SRAM0_SIZE + SRAM1_SIZE + SRAM2_SIZE)
extern uint8_t _ram_start[];

/* LED pins */
#define LED_1               MCU_GPIO_PORT1(6)
#define LED_2               MCU_GPIO_PORT1(7)
#define LED_3               MCU_GPIO_PORT1(4)
#define LED_RED             LED_1
#define LED_GREEN           LED_2
#define LED_BLUE            LED_3
#define LED_BLINK_PIN       LED_BLUE

/* Button pin */
#define BUTTON_1            MCU_GPIO_PORT0(5)
#define BUTTON_2            MCU_GPIO_PORT1(9)

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
