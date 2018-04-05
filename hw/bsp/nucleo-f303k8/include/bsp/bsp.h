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
#include <mcu/mcu.h>
#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Define special stackos sections */
#define sec_data_core   __attribute__((section(".data.core")))
#define sec_bss_core    __attribute__((section(".bss.core")))
#define sec_bss_nz_core __attribute__((section(".bss.core.nz")))

/* More convenient section placement macros. */
#define bssnz_t         sec_bss_nz_core

/*
 * Symbols from linker script.
 */
extern uint8_t _sram_start;
extern uint8_t _ccram_start;

#define SRAM_SIZE (12 * 1024)
#define CCRAM_SIZE (4 *1024)

/* LED pins */
#define LED_BLINK_PIN_1   MCU_GPIO_PORTB(3)
    
#define LED_BLINK_PIN LED_BLINK_PIN_1
    

/* UART ports */
#if (MYNEWT_VAL(UART_0) && MYNEWT_VAL(UART_1))
#   define  UART_CNT      2
#   define  UART_0_DEV_ID 0
#   define  UART_1_DEV_ID 1
#elif MYNEWT_VAL(UART_0)
#   define  UART_CNT      1
#   define  UART_0_DEV_ID 0
#elif MYNEWT_VAL(UART_1)
#   define UART_CNT       1
#   define  UART_1_DEV_ID 0
#else
#   define UART_CNT       0
#endif

#define _UART_DEV_NAME(id) ("uart" #id)
#define UART_DEV_NAME(id) _UART_DEV_NAME(id)

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
