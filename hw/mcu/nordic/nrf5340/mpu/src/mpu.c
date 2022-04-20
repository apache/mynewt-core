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

#include <os/mynewt.h>
#include <hal/hal_spi.h>
#include <nrf.h>
#include <bsp/bsp.h>

void
mpu_pkg_init(void)
{
    const struct flash_area *fa;
    ARM_MPU_Region_t regions[5];
    uint32_t bootloader_end = 0;
    int region_index;
    /*
     * Protect settings:
     *              read        write      execute
     *              ------------------------------
     * RAM            +           +          -
     * Flash:
     * - Bootloader   -           -          -
     * - Slot0        +           -          +
     * - Rest         +           +          -
     * Peripherals:   +           +          -
     */

    ARM_MPU_Disable();
    /* Set Attr 0 */
    ARM_MPU_SetMemAttr(0UL, ARM_MPU_ATTR(           /* Normal memory */
        ARM_MPU_ATTR_MEMORY_(0UL, 1UL, 1UL, 1UL),   /* Outer Write-Back transient with read and write allocate */
        ARM_MPU_ATTR_MEMORY_(0UL, 0UL, 1UL, 1UL))); /* Inner Write-Through transient with read and write allocate */

    /* RAM */
    regions[0].RBAR = ARM_MPU_RBAR((uint32_t)&_ram_start, ARM_MPU_SH_OUTER, 0UL, 1UL, 1UL);
    regions[0].RLAR = ARM_MPU_RLAR(((uint32_t)&_ram_start + RAM_SIZE - 1), 0UL);
    region_index = 1;

    if (flash_area_open(FLASH_AREA_BOOTLOADER, &fa) == 0) {
        bootloader_end = fa->fa_size;
    }
#if MYNEWT_VAL(BOOT_LOADER)
    if (bootloader_end) {
        /* Bootloader read/exec */
        regions[region_index].RBAR = ARM_MPU_RBAR(0, ARM_MPU_SH_NON, 1UL, 1UL, 0UL);
        regions[region_index].RLAR = ARM_MPU_RLAR((bootloader_end - 1), 0UL);
        region_index++;
    }
#endif
    if (flash_area_open(FLASH_AREA_IMAGE_0, &fa) == 0) {
        /* Flash between bootloader and SLOT0 marked as read/write */
        if (fa->fa_off > bootloader_end) {
            regions[region_index].RBAR = ARM_MPU_RBAR(bootloader_end, ARM_MPU_SH_NON, 0UL, 1UL, 1UL);
            regions[region_index].RLAR = ARM_MPU_RLAR((fa->fa_off - 1), 0UL);
            region_index++;
        }
        /* SLOT0 read/exec */
        regions[region_index].RBAR = ARM_MPU_RBAR(fa->fa_off, ARM_MPU_SH_NON, 1UL, 1UL, 0UL);
        regions[region_index].RLAR = ARM_MPU_RLAR((fa->fa_off + fa->fa_size - 0x1000 - 1), 0UL);
        region_index++;
        /* Rest of flash read/write */
        regions[region_index].RBAR = ARM_MPU_RBAR((fa->fa_off + fa->fa_size - 0x1000), ARM_MPU_SH_NON, 0UL, 1UL, 1UL);
        regions[region_index].RLAR = ARM_MPU_RLAR((0x100000 - 1), 0UL);
        region_index++;
        /* Peripherals read/write */
        regions[region_index].RBAR = ARM_MPU_RBAR(0x40000000, ARM_MPU_SH_OUTER, 0UL, 1UL, 1UL);
        regions[region_index].RLAR = ARM_MPU_RLAR((0xF0000000 - 1), 0UL);
        region_index++;
        ARM_MPU_Load(0, regions, region_index);
        /* Enable MPU with no default map, only explicit regions are allowed */
        ARM_MPU_Enable(0);
    }
}
