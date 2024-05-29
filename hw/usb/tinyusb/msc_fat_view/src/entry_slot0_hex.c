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

#include <stdio.h>
#include <sysflash/sysflash.h>
#include <msc_fat_view/msc_fat_view.h>
#include <bootutil/image.h>

static uint32_t
slot0_hex_size(const file_entry_t *file)
{
    uint32_t size = 0;
    const struct flash_area *fa;
    (void)file;

    if (0 == flash_area_open(FLASH_AREA_IMAGE_0, &fa)) {
        size = fa->fa_size * 4;
        flash_area_close(fa);
    }

    return size;
}

static uint8_t
hex_digit(int v)
{
    v &= 0xF;

    return (v < 10) ? (v + '0') : (v + 'A' - 10);
}

static void
slot0_hex_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    const struct flash_area *fa;
    int i;
    int j;
    int k;
    uint32_t addr = (file_sector * 512) / 4;
    uint32_t addr_buf;
    (void)entry;

    if (0 == flash_area_open(FLASH_AREA_IMAGE_0, &fa)) {
        flash_area_read(fa, addr, buffer, 512 / 4);
        flash_area_close(fa);
        for (i = 512, j = 128; i > 0;) {
            buffer[--i] = '\n';
            buffer[--i] = '\r';
            for (k = 0; k < 16; ++k) {
                buffer[--i] = hex_digit(buffer[--j]);
                buffer[--i] = hex_digit(buffer[j] >> 4);
                buffer[--i] = ' ';
            }
            for (k = 0; k < 5; ++k) {
                buffer[--i] = ' ';
            }
            buffer[--i] = ':';
            addr_buf = addr + j;
            for (k = 0; k < 8; ++k) {
                buffer[--i] = hex_digit(addr_buf);
                addr_buf >>= 4;
            }
        }
    }
}

ROOT_DIR_ENTRY(slot0_hex, "SLOT0.HEX", FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY, slot0_hex_size, slot0_hex_read, NULL, NULL, NULL);
