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

#define BUF_SIZE   32

static uint32_t
bootloader_size(const file_entry_t *file)
{
    static uint32_t size = 0;
    const struct flash_area *fa;
    uint32_t offset;
    int buf_size = BUF_SIZE;
    uint8_t buf[BUF_SIZE];
    (void)file;

    if (size == 0) {
        if (0 == flash_area_open(FLASH_AREA_BOOTLOADER, &fa)) {
            offset = FLASH_AREA_BOOTLOADER_SIZE - BUF_SIZE;
            while (offset > 0 && buf_size > 2) {
                if (!flash_area_read_is_empty(fa, offset, buf, buf_size)) {
                    buf_size /= 2;
                    offset += buf_size;
                } else {
                    offset -= buf_size;
                }
            }
            size = offset + buf_size;

            flash_area_close(fa);
        }
    }

    return size;
}

static void
bootlaoder_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    const struct flash_area *fa;
    uint32_t addr;
    (void)entry;

    if (0 == flash_area_open(FLASH_AREA_BOOTLOADER, &fa)) {
        addr = file_sector * 512;
        flash_area_read(fa, addr, buffer, 512);
        flash_area_close(fa);
    }
}

ROOT_DIR_ENTRY(boot_bin, "BOOT.BIN", FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY, bootloader_size,
               bootlaoder_read, NULL, NULL, NULL);
