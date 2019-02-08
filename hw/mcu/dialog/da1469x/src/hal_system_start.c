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

#include <assert.h>
#include <stdint.h>
#include "mcu/mcu.h"
#include "mcu/da1469x_hal.h"
#include "DA1469xAB.h"

void __attribute__((naked))
hal_system_start(void *img_start)
{
    uint32_t img_data_addr;
    uint32_t *img_data;

    img_data_addr = MCU_MEM_QSPIF_M_START_ADDRESS + (uint32_t)img_start;

    assert(img_data_addr < MCU_MEM_QSPIF_M_END_ADDRESS);

    img_data = (uint32_t *)img_data_addr;

    asm volatile (".syntax unified        \n"
                  /* 1st word is stack pointer */
                  "    msr  msp, %0       \n"
                  /* 2nd word is a reset handler (image entry) */
                  "    bx   %1            \n"
                  : /* no output */
                  : "r" (img_data[0]), "r" (img_data[1]));
}

void
hal_system_restart(void *img_start)
{
    uint32_t primask __attribute__((unused));
    int i;

    /*
     * Disable interrupts, and leave them disabled.
     * They get re-enabled when system starts coming back again.
     */
    __HAL_DISABLE_INTERRUPTS(primask);

    for (i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++) {
        NVIC->ICER[i] = 0xffffffff;
    }

    hal_system_start(img_start);
}
