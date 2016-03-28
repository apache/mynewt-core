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
#ifndef __NATIVE_BSP_H
#define __NATIVE_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mcu/native_bsp.h>
    
/* Define special stackos sections */
#define sec_data_core
#define sec_bss_core
#define sec_bss_nz_core

/* More convenient section placement macros. */
#define bssnz_t

/* LED pins */
#define LED_BLINK_PIN   (0x1)

/* Logical UART ports */
#define UART_CNT	2
#define CONSOLE_UART	0

int bsp_imgr_current_slot(void);

#define NFFS_AREA_MAX    (8)
 
/* this is a native build anjd these are just simulated */
enum BspPinDescriptor {
    NATIVE_PIN_A0 = MCU_PIN_0,
    NATIVE_PIN_A1 = MCU_PIN_1,
    NATIVE_PIN_A2 = MCU_PIN_2,
    NATIVE_PIN_A3 = MCU_PIN_3,
    NATIVE_PIN_A4 = MCU_PIN_4,
    NATIVE_PIN_A5 = MCU_PIN_5,
    /* TODO */
};

#ifdef __cplusplus
}
#endif

#endif  /* __NATIVE_BSP_H */
