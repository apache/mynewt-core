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
#include <bsp/bsp.h>
#include <sysflash/sysflash.h>
#include <tusb.h>
#include <class/msc/msc.h>
#include <msc_fat_view/msc_fat_view.h>
#include <img_mgmt/img_mgmt.h>
#include <bootutil/image.h>
#include <modlog/modlog.h>
#include <hal/hal_flash.h>

#if MYNEWT_VAL(BOOT_LOADER)
#define BOOT_LOADER     1
#define FLASH_AREA_IMAGE FLASH_AREA_IMAGE_0
#else
#define BOOT_LOADER     0
#ifdef FLASH_AREA_IMAGE_1
#define FLASH_AREA_IMAGE FLASH_AREA_IMAGE_1
#endif
#endif

/* If true, test image will be confirmed on root directory read */
static bool confirmed;

typedef enum {
    MEDIUM_NOT_PRESENT,
    REPORT_MEDIUM_CHANGE,
    MEDIUM_RELOAD,
    MEDIUM_PRESENT,
} medium_state_t;

static medium_state_t medium_state;

struct unallocated_write {
    uint32_t first_sector;
    uint32_t last_sector;
    enum {
        NOT_TOUCHED_YET = 0,
        WRITE_IN_PROGRESS = 1,
        NOT_AN_IMAGE = -1,
        CURRENT_IMAGE_NOT_CONFIRMED = -2,
        WRITE_EXCEEDED_SPACE = -3,
        WRITE_NOT_IN_SEQUENCE = -4,
    } write_status;
} unallocated_write;

static int write_status;
static const char *write_result_text[] = {
    "File that was written was not a valid image.",
    "Current image not confirmed, new image rejected.",
    "File write error.",
};

static uint32_t
flash_result_create_content(struct MemFile *file)
{
    int ix = abs(write_status) - 1;
    if (ix > 2) {
        ix = 2;
    }
    fwrite(write_result_text[ix], 1, strlen(write_result_text[ix]), &file->file);

    return file->bytes_written;
}

static uint32_t
flash_result_size(const file_entry_t *file_entry)
{
    struct MemFile sector_file;

    (void)file_entry;
    fmemopen_w(&sector_file, (char *)NULL, 0);

    flash_result_create_content(&sector_file);

    return sector_file.bytes_written;
}

static void
flash_result_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    struct MemFile sector_file;
    int written = 0;
    (void)entry;

    if (file_sector == 0) {
        fmemopen_w(&sector_file, (char *)buffer, 512);
        flash_result_create_content(&sector_file);
        written = sector_file.bytes_written;
    }

    memset(buffer + written, 0, 512 - written);
}

static const file_entry_t flash_result = {
    .name = "Write error.txt",
    .attributes = FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY,
    .size = flash_result_size,
    .read_sector = flash_result_read,
};

static int
image_write_sector(struct msc_fat_view_write_handler *handler, uint32_t sector, uint8_t *buffer)
{
#ifdef FLASH_AREA_IMAGE
    const struct flash_area *fa;
    uint32_t write_offset;
    int rc;

    if (unallocated_write.write_status < 0) {
        return 0;
    }
    flash_area_open(FLASH_AREA_IMAGE, &fa);
    if (unallocated_write.write_status == NOT_TOUCHED_YET) {
        if (BOOT_LOADER) {
            if (((struct image_header *)buffer)->ih_magic == IMAGE_MAGIC) {
                unallocated_write.write_status = WRITE_IN_PROGRESS;
                /* TODO: unmount and add error file */

            }
        } else if ((img_mgmt_state_flags(0) & IMG_MGMT_STATE_F_CONFIRMED) == 0) {
            MSC_FAT_VIEW_LOG_ERROR("Image not confirmed, write rejected\n");

            /* Image in slot 0 not confirmed, do not write */
            unallocated_write.write_status = CURRENT_IMAGE_NOT_CONFIRMED;
        } else if (((struct image_header *)buffer)->ih_magic == IMAGE_MAGIC) {
            unallocated_write.write_status = WRITE_IN_PROGRESS;
        }
        if (unallocated_write.write_status == WRITE_IN_PROGRESS) {
            MSC_FAT_VIEW_LOG_INFO("Image writing detected\n");
            unallocated_write.first_sector = sector;
            unallocated_write.last_sector = sector;
        }
    } else if (unallocated_write.write_status == WRITE_IN_PROGRESS) {
        if (sector != unallocated_write.last_sector + 1) {
            /*
             * Unallocated space written without order, code will not be able to used
             * it sensible.
             */
            unallocated_write.write_status = WRITE_NOT_IN_SEQUENCE;
            MSC_FAT_VIEW_LOG_ERROR("Not continuous writes to unallocated space rejected\n");
        }
    }
    if (unallocated_write.write_status == WRITE_IN_PROGRESS) {
        write_offset = (sector - unallocated_write.first_sector) * SECTOR_SIZE;
        if (!hal_flash_isempty_no_buf(fa->fa_device_id, fa->fa_off + write_offset, SECTOR_SIZE)) {
            flash_area_erase(fa, write_offset, SECTOR_SIZE);
        }

        if ((rc = flash_area_write(fa, write_offset, buffer, SECTOR_SIZE)) < 0) {
            MSC_FAT_VIEW_LOG_ERROR("Flash write error, following writes will be rejected %d 0x%08x\n",
                                   rc, fa->fa_off + write_offset);
            unallocated_write.write_status = WRITE_EXCEEDED_SPACE;
        }
    }
    flash_area_close(fa);
    if (unallocated_write.write_status == WRITE_IN_PROGRESS) {
        unallocated_write.last_sector = sector;
    }
    return 512;
#else
    return 0;
#endif
}

static int
image_file_written(struct msc_fat_view_write_handler *handler, uint32_t size,
                   uint32_t sector, bool first_sector)
{
    struct image_version version;
    uint32_t flags;

    if (unallocated_write.write_status == WRITE_IN_PROGRESS) {
        if (unallocated_write.first_sector == sector && first_sector) {
            MSC_FAT_VIEW_LOG_INFO("New file detected\n");
            if (BOOT_LOADER) {
                img_mgmt_state_confirm();
                hal_system_reset();
            } else {
                if (img_mgmt_read_info(1, &version, NULL, &flags) == 0) {
                    MSC_FAT_VIEW_LOG_INFO("New image OK, resetting\n");
                    img_mgmt_state_set_pending(1, 0);
                    hal_system_reset();
                } else {
                    MSC_FAT_VIEW_LOG_ERROR("New file not an valid image\n");
                }
            }
        } else {
            MSC_FAT_VIEW_LOG_ERROR(
                "New file not ready to flash new sectors (%d-%d), (sector %d)\n",
                unallocated_write.first_sector, unallocated_write.last_sector, sector);
        }
    } else {
        if ((int)unallocated_write.write_status < 0) {
            MSC_FAT_VIEW_LOG_ERROR("Write failed, reloading medium\n");
            medium_state = MEDIUM_RELOAD;
        }
    }
    if (unallocated_write.write_status < 0) {
        write_status = unallocated_write.write_status;
        msc_fat_view_add_dir_entry(&flash_result);
    }
    unallocated_write.write_status = NOT_TOUCHED_YET;
    return 0;
}

MSC_FAT_VIEW_WRITE_HANDLER(update_handler, image_write_sector, image_file_written);

static int
invalid_fun(const struct file_entry *entry)
{
    (void)entry;

    /* Image auto-confirmation enabled */
    if (MYNEWT_VAL(MSC_FAT_VIEW_AUTOCONFIRM)) {
        /* Image not confirmed yet or confirmation state not checked */
        if (!confirmed) {
            /* Image actually not confirmed yet */
            if ((img_mgmt_state_flags(0) & IMG_MGMT_STATE_F_CONFIRMED) == 0) {
                /* Mark as confirmed */
                img_mgmt_state_confirm();
                confirmed = true;
            }
        }
    }

    /* Return MSC_FAT_VIEW_FILE_ENTRY_NOT_VALID to prevent from adding entry */
    return MSC_FAT_VIEW_FILE_ENTRY_NOT_VALID;
}

ROOT_DIR_ENTRY(ghost_entry, "", 0, NULL, NULL, NULL, NULL, invalid_fun);
