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

#include <os/os.h>
#include <bsp/bsp.h>
#include <sysflash/sysflash.h>
#include <msc_fat_view/msc_fat_view.h>
#include <coredump/coredump.h>

#if MYNEWT_VAL(OS_COREDUMP) && MYNEWT_VAL(MSC_FAT_VIEW_COREDUMP_FILES)
/* If OS_COREDUMP is enabled use flash area ID used for dumps */
#define COREDUMP1 MYNEWT_VAL(COREDUMP_FLASH_AREA)
#else
#define COREDUMP1 0
#endif

/* Second core dump can be generated on different core so it can be
 * enabled even if OS_COREDUMP is not turned on.
 */
#if MYNEWT_VAL(MSC_FAT_VIEW_COREDUMP_FILES) && MYNEWT_VAL(MSC_FAT_VIEW_COREDUMP2_FLASH_AREA) >= 0
#define COREDUMP2 MYNEWT_VAL(MSC_FAT_VIEW_COREDUMP2_FLASH_AREA)
#else
#define COREDUMP2 0
#endif

typedef struct coredump_entry {
    file_entry_t file_entry;
    uint32_t core_size;
    const struct flash_area *flash_area;
} coredump_entry_t;

static uint32_t
coredump_size(const file_entry_t *file_entry)
{
    coredump_entry_t *core_entry = (coredump_entry_t *)file_entry;

    return core_entry->core_size;
}

static void
coredump_read(const struct file_entry *file_entry, uint32_t file_sector, uint8_t buffer[512])
{
    coredump_entry_t *core_entry = (coredump_entry_t *)file_entry;
    const struct flash_area *fa = core_entry->flash_area;

    flash_area_read(fa, file_sector * 512, buffer, 512);
}

static void
coredump_delete(const struct file_entry *file_entry)
{
    coredump_entry_t *core_entry = (coredump_entry_t *)file_entry;
    const struct flash_area *fa = core_entry->flash_area;

    flash_area_erase(fa, 0, core_entry->core_size);
}

static struct coredump_entry coredump1 = {
    .file_entry = {
        .name = MYNEWT_VAL(MSC_FAT_VIEW_COREDUMP_FILE_NAME),
        .attributes = 0,
        .size = coredump_size,
        .read_sector = coredump_read,
        .delete_entry = coredump_delete,
    }
};

static struct coredump_entry coredump2 = {
    .file_entry = {
        .name = MYNEWT_VAL(MSC_FAT_VIEW_COREDUMP2_FILE_NAME),
        .attributes = 0,
        .size = coredump_size,
        .read_sector = coredump_read,
        .delete_entry = coredump_delete,
    }
};

static const struct flash_area *
coredump_area(int flash_area_id, struct coredump_header *hdr)
{
    const struct flash_area *fa;
    int rc;

    rc = flash_area_open(flash_area_id, &fa);
    if (rc != 0) {
        return NULL;
    }

    rc = flash_area_read(fa, 0, hdr, sizeof(*hdr));
    if (rc != 0) {
        return NULL;
    }

    if (hdr->ch_magic != COREDUMP_MAGIC) {
        return NULL;
    }

    return fa;
}


void
msc_fat_view_coredump_pkg_init(void)
{
    const struct flash_area *fa;
    struct coredump_header hdr;

    if (COREDUMP1) {
        fa = coredump_area(COREDUMP1, &hdr);
        if (fa) {
            coredump1.flash_area = fa;
            coredump1.core_size = hdr.ch_size;
        }
    }
    if (COREDUMP2) {
        fa = coredump_area(COREDUMP2, &hdr);
        if (fa) {
            coredump2.flash_area = fa;
            coredump2.core_size = hdr.ch_size;
        }
    }
}

void
msc_fat_view_add_coredumps(void)
{
    if (COREDUMP1 && coredump1.core_size > 0) {
        msc_fat_view_add_dir_entry(&coredump1.file_entry);
    }
    if (COREDUMP2 && coredump2.core_size > 0) {
        msc_fat_view_add_dir_entry(&coredump2.file_entry);
    }
}
