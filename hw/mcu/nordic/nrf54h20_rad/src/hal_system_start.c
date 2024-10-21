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

#include <stddef.h>
#include <inttypes.h>
#include <mcu/cortex_m33.h>
#include <mcu/nrf54h20_rad_hal.h>
//
///**
// * Boots the image described by the supplied image header.
// *
// * @param hdr                   The header for the image to boot.
// */
//void __attribute__((naked))
//hal_system_start(void *img_start)
//{
//    uint32_t *img_data = img_start;
//
//    asm volatile (".syntax unified        \n"
//                  /* 1st word is stack pointer */
//                  "    msr  msp, %0       \n"
//                  /* 2nd word is a reset handler (image entry) */
//                  "    bx   %1            \n"
//                  : /* no output */
//                  : "r" (img_data[0]), "r" (img_data[1]));
//}
//
///**
// * Boots the image described by the supplied image header.
// * This routine is used in split-app scenario when loader decides
// * that it wants to run the app instead.
// *
// * @param hdr                   The header for the image to boot.
// */
//void
//hal_system_restart(void *img_start)
//{
//    int i;
//
//    /*
//     * NOTE: on reset, PRIMASK should have global interrupts enabled so
//     * the code disables interrupts, clears the interrupt enable bits,
//     * clears any pending interrupts, then enables global interrupts
//     * so processor looks like state it would be in if it reset.
//     */
//    __disable_irq();
//    for (i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++) {
//        NVIC->ICER[i] = 0xffffffff;
//    }
//
//    for (i = 0; i < sizeof(NVIC->ICPR) / sizeof(NVIC->ICPR[0]); i++) {
//        NVIC->ICPR[i] = 0xffffffff;
//    }
//    __enable_irq();
//
//    hal_system_start(img_start);
//}
