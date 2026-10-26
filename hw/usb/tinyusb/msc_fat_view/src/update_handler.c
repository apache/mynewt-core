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
#include <stdlib.h>
#include <string.h>
#include <bsp/bsp.h>
#include <sysflash/sysflash.h>
#include <tusb.h>
#include <class/msc/msc.h>
#include <msc_fat_view/msc_fat_view.h>
#include <msc_fat_view/readme.h>
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
    /* Last flash sector prepared for writing (-1 none yet) */
    int flash_sector_id;
    /* Offset in flash area up to which flash is ready for writing */
    uint32_t erased_limit;
    /* Offset in flash area at which image starts */
    uint32_t image_offset;
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

static void
flash_result_create_content(struct out_stream *out)
{
    int ix = abs(write_status) - 1;
    if (ix > 2) {
        ix = 2;
    }
    ostream_write_str(out, write_result_text[ix]);
}

static uint32_t
flash_result_size(const file_entry_t *file_entry)
{
    struct mem_out_stream mem;

    msc_fat_view_sector_stream_init(&mem, NULL, 0);
    flash_result_create_content((struct out_stream *)&mem);

    return mem.write_ptr;
}

static void
flash_result_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    struct mem_out_stream mem;

    memset(buffer, 0, 512);
    msc_fat_view_sector_stream_init(&mem, buffer, file_sector);
    flash_result_create_content((struct out_stream *)&mem);
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
    struct flash_area flash_sector;
    uint32_t write_offset;
    int rc;

    if (unallocated_write.write_status < 0) {
        return 0;
    }
    if (flash_area_open(FLASH_AREA_IMAGE, &fa)) {
        return 0;
    }
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
            unallocated_write.flash_sector_id = -1;
            unallocated_write.erased_limit = 0;
            unallocated_write.image_offset = 0;
            if (!BOOT_LOADER &&
                img_mgmt_upload_offset(&unallocated_write.image_offset) != 0) {
                unallocated_write.write_status = WRITE_EXCEEDED_SPACE;
            }
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
        write_offset = unallocated_write.image_offset +
                       (sector - unallocated_write.first_sector) * SECTOR_SIZE;
        /*
         * Flash sectors are usually bigger than disk sector. Erase each flash
         * sector (if needed) only once, when first disk sector falls into it.
         * Erasing it later would destroy data that was already written.
         */
        rc = 0;
        while (rc == 0 && write_offset + SECTOR_SIZE > unallocated_write.erased_limit) {
            rc = flash_area_getnext_sector(FLASH_AREA_IMAGE,
                                           &unallocated_write.flash_sector_id,
                                           &flash_sector);
            if (rc == 0) {
                if (!hal_flash_isempty_no_buf(flash_sector.fa_device_id,
                                              flash_sector.fa_off,
                                              flash_sector.fa_size)) {
                    rc = hal_flash_erase_sector(flash_sector.fa_device_id,
                                                flash_sector.fa_off);
                }
                unallocated_write.erased_limit =
                    flash_sector.fa_off + flash_sector.fa_size - fa->fa_off;
            }
        }

        if (rc != 0 ||
            (rc = flash_area_write(fa, write_offset, buffer, SECTOR_SIZE)) < 0) {
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

/*
 * Checks if whole image (including TLVs) was already written.
 * Host can update directory entry before all file data is written
 * (i.e. when file grows with each write), image must not be checked
 * till then.
 */
static bool
image_write_complete(void)
{
#ifdef FLASH_AREA_IMAGE
    const struct flash_area *fa;
    struct image_header hdr;
    struct image_tlv_info tlv_info;
    uint32_t written;
    uint32_t tlv_off;
    bool complete = false;

    written = (unallocated_write.last_sector - unallocated_write.first_sector + 1) *
              SECTOR_SIZE;
    if (flash_area_open(FLASH_AREA_IMAGE, &fa)) {
        return false;
    }
    if (flash_area_read(fa, unallocated_write.image_offset, &hdr, sizeof(hdr)) == 0) {
        tlv_off = hdr.ih_hdr_size + hdr.ih_img_size + hdr.ih_protect_tlv_size;
        if (tlv_off + sizeof(tlv_info) <= written &&
            flash_area_read(fa, unallocated_write.image_offset + tlv_off,
                            &tlv_info, sizeof(tlv_info)) == 0) {
            /* Not a TLV area, image is broken, let image check reject it */
            complete = tlv_info.it_magic != IMAGE_TLV_INFO_MAGIC ||
                       tlv_off + tlv_info.it_tlv_tot <= written;
        }
    }
    flash_area_close(fa);

    return complete;
#else
    return true;
#endif
}

static int
image_file_written(struct msc_fat_view_write_handler *handler, uint32_t size,
                   uint32_t sector, bool first_sector)
{
    struct image_version version;
    uint32_t flags;

    if (unallocated_write.write_status == WRITE_IN_PROGRESS) {
        if (unallocated_write.first_sector == sector && first_sector &&
            !image_write_complete()) {
            MSC_FAT_VIEW_LOG_INFO("New file not complete yet (%d bytes)\n", (int)size);
            return 0;
        }
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
            return 0;
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

#ifdef FLASH_AREA_IMAGE
README_TEXT(90, "\nNew firmware can be copied to this drive (drag-drop .img file to upgrade device).");
#endif
