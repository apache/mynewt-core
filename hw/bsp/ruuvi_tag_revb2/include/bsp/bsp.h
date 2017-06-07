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

#include <syscfg/syscfg.h>

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
#define RAM_SIZE        0x10000

/* IRQ */
#define ACC_INT1        (2)
#define ACC_INT2        (6)

/* SPI CS */
#define SPI_HUMI_CS     (3)
#define SPI_ACC_CS      (8)

/* LED pins */
#define LED_1           (17)
#define LED_2           (19)
#define LED_BLINK_PIN   (LED_1)

#if MYNEWT_VAL(BOOT_SERIAL)
#define BOOT_SERIAL_DETECT_PIN          13 /* Button 1 */
#define BOOT_SERIAL_DETECT_PIN_CFG      HAL_GPIO_PULL_UP
#define BOOT_SERIAL_DETECT_PIN_VAL      0

#define BOOT_SERIAL_REPORT_PIN          LED_BLINK_PIN
#define BOOT_SERIAL_REPORT_FREQ         (MYNEWT_VAL(OS_CPUTIME_FREQ) / 4)
#endif

#define NFFS_AREA_MAX   (8)

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
