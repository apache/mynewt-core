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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "flash_map/flash_map.h"
#include "hal/hal_bsp.h"
#include "img_mgmt/img_mgmt.h"
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"

#include "memfault/core/math.h"
#include "memfault/panics/platform/coredump.h"


const sMfltCoredumpRegion *
memfault_platform_coredump_get_regions(const sCoredumpCrashInfo *crash_info,
                                       size_t *num_regions)
{
    static sMfltCoredumpRegion s_coredump_regions[3];
    int area_cnt;

    const struct hal_bsp_mem_dump *mem = hal_bsp_core_dump(&area_cnt);

    s_coredump_regions[0] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
       mem->hbmd_start, mem->hbmd_size);

    // s_coredump_regions[0] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(start_addr1, size1);
    // s_coredump_regions[1] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(start_addr2, size2);
    // s_coredump_regions[2] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(start_addr3, size3);

    *num_regions = MEMFAULT_ARRAY_SIZE(s_coredump_regions);

    return s_coredump_regions;
}

int
prv_flash_open(const struct flash_area **fa)
{
    int slot;

    if (flash_area_open(MYNEWT_VAL(COREDUMP_FLASH_AREA), fa)) {
        return -OS_ERROR;
    }

    /* Don't overwrite an image that has any flags set (pending, active, or
     * confirmed).
     */
    slot = flash_area_id_to_image_slot(MYNEWT_VAL(COREDUMP_FLASH_AREA));
    if (slot != -1 && img_mgmt_slot_in_use(slot)) {
        return -OS_ERROR;
    }

    return OS_OK;
}

void
memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info)
{
    const struct flash_area *fa;

    if (prv_flash_open(&fa)) {
        assert(0);
    }

    *info = (sMfltCoredumpStorageInfo) {
        .size = fa->fa_size,
        .sector_size = fa->fa_size,
    };
}

bool
memfault_platform_coredump_storage_read(uint32_t offset, void *data,
                                        size_t read_len)
{
    const struct flash_area *fa;

    if (prv_flash_open(&fa)) {
        return false;
    }

    if ((offset + read_len) > fa->fa_size) {
        return false;
    }

    if (flash_area_read(fa, offset, data, read_len)) {
        return false;
    }

    return true;
}

bool
memfault_platform_coredump_storage_erase(uint32_t offset, size_t erase_size)
{
    const struct flash_area *fa;

    if (prv_flash_open(&fa)) {
        return false;
    }

    if ((offset + erase_size) > fa->fa_size) {
        return false;
    }

    if (flash_area_erase(fa, offset, erase_size)) {
        return false;
    }

    return true;
}

bool
memfault_platform_coredump_storage_write(uint32_t offset, const void *data,
                                         size_t data_len)
{
    const struct flash_area *fa;

    if (prv_flash_open(&fa)) {
        return false;
    }

    if ((offset + data_len) > fa->fa_size) {
        return false;
    }

    if (flash_area_write(fa, offset, data, data_len)) {
        return false;
    }

    return true;
}

void
memfault_platform_coredump_storage_clear(void)
{
    const struct flash_area *fa;

    if (prv_flash_open(&fa)) {
        assert(0);
    }

    /* Erasing whole area takes too much time and causes
     * ble connection to time out. Instead erase only
     * the magic value.
     */
    if (flash_area_erase(fa, 0, sizeof(uint32_t))) {
        assert(0);
    }
}
