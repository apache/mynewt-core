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

#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "os/mynewt.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "hal/hal_flash_int.h"
#include "mfg/mfg.h"
#include "flash_map/flash_map.h"

const struct flash_area *flash_map;
int flash_map_entries;

int
flash_area_open(uint8_t id, const struct flash_area **fap)
{
    const struct flash_area *area;
    int i;

    if (flash_map == NULL) {
        return SYS_EACCES;
    }

    for (i = 0; i < flash_map_entries; i++) {
        area = flash_map + i;
        if (area->fa_id == id) {
            *fap = area;
            return 0;
        }
    }

    return SYS_ENOENT;
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

    hf = hal_bsp_flash_dev(fa->fa_device_id);
    for (i = 0; i < hf->hf_sector_cnt; i++) {
        hf->hf_itf->hff_sector_info(hf, i, &start, &size);
        if (start >= fa->fa_off && start < fa->fa_off + fa->fa_size) {
            if (ret) {
                ret->fa_id = id;
                ret->fa_device_id = fa->fa_device_id;
                ret->fa_off = start;
                ret->fa_size = size;
                ret++;
            }
            (*cnt)++;
        }
    }
    flash_area_close(fa);
    return 0;
}

int
flash_area_getnext_sector(int id, int *sec_id, struct flash_area *ret)
{
    const struct flash_area *fa;
    const struct hal_flash *hf;
    uint32_t start;
    uint32_t size;
    int rc;
    int i;

    rc = flash_area_open(id, &fa);
    if (rc) {
        return rc;
    }
    if (!ret || *sec_id < -1) {
        rc = SYS_EINVAL;
        goto end;
    }
    hf = hal_bsp_flash_dev(fa->fa_device_id);
    i = *sec_id + 1;
    for (; i < hf->hf_sector_cnt; i++) {
        hf->hf_itf->hff_sector_info(hf, i, &start, &size);
        if (start >= fa->fa_off && start < fa->fa_off + fa->fa_size) {
            ret->fa_id = id;
            ret->fa_device_id = fa->fa_device_id;
            ret->fa_off = start;
            ret->fa_size = size;
            *sec_id = i;
            rc = 0;
            goto end;
        }
    }
    rc = SYS_ENOENT;
end:
    flash_area_close(fa);
    return rc;
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
flash_area_write(const struct flash_area *fa, uint32_t off, const void *src,
    uint32_t len)
{
    if (off > fa->fa_size || off + len > fa->fa_size) {
        return -1;
    }
    return hal_flash_write(fa->fa_device_id, fa->fa_off + off,
                           (void *)src, len);
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

/**
 * Checks if a flash area has been erased. Returns false if there are any
 * non 0xFFFFFFFF bytes.
 *
 * @param fa                    An opened flash area to iterate.
 *                                  the count of flash area TLVs in the meta
 *                                  region is greater than this number, this
 *                                  function fails.
 * @param empty (out)           On success, the is_empty result gets written
 *                                  here.
 * @return                      0 on success; nonzero on failure.
 */
int
flash_area_is_empty(const struct flash_area *fa, bool *empty)
{
    uint32_t data[64 >> 2];
    uint32_t data_off = 0;
    int8_t bytes_to_read;
    uint8_t i;
    int rc;

    while (data_off < fa->fa_size) {
        bytes_to_read = min(64, fa->fa_size - data_off);
        rc = flash_area_read(fa, data_off, data, bytes_to_read);
        if (rc) {
            return rc;
        }
        for (i = 0; i < bytes_to_read >> 2; i++) {
          if (data[i] != (uint32_t) -1) {
            goto not_empty;
          }
        }
        data_off += bytes_to_read;
    }
    *empty = true;
    return 0;
not_empty:
    *empty = false;
    return 0;
}

/**
 * Converts the specified image slot index to a flash area ID.  If the
 * specified value is not a valid image slot index (0 or 1), a crash is
 * triggered.
 */
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

/**
 * Converts the specified flash area ID to an image slot index (0 or 1).  If
 * the area ID does not correspond to an image slot, -1 is returned.
 */
int
flash_area_id_to_image_slot(int area_id)
{
    switch (area_id) {
    case FLASH_AREA_IMAGE_0:
        return 0;
    case FLASH_AREA_IMAGE_1:
        return 1;
    default:
        return -1;
    }
}

/**
 * Reads the flash map layout from the manufacturing meta region.  This
 * function requires that the flash map be populated with a
 * FLASH_AREA_BOOTLOADER entry, as the meta region is stored at the end of the
 * boot loader area.
 *
 * @param max_areas             The maximum number of flash areas to read.  If
 *                                  the count of flash area TLVs in the meta
 *                                  region is greater than this number, this
 *                                  function fails.
 * @param out_areas (out)       An array of flash areas.  On success, the flash
 *                                  map stored in the meta region gets written
 *                                  here.
 * @param out_num_areas (out)   On success, the number of flash areas read gets
 *                                  written here.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
flash_map_read_mfg(int max_areas,
                   struct flash_area *out_areas, int *out_num_areas)
{
    struct mfg_meta_flash_area meta_flash_area;
    struct mfg_meta_tlv tlv;
    struct flash_area *fap;
    uint32_t off;
    int rc;

    *out_num_areas = 0;
    off = 0;

    /* Ensure manufacturing meta region has been located in flash. */
    rc = mfg_init();
    if (rc != 0) {
        return rc;
    }

    while (1) {
        if (*out_num_areas >= max_areas) {
            return -1;
        }

        rc = mfg_next_tlv_with_type(&tlv, &off, MFG_META_TLV_TYPE_FLASH_AREA);
        switch (rc) {
        case 0:
            break;
        case MFG_EDONE:
            return 0;
        default:
            return rc;
        }

        rc = mfg_read_tlv_flash_area(&tlv, off, &meta_flash_area);
        if (rc != 0) {
            return rc;
        }

        fap = out_areas + *out_num_areas;
        fap->fa_id = meta_flash_area.area_id;
        fap->fa_device_id = meta_flash_area.device_id;
        fap->fa_off = meta_flash_area.offset;
        fap->fa_size = meta_flash_area.size;

        (*out_num_areas)++;
    }
}

void
flash_map_init(void)
{
    static struct flash_area mfg_areas[MYNEWT_VAL(FLASH_MAP_MAX_AREAS)];

    int num_areas;
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = hal_flash_init();
    SYSINIT_PANIC_ASSERT(rc == 0);

    /* Use the hardcoded default flash map.  This is done for two reasons:
     * 1. A minimal flash map configuration is required to boot strap the
     *    process of reading the flash map from the manufacturing meta region.
     *    In particular, a FLASH_AREA_BOOTLOADER entry is required, as the meta
     *    region is located at the end of the boot loader area.
     * 2. If we fail to read the flash map from the meta region, the system
     *    continues to use the default flash map.
     */
    flash_map = sysflash_map_dflt;
    flash_map_entries = sizeof sysflash_map_dflt / sizeof sysflash_map_dflt[0];

    /* Attempt to read the flash map from the manufacturing meta region.  On
     * success, use the new flash map instead of the default hardcoded one.
     */
    rc = flash_map_read_mfg(sizeof mfg_areas / sizeof mfg_areas[0],
                            mfg_areas, &num_areas);
    if (rc == 0) {
        flash_map = mfg_areas;
        flash_map_entries = num_areas;
    }
}
