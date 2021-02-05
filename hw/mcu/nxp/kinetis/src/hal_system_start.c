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

/**
 * Boots the image described by the supplied image header.
 *
 * @param hdr                   The header for the image to boot.
 */
void
hal_system_start(void *img_start)
{
    /* Turn off interrupts. */
    __disable_irq();

    /* Set the VTOR to default. */
    SCB->VTOR = 0;

    // Memory barriers for good measure.
    __ISB();
    __DSB();

    /* First word contains initial MSP value. */
    __set_MSP(*(uint32_t *)img_start);
    __set_PSP(*(uint32_t *)img_start);

    /* Second word contains address of entry point (Reset_Handler). */
    void (*entry)(void) = (void (*)(void))*(uint32_t *)(img_start + 4);

    /* Jump to image. */
    entry();

    /* Should never reach this point */
    while (1)
        ;
}
