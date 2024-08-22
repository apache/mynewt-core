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

#include "os/mynewt.h"
#include <hal/hal_system.h>
#include <mpu_armv8.h>

static int
mpu_add_region(uint32_t rnr, uint32_t start, size_t size, uint8_t attr_ix, uint8_t ro, uint8_t xn)
{
    if (rnr < 8) {
        ARM_MPU_SetRegion(rnr, ARM_MPU_RBAR(start, 1, ro, 1, xn), ARM_MPU_RLAR((start + size - 1), attr_ix));
        rnr++;
    }
    return rnr;
}

static void
mpu_init(void)
{
    int nrn = 0;

    ARM_MPU_Disable();
    /* Set Attr 0 - normal memory */
    ARM_MPU_SetMemAttr(0, ARM_MPU_ATTR(
        ARM_MPU_ATTR_MEMORY_(0, 1, 1, 1),   /* Outer Write-Back transient with read and write allocate */
        ARM_MPU_ATTR_MEMORY_(0, 0, 1, 1))); /* Inner Write-Through transient with read and write allocate */

    /*
     * Add USB RAM, this is required to allow unaligned access to this RAM that is used by
     * USB1 (High-speed) for storing data
     */
    mpu_add_region(nrn, FSL_FEATURE_USB_USB_RAM_BASE_ADDRESS, FSL_FEATURE_USB_USB_RAM, 0, 0, 1);

    /* Enable MPU with default map */
    ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);
    __DSB();
    __ISB();
}

void
hal_system_init(void)
{
    SystemCoreClockUpdate();

    /* Relocate the vector table */
    NVIC_Relocate();

    /* MPU required to access USB SRAM needed by TinyUSB */
    mpu_init();
}

