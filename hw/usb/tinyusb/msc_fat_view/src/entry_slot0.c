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
slot0_img_size(const file_entry_t *file)
{
    struct image_header image_header;
    struct image_tlv_info tlv_info;
    uint32_t size = 0;
    const struct flash_area *fa;
    (void)file;

    if (0 == flash_area_open(FLASH_AREA_IMAGE_0, &fa)) {
        flash_area_read(fa, 0, &image_header, sizeof(image_header));
        size = image_header.ih_img_size + image_header.ih_hdr_size;
        flash_area_read(fa, size, &tlv_info, sizeof(tlv_info));
        if (tlv_info.it_magic == IMAGE_TLV_INFO_MAGIC) {
            size += tlv_info.it_tlv_tot;
        }
        flash_area_close(fa);
    }

    return size;
}

static void
slot0_img_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    const struct flash_area *fa;
    uint32_t addr;
    (void)entry;

    if (0 == flash_area_open(FLASH_AREA_IMAGE_0, &fa)) {
        addr = file_sector * 512;
        flash_area_read(fa, addr, buffer, 512);
        flash_area_close(fa);
    }
}

ROOT_DIR_ENTRY(slot0, "FIRMWARE.IMG", FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY, slot0_img_size, slot0_img_read, NULL, NULL, NULL);
