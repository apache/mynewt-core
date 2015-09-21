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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include "hal/hal_flash.h"

static FILE *file;

static const struct flash_area_desc {
    uint32_t fad_offset;
    uint32_t fad_length;
    uint32_t fad_sector_id;
} flash_area_descs[] = {
    { 0x00000000, 16 * 1024,  0 },
    { 0x00004000, 16 * 1024,  1 },
    { 0x00008000, 16 * 1024,  2 },
    { 0x0000c000, 16 * 1024,  3 },
    { 0x00010000, 64 * 1024,  4 },
    { 0x00020000, 128 * 1024, 5 },
    { 0x00040000, 128 * 1024, 6 },
    { 0x00060000, 128 * 1024, 7 },
    { 0x00080000, 128 * 1024, 8 },
    { 0x000a0000, 128 * 1024, 9 },
    { 0x000c0000, 128 * 1024, 10 },
    { 0x000e0000, 128 * 1024, 11 },
};

#define FLASH_NUM_AREAS   (int)(sizeof flash_area_descs /       \
                                sizeof flash_area_descs[0])

static void
flash_native_erase(uint32_t addr, uint32_t len)
{
    static uint8_t buf[256];
    uint32_t end;
    int chunk_sz;
    int rc;

    end = addr + len;
    memset(buf, 0xff, sizeof buf);

    rc = fseek(file, addr, SEEK_SET);
    assert(rc == 0);

    while (addr < end) {
        if (end - addr < sizeof buf) {
            chunk_sz = end - addr;
        } else {
            chunk_sz = sizeof buf;
        }
        rc = fwrite(buf, chunk_sz, 1, file);
        assert(rc == 1);

        addr += chunk_sz;
    }
    fflush(file);
}

static void
flash_native_ensure_file_open(void)
{
    int expected_sz;
    int i;

    if (file == NULL) {
        expected_sz = 0;
        for (i = 0; i < FLASH_NUM_AREAS; i++) {
            expected_sz += flash_area_descs[i].fad_length;
        }

        file = tmpfile();
        assert(file != NULL);
        flash_native_erase(0, expected_sz);
    }
}

static int
flash_native_write_internal(uint32_t address, const void *src, uint32_t length,
                            int allow_overwrite)
{
    static uint8_t buf[256];
    uint32_t cur;
    uint32_t end;
    int chunk_sz;
    int rc;
    int i;

    if (length == 0) {
        return 0;
    }

    end = address + length;

    flash_native_ensure_file_open();

    fseek(file, address, SEEK_SET);

    cur = address;
    while (cur < end) {
        if (end - cur < sizeof buf) {
            chunk_sz = end - cur;
        } else {
            chunk_sz = sizeof buf;
        }

        /* Ensure data is not being overwritten. */
        if (!allow_overwrite) {
            rc = flash_read(cur, buf, chunk_sz);
            assert(rc == 0);
            for (i = 0; i < chunk_sz; i++) {
                assert(buf[i] == 0xff);
            }
        }

        cur += chunk_sz;
    }

    fseek(file, address, SEEK_SET);
    rc = fwrite(src, length, 1, file);
    assert(rc == 1);

    fflush(file);
    return 0;
}

int
flash_write(uint32_t address, const void *src, uint32_t length)
{
    return flash_native_write_internal(address, src, length, 0);
}

int
flash_native_overwrite(uint32_t address, const void *src, uint32_t length)
{
    return flash_native_write_internal(address, src, length, 1);
}

int
flash_native_memset(uint32_t offset, uint8_t c, uint32_t len)
{
    uint8_t buf[256];
    int chunk_sz;
    int rc;

    memset(buf, c, sizeof buf);

    while (len > 0) {
        if (len > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = len;
        }

        rc = flash_native_overwrite(offset, buf, chunk_sz);
        if (rc != 0) {
            return rc;
        }

        offset += chunk_sz;
        len -= chunk_sz;
    }

    return 0;
}

int
flash_read(uint32_t address, void *dst, uint32_t length)
{
    flash_native_ensure_file_open();
    fseek(file, address, SEEK_SET);
    fread(dst, length, 1, file);

    return 0;
}

static int
find_area(uint32_t address)
{
    int i;

    for (i = 0; i < FLASH_NUM_AREAS; i++) {
        if (flash_area_descs[i].fad_offset == address) {
            return i;
        }
    }

    return -1;
}

int
flash_erase_sector(uint32_t sector_address)
{
    int next_sector_id;
    int sector_id;
    int area_id;

    flash_native_ensure_file_open();

    area_id = find_area(sector_address);
    if (area_id == -1) {
        return -1;
    }

    sector_id = flash_area_descs[area_id].fad_sector_id;
    while (1) {
        flash_native_erase(sector_address,
                           flash_area_descs[area_id].fad_length);

        area_id++;
        if (area_id >= FLASH_NUM_AREAS) {
            break;
        }

        next_sector_id = flash_area_descs[area_id].fad_sector_id;
        if (next_sector_id != sector_id) {
            break;
        }

        sector_id = next_sector_id;
    }

    return 0;
}

int
flash_erase(uint32_t address, uint32_t num_bytes)
{
    const struct flash_area_desc *area;
    uint32_t end;
    int i;

    flash_native_ensure_file_open();

    end = address + num_bytes;

    for (i = 0; i < FLASH_NUM_AREAS; i++) {
        area = flash_area_descs + i;

        if (area->fad_offset >= end) {
            return 0;
        }

        if (address >= area->fad_offset &&
            address < area->fad_offset + area->fad_length) {

            flash_native_erase(area->fad_offset, area->fad_length);
        }
    }

    return 0;
}

int
flash_init(void)
{
    return 0;
}

