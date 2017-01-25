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
#include <mcu/cortex_m4.h>
#include <mcu/nrf52_hal.h>

/**
 * Boots the image described by the supplied image header.
 *
 * @param hdr                   The header for the image to boot.
 */
void
hal_system_start(void *img_start)
{
    typedef void jump_fn(void);

    uint32_t base0entry;
    uint32_t jump_addr;
    register jump_fn *fn;

    /* First word contains initial MSP value. */
    __set_MSP(*(uint32_t *)img_start);

    /* Second word contains address of entry point (Reset_Handler). */
    base0entry = *(uint32_t *)(img_start + 4);
    jump_addr = base0entry;
    fn = (jump_fn *)jump_addr;

    /* Jump to image. */
    fn();
}

/**
 * Boots the image described by the supplied image header.
 * This routine is used in split-app scenario when loader decides
 * that it wants to run the app instead.
 *
 * @param hdr                   The header for the image to boot.
 */
void
hal_system_restart(void *img_start)
{
    int i;
    int sr;

    /*
     * Disable interrupts, and leave the disabled.
     * They get re-enabled when system starts coming back again.
     */
    __HAL_DISABLE_INTERRUPTS(sr);
    for (i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++) {
        NVIC->ICER[i] = 0xffffffff;
    }
    (void)sr;

    hal_system_start(img_start);
}
