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

#include <assert.h>
#include <ctype.h>
#include <os/endian.h>
#include <bsp/bsp.h>
#include <sysflash/sysflash.h>
#include <hal/hal_gpio.h>
#include <os/util.h>
#include <tusb.h>
#include <class/msc/msc.h>
#include <class/msc/msc_device.h>
#include <tinyusb/tinyusb.h>
#include <msc_fat_view/msc_fat_view.h>
#include <img_mgmt/img_mgmt.h>
#include <bootutil/image.h>
#include <modlog/modlog.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include "coredump_files.h"

#if MYNEWT_VAL(BOOT_LOADER)
#define BOOT_LOADER     1
#define FLASH_AREA_IMAGE FLASH_AREA_IMAGE_0
#else
#define BOOT_LOADER     0
#ifdef FLASH_AREA_IMAGE_1
#define FLASH_AREA_IMAGE FLASH_AREA_IMAGE_1
#endif
#endif

static const char readme_text[] = "This device runs "
                                  MYNEWT_VAL(MSC_FAT_VIEW_DEFAULT_README_APP_NAME)  " on "
                                  MYNEWT_VAL(BSP_NAME);

#if defined MYNEWT_VAL_REPO_VERSION_APACHE_MYNEWT_CORE
#define REPO_VERSION_APACHE_MYNEWT_CORE     MYNEWT_VAL(REPO_VERSION_APACHE_MYNEWT_CORE)
#else
#define REPO_VERSION_APACHE_MYNEWT_CORE     "unknown"
#endif

#if defined MYNEWT_VAL_REPO_HASH_APACHE_MYNEWT_CORE
#define REPO_HASH_APACHE_MYNEWT_CORE        MYNEWT_VAL(REPO_HASH_APACHE_MYNEWT_CORE)
#else
#define REPO_HASH_APACHE_MYNEWT_CORE        ""
#endif

#if defined MYNEWT_VAL_REPO_VERSION_APACHE_MYNEWT_NIMBLE
#define REPO_VERSION_APACHE_MYNEWT_NIMBLE   MYNEWT_VAL(REPO_VERSION_APACHE_MYNEWT_NIMBLE)
#else
#define REPO_VERSION_APACHE_MYNEWT_NIMBLE   "unknown"
#endif

#if defined MYNEWT_VAL_REPO_HASH_APACHE_MYNEWT_NIMBLE
#define REPO_HASH_APACHE_MYNEWT_NIMBLE      MYNEWT_VAL(REPO_HASH_APACHE_MYNEWT_NIMBLE)
#else
#define REPO_HASH_APACHE_MYNEWT_NIMBLE      ""
#endif

#if defined MYNEWT_VAL_REPO_VERSION_APACHE_MYNEWT_MCUMGR
#define REPO_VERSION_APACHE_MYNEWT_MCUMGR   MYNEWT_VAL(REPO_VERSION_APACHE_MYNEWT_MCUMGR)
#else
#define REPO_VERSION_APACHE_MYNEWT_MCUMGR   "unknown"
#endif

#if defined MYNEWT_VAL_REPO_HASH_APACHE_MYNEWT_MCUMGR
#define REPO_HASH_APACHE_MYNEWT_MCUMGR      MYNEWT_VAL(REPO_HASH_APACHE_MYNEWT_MCUMGR)
#else
#define REPO_HASH_APACHE_MYNEWT_MCUMGR      ""
#endif

#if defined MYNEWT_VAL_REPO_VERSION_TINYUSB
#define REPO_VERSION_TINYUSB                MYNEWT_VAL(REPO_VERSION_TINYUSB)
#else
#define REPO_VERSION_TINYUSB                "unknown"
#endif

#if defined MYNEWT_VAL_REPO_HASH_TINYUSB
#define REPO_HASH_TINYUSB                   MYNEWT_VAL(REPO_HASH_TINYUSB)
#else
#define REPO_HASH_TINYUSB                   ""
#endif

static int
readme_create_content(struct MemFile *file)
{
    struct image_header image_header;
    const struct flash_area *fa;

    fwrite(readme_text, 1, sizeof(readme_text) - 1, &file->file);

    if (MYNEWT_VAL(MSC_FAT_VIEW_DEFAULT_README_VERSION) && 0 == flash_area_open(FLASH_AREA_IMAGE_0, &fa)) {
        flash_area_read(fa, 0, &image_header, sizeof(image_header));
        if (image_header.ih_magic == IMAGE_MAGIC) {
            fprintf(&file->file, "\n\nApp version: %u.%u.%u.%u\n", image_header.ih_ver.iv_major,
                    image_header.ih_ver.iv_minor,
                    image_header.ih_ver.iv_revision,
                    (unsigned)image_header.ih_ver.iv_build_num);
        }
        flash_area_close(fa);
    }

    if (MYNEWT_VAL(MSC_FAT_VIEW_DEFAULT_README_INCLUDE_HASHES)) {
        fprintf(&file->file, "\n\nmynewt: " REPO_VERSION_APACHE_MYNEWT_CORE " " REPO_HASH_APACHE_MYNEWT_CORE);
        fprintf(&file->file, "\nnimble: " REPO_VERSION_APACHE_MYNEWT_NIMBLE " " REPO_HASH_APACHE_MYNEWT_NIMBLE);
        fprintf(&file->file, "\nmcumgr: " REPO_VERSION_APACHE_MYNEWT_MCUMGR " " REPO_HASH_APACHE_MYNEWT_MCUMGR);
        fprintf(&file->file, "\ntinyusb: " REPO_VERSION_TINYUSB " " REPO_HASH_TINYUSB);
    }
    if (MYNEWT_VAL(MSC_FAT_VIEW_HUGE_FILE)) {
        fprintf(&file->file, "\n\n'Huge file' can be used to verify USB performance.\n");
    }

#ifdef FLASH_AREA_IMAGE
    fprintf(&file->file, "\n\nNew firmware can be copied to this drive (drag-drop .img file to upgrade device).\n");
#endif

    return file->bytes_written;
}

static uint32_t
readme_size(const file_entry_t *file_entry)
{
    (void)file_entry;
    struct MemFile empty_mem_file;
    uint32_t len;

    fmemopen_w(&empty_mem_file, (char *)NULL, 0);
    len = readme_create_content(&empty_mem_file);

    return len;
}

static void
readme_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    struct MemFile sector_file;
    int written = 0;
    (void)entry;

    MSC_FAT_VIEW_LOG_DEBUG("Readme read %d\n", file_sector);
    if (file_sector == 0) {
        fmemopen_w(&sector_file, (char *)buffer, 512);
        readme_create_content(&sector_file);
        written = sector_file.bytes_written;
    }

    memset(buffer + written, 0, 512 - written);
}

ROOT_DIR_ENTRY(readme, "README.TXT", FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY, readme_size, readme_read, NULL, NULL);

