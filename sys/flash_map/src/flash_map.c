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

#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "syscfg/syscfg.h"
#include "sysinit/sysinit.h"
#include "sysflash/sysflash.h"
#include "defs/error.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "hal/hal_flash_int.h"
#include "flash_map/flash_map.h"

const struct flash_area *flash_map;
int flash_map_entries;

int
flash_area_open(uint8_t id, const struct flash_area **fap)
{
    const struct flash_area *area;
    int i;

    if (flash_map == NULL) {
        return EACCES;
    }

    for (i = 0; i < flash_map_entries; i++) {
        area = flash_map + i;
        if (area->fa_id == id) {
            *fap = area;
            return 0;
        }
    }

    return ENOENT;
}

void
flash_area_close(const struct flash_area *fa)
{
    /* nothing to do for now */
}

int
flash_area_to_sectors(int id, int *cnt, struct flash_area *ret)
{
    const struct flash_area *fa;
    const struct hal_flash *hf;
    uint32_t start;
    uint32_t size;
    int rc;
    int i;

    rc = flash_area_open(id, &fa);
    if (rc != 0) {
        return rc;
    }

    *cnt = 0;

    hf = bsp_flash_dev(fa->fa_device_id);
    for (i = 0; i < hf->hf_sector_cnt; i++) {
        hf->hf_itf->hff_sector_info(i, &start, &size);
        if (start >= fa->fa_off && start < fa->fa_off + fa->fa_size) {
            if (ret) {
                ret->fa_device_id = fa->fa_device_id;
                ret->fa_off = start;
                ret->fa_size = size;
                ret++;
            }
            (*cnt)++;
        }
    }
    return 0;
}

int
flash_area_read(const struct flash_area *fa, uint32_t off, void *dst,
    uint32_t len)
{
    if (off > fa->fa_size || off + len > fa->fa_size) {
        return -1;
    }
    return hal_flash_read(fa->fa_device_id, fa->fa_off + off, dst, len);
}

int
flash_area_write(const struct flash_area *fa, uint32_t off, void *src,
    uint32_t len)
{
    if (off > fa->fa_size || off + len > fa->fa_size) {
        return -1;
    }
    return hal_flash_write(fa->fa_device_id, fa->fa_off + off, src, len);
}

int
flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    if (off > fa->fa_size || off + len > fa->fa_size) {
        return -1;
    }
    return hal_flash_erase(fa->fa_device_id, fa->fa_off + off, len);
}

uint8_t
flash_area_align(const struct flash_area *fa)
{
    return hal_flash_align(fa->fa_device_id);
}

int
flash_area_id_from_image_slot(int slot)
{
    switch (slot) {
    case 0:
        return FLASH_AREA_IMAGE_0;
    case 1:
        return FLASH_AREA_IMAGE_1;
    default:
        assert(0);
        return FLASH_AREA_IMAGE_0;
    }
}

void
flash_map_init(void)
{
    int rc;

    rc = hal_flash_init();
    SYSINIT_PANIC_ASSERT(rc == 0);

    /* XXX: Attempt to read from meta region; for now we always use default
     * map
     */

    flash_map = sysflash_map_dflt;
    flash_map_entries = sizeof sysflash_map_dflt / sizeof sysflash_map_dflt[0];
}
