/**
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
#include <string.h>
#include <inttypes.h>

#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "defs/error.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "flash_map/flash_map.h"
#include "os/os.h"
#include "bootutil/image.h"
#include "bootutil/loader.h"
#include "bootutil/bootutil_misc.h"
#include "bootutil_priv.h"

int boot_current_slot;
int8_t boot_split_app_active;

/*
 * Read the image trailer from a given slot.
 */
int
boot_vect_read_img_trailer(int slot, struct boot_img_trailer *bit)
{
    int rc;
    const struct flash_area *fap;
    uint32_t off;
    int area_id;

    area_id = flash_area_id_from_image_slot(slot);
    rc = flash_area_open(area_id, &fap);
    if (rc) {
        return rc;
    }
    off = fap->fa_size - sizeof(struct boot_img_trailer);
    rc = flash_area_read(fap, off, bit, sizeof(*bit));
    flash_area_close(fap);

    return rc;
}

int
boot_status_sz(void)
{
    return sizeof(struct boot_img_trailer) + 32 * sizeof(uint32_t);
}

int
boot_swap_type(void)
{
    struct boot_img_trailer bit0;
    struct boot_img_trailer bit1;

    boot_vect_read_img_trailer(0, &bit0);
    boot_vect_read_img_trailer(1, &bit1);

    if (bit0.bit_copy_start == BOOT_MAGIC_SWAP_NONE &&
        bit1.bit_copy_start == BOOT_MAGIC_SWAP_NONE) {

        return BOOT_SWAP_TYPE_NONE;
    }

    if (bit1.bit_copy_start == BOOT_MAGIC_SWAP_TEMP) {
        return BOOT_SWAP_TYPE_TEMP;
    }

    if (bit0.bit_copy_start == BOOT_MAGIC_SWAP_PERM) {
        if (bit0.bit_img_ok != 0xff) {
            return BOOT_SWAP_TYPE_NONE;
        } else {
            return BOOT_SWAP_TYPE_PERM;
        }
    }

    /* This should never happen. */
    /* XXX: Remove this assert. */
    assert(0);
    return BOOT_SWAP_TYPE_NONE;
}

/**
 * Write the test image version number from the boot vector.
 *
 * @param slot              The image slot to write to.  Note: this is an
 *                              image slot index, not a flash area ID.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_write_test(int slot)
{
    const struct flash_area *fap;
    uint32_t off;
    uint32_t magic;
    int area_id;
    int rc;

    area_id = flash_area_id_from_image_slot(slot);
    rc = flash_area_open(area_id, &fap);
    if (rc) {
        return rc;
    }

    off = fap->fa_size - sizeof(struct boot_img_trailer);
    magic = BOOT_MAGIC_SWAP_TEMP;

    rc = flash_area_write(fap, off, &magic, sizeof(magic));
    flash_area_close(fap);

    return rc;
}

/**
 * Deletes the main image version number from the boot vector.
 * This must be called by the app to confirm that it is ok to keep booting
 * to this image.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_write_main(void)
{
    const struct flash_area *fap;
    uint32_t off;
    int rc;
    uint8_t val;

    /*
     * Write to slot 0.
     */
    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    if (rc) {
        return rc;
    }

    off = fap->fa_size - sizeof(struct boot_img_trailer);
    off += (sizeof(uint32_t) + sizeof(uint8_t));
    rc = flash_area_read(fap, off, &val, sizeof(val));
    if (!rc && val == 0xff) {
        val = 0;
        rc = flash_area_write(fap, off, &val, sizeof(val));
    }
    return rc;
}

/**
 * Reads the header of image present in flash.  Header corresponding to
 * empty image slot is filled with 0xff bytes.
 *
 * @param out_headers           Points to an array of image headers.  Each
 *                                  element is filled with the header of the
 *                                  corresponding image in flash.
 * @param addresses             An array containing the flash addresses of each
 *                                  image slot.
 * @param num_addresses         The number of headers to read.  This should
 *                                  also be equal to the lengths of the
 *                                  out_headers and addresses arrays.
 */
int
boot_read_image_header(struct boot_image_location *loc,
                       struct image_header *out_hdr)
{
    int rc;

    rc = hal_flash_read(loc->bil_flash_id, loc->bil_address, out_hdr,
                        sizeof *out_hdr);
    if (rc != 0) {
        rc = BOOT_EFLASH;
    } else if (out_hdr->ih_magic != IMAGE_MAGIC) {
        rc = BOOT_EBADIMAGE;
    }

    if (rc) {
        memset(out_hdr, 0xff, sizeof(*out_hdr));
    }
    return rc;
}

/*
 * How far has the copy progressed?
 */
static void
boot_read_status_bytes(struct boot_status *bs, uint8_t flash_id, uint32_t off)
{
    uint8_t status;

    assert(bs->elem_sz);
    off -= bs->elem_sz * 2;
    while (1) {
        hal_flash_read(flash_id, off, &status, sizeof(status));
        if (status == 0xff) {
            break;
        }
        off -= bs->elem_sz;
        if (bs->state == 2) {
            bs->idx++;
            bs->state = 0;
        } else {
            bs->state++;
        }
    }
}

/**
 * Reads the boot status from the flash.  The boot status contains
 * the current state of an interrupted image copy operation.  If the boot
 * status is not present, or it indicates that previous copy finished,
 * there is no operation in progress.
 */
int
boot_read_status(struct boot_status *bs)
{
    struct boot_img_trailer bit;
    uint8_t flash_id;
    uint32_t off;

    /* Check if boot_img_trailer is in scratch, or at the end of slot0. */
    boot_slot_magic(0, &bit);
    if (bit.bit_copy_start != BOOT_MAGIC_SWAP_NONE &&
        bit.bit_copy_done == 0xff) {

        boot_magic_loc(0, &flash_id, &off);
        boot_read_status_bytes(bs, flash_id, off);
        return 1;
    }

    boot_scratch_magic(&bit);
    if (bit.bit_copy_start != BOOT_MAGIC_SWAP_NONE &&
        bit.bit_copy_done == 0xff) {

        boot_scratch_loc(&flash_id, &off);
        boot_read_status_bytes(bs, flash_id, off);
        return 1;
    }


    return 0;
}


/**
 * Writes the supplied boot status to the flash file system.  The boot status
 * contains the current state of an in-progress image copy operation.
 *
 * @param bs                    The boot status to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
boot_write_status(struct boot_status *bs)
{
    uint32_t off;
    uint8_t flash_id;
    uint8_t val;

    if (bs->idx == 0) {
        /*
         * Write to scratch
         */
        boot_scratch_loc(&flash_id, &off);
    } else {
        /*
         * Write to slot 0;
         */
        boot_magic_loc(0, &flash_id, &off);
    }
    off -= ((3 * bs->elem_sz) * bs->idx + bs->elem_sz * (bs->state + 1));

    val = bs->state;
    hal_flash_write(flash_id, off, &val, sizeof(val));

    return 0;
}

/**
 * Finalizes the copy-in-progress status on the flash.  The boot status
 * contains the current state of an in-progress image copy operation.  By
 * clearing this, it is implied that there is no copy operation in
 * progress.
 */
void
boot_set_copy_done(void)
{
    struct boot_img_trailer bit;
    uint32_t off;
    uint8_t flash_id;

    /*
     * Write to slot 0; boot_img_trailer is the 8 bytes within image slot.
     * Here we say that copy operation was finished.
     */
    boot_magic_loc(0, &flash_id, &off);
    bit.bit_copy_start = BOOT_MAGIC_SWAP_PERM;
    bit.bit_copy_done = 1;
    hal_flash_write(flash_id, off, &bit, 5);
}

int
boot_split_app_active_get(void)
{
    return boot_split_app_active;
}

void
boot_split_app_active_set(int active)
{
    boot_split_app_active = !!active;
}
