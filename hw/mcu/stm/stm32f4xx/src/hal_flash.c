/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include "stm32f4xx/stm32f4xx_hal_def.h"
#include "stm32f4xx/stm32f4xx_hal_flash.h"
#include "stm32f4xx/stm32f4xx_hal_flash_ex.h"
#include "hal/hal_flash.h"

static const struct flash_area_desc {
    uint32_t fad_offset;
    uint32_t fad_length;
    int fad_sector_id;
} flash_area_descs[] = {
    { 0x08000000, 16 * 1024, FLASH_SECTOR_0 },
    { 0x08004000, 16 * 1024, FLASH_SECTOR_1 },
    { 0x08008000, 16 * 1024, FLASH_SECTOR_2 },
    { 0x0800c000, 16 * 1024, FLASH_SECTOR_3 },
    { 0x08010000, 64 * 1024, FLASH_SECTOR_4 },
    { 0x08020000, 128 * 1024, FLASH_SECTOR_5 },
    { 0x08040000, 128 * 1024, FLASH_SECTOR_6 },
    { 0x08060000, 128 * 1024, FLASH_SECTOR_7 },
    { 0x08080000, 128 * 1024, FLASH_SECTOR_8 },
    { 0x080a0000, 128 * 1024, FLASH_SECTOR_9 },
    { 0x080c0000, 128 * 1024, FLASH_SECTOR_10 },
    { 0x080e0000, 128 * 1024, FLASH_SECTOR_11 },
};

#define FLASH_NUM_AREAS   (int)(sizeof flash_area_descs /       \
                                sizeof flash_area_descs[0])

int
flash_read(uint32_t address, void *dst, uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

int
flash_write(uint32_t address, const void *src, uint32_t num_bytes)
{
    const uint8_t *sptr;
    uint32_t i;
    int rc;

    sptr = src;
    for (i = 0; i < num_bytes; i++) {
        rc = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address, sptr[i]);
        if (rc != 0) {
            return rc;
        }

        address++;
    }

    return 0;
}

static void
flash_erase_sector_id(int sector_id)
{
    FLASH_Erase_Sector(sector_id, FLASH_VOLTAGE_RANGE_1);
}

int
flash_erase_sector(uint32_t sector_address)
{
    int i;

    for (i = 0; i < FLASH_NUM_AREAS; i++) {
        if (flash_area_descs[i].fad_offset == sector_address) {
            flash_erase_sector_id(flash_area_descs[i].fad_sector_id);
            return 0;
        }
    }

    return -1;
}

int
flash_erase(uint32_t address, uint32_t num_bytes)
{
    const struct flash_area_desc *area;
    uint32_t end;
    int i;

    end = address + num_bytes;

    for (i = 0; i < FLASH_NUM_AREAS; i++) {
        area = flash_area_descs + i;

        if (area->fad_offset >= end) {
            return 0;
        }

        if (address >= area->fad_offset &&
            address < area->fad_offset + area->fad_length) {

            flash_erase_sector_id(area->fad_sector_id);
        }
    }

    return 0;
}

int
flash_init(void)
{
    int rc;

    rc = HAL_FLASH_Unlock();
    if (rc != 0) {
        return -1;
    }

    return 0;
}
