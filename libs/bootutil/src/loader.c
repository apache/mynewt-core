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
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <hal/flash_map.h>
#include <hal/hal_flash.h>
#include <os/os_malloc.h>
#include "bootutil/loader.h"
#include "bootutil/image.h"
#include "bootutil/bootutil_misc.h"
#include "bootutil_priv.h"

/** Number of image slots in flash; currently limited to two. */
#define BOOT_NUM_SLOTS              2

/** The request object provided by the client. */
static const struct boot_req *boot_req;

/** Image headers read from flash. */
struct image_header boot_img_hdrs[2];

static struct boot_status boot_state;

#define BOOT_PERSIST(idx, st) (((idx) << 8) | (0xff & (st)))
#define BOOT_PERSIST_IDX(st) (((st) >> 8) & 0xffffff)
#define BOOT_PERSIST_ST(st) ((st) & 0xff)

/**
 * Calculates the flash offset of the specified image slot.
 *
 * @param slot_num              The number of the slot to calculate.
 *
 * @return                      The flash offset of the image slot.
 */
static void
boot_slot_addr(int slot_num, uint8_t *flash_id, uint32_t *address)
{
    const struct flash_area *area_desc;
    uint8_t area_idx;

    assert(slot_num >= 0 && slot_num < BOOT_NUM_SLOTS);

    area_idx = boot_req->br_slot_areas[slot_num];
    area_desc = boot_req->br_area_descs + area_idx;
    *flash_id = area_desc->fa_flash_id;
    *address = area_desc->fa_off;
}

/**
 * Searches flash for an image with the specified version number.
 *
 * @param ver                   The version number to search for.
 *
 * @return                      The image slot containing the specified image
 *                              on success; -1 on failure.
 */
static int
boot_find_image_slot(const struct image_version *ver)
{
    int i;

    for (i = 0; i < 2; i++) {
        if (memcmp(&boot_img_hdrs[i].ih_ver, ver, sizeof *ver) == 0) {
            return i;
        }
    }

    return -1;
}

/**
 * Selects a slot number to boot from, based on the contents of the boot
 * vector.
 *
 * @return                      The slot number to boot from on success;
 *                              -1 if an appropriate slot could not be
 *                              determined.
 */
static int
boot_select_image_slot(void)
{
    struct image_version ver;
    int slot;
    int rc;

    rc = boot_vect_read_test(&ver);
    if (rc == 0) {
        slot = boot_find_image_slot(&ver);
        if (slot == -1) {
            boot_vect_write_test(NULL);
        } else {
            return slot;
        }
    }

    rc = boot_vect_read_main(&ver);
    if (rc == 0) {
        slot = boot_find_image_slot(&ver);
        if (slot == -1) {
            boot_vect_write_main(NULL);
        } else {
            return slot;
        }
    }

    return -1;
}

/*
 * Validate image hash/signature in a slot.
 */
static int
boot_image_check(struct image_header *hdr, struct boot_image_location *loc)
{
    static void *tmpbuf;

    if (!tmpbuf) {
        tmpbuf = malloc(BOOT_TMPBUF_SZ);
        if (!tmpbuf) {
            return BOOT_ENOMEM;
        }
    }
    if (bootutil_img_validate(hdr, loc->bil_flash_id, loc->bil_address,
        tmpbuf, BOOT_TMPBUF_SZ)) {
        return BOOT_EBADIMAGE;
    }
    return 0;
}

/*
 * How many sectors starting from sector[idx] can fit inside scratch.
 *
 */
static uint32_t
boot_copy_sz(int idx, int max_idx, int *cnt)
{
    int i;
    uint32_t sz;
    static uint32_t scratch_sz = 0;

    if (!scratch_sz) {
        for (i = boot_req->br_scratch_area_idx;
             i < boot_req->br_num_image_areas;
             i++) {
            scratch_sz += boot_req->br_area_descs[i].fa_size;
        }
    }
    sz = 0;
    *cnt = 0;
    for (i = idx; i < max_idx; i++) {
        if (sz + boot_req->br_area_descs[i].fa_size > scratch_sz) {
            break;
        }
        sz += boot_req->br_area_descs[i].fa_size;
        *cnt = *cnt + 1;
    }
    return sz;
}


static int
boot_erase_area(int area_idx, uint32_t sz)
{
    const struct flash_area *area_desc;
    int rc;

    area_desc = boot_req->br_area_descs + area_idx;
    rc = hal_flash_erase(area_desc->fa_flash_id, area_desc->fa_off, sz);
    if (rc != 0) {
        return BOOT_EFLASH;
    }
    return 0;
}

/**
 * Copies the contents of one area to another.  The destination area must
 * be erased prior to this function being called.
 *
 * @param from_area_idx       The index of the source area.
 * @param to_area_idx         The index of the destination area.
 * @param sz                  The number of bytes to move.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_copy_area(int from_area_idx, int to_area_idx, uint32_t sz)
{
    const struct flash_area *from_area_desc;
    const struct flash_area *to_area_desc;
    uint32_t from_addr;
    uint32_t to_addr;
    uint32_t off;
    int chunk_sz;
    int rc;

    static uint8_t buf[1024];

    from_area_desc = boot_req->br_area_descs + from_area_idx;
    to_area_desc = boot_req->br_area_descs + to_area_idx;

    assert(to_area_desc->fa_size >= from_area_desc->fa_size);

    off = 0;
    while (off < sz) {
        if (sz - off > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = sz - off;
        }

        from_addr = from_area_desc->fa_off + off;
        rc = hal_flash_read(from_area_desc->fa_flash_id, from_addr, buf,
                            chunk_sz);
        if (rc != 0) {
            return rc;
        }

        to_addr = to_area_desc->fa_off + off;
        rc = hal_flash_write(to_area_desc->fa_flash_id, to_addr, buf,
                             chunk_sz);
        if (rc != 0) {
            return rc;
        }

        off += chunk_sz;
    }

    return 0;
}

/**
 * Swaps the contents of two flash areas.
 *
 * @param area_idx_1          The index of one area to swap.  This area
 *                                  must be part of the first image slot.
 * @param area_idx_2          The index of the other area to swap.  This
 *                                  area must be part of the second image
 *                                  slot.
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_swap_areas(int idx, uint32_t sz)
{
    int area_idx_1;
    int area_idx_2;
    int rc;
    int state;

    area_idx_1 = boot_req->br_slot_areas[0] + idx;
    area_idx_2 = boot_req->br_slot_areas[1] + idx;
    assert(area_idx_1 != area_idx_2);
    assert(area_idx_1 != boot_req->br_scratch_area_idx);
    assert(area_idx_2 != boot_req->br_scratch_area_idx);

    state = BOOT_PERSIST_ST(boot_state.state);
    if (state == 0) {
        rc = boot_erase_area(boot_req->br_scratch_area_idx, sz);
        if (rc != 0) {
            return rc;
        }

        rc = boot_copy_area(area_idx_2, boot_req->br_scratch_area_idx, sz);
        if (rc != 0) {
            return rc;
        }

        boot_state.state = BOOT_PERSIST(idx, 1);
        (void)boot_write_status(&boot_state);
        state = 1;
    }
    if (state == 1) {
        rc = boot_erase_area(area_idx_2, sz);
        if (rc != 0) {
            return rc;
        }

        rc = boot_copy_area(area_idx_1, area_idx_2, sz);
        if (rc != 0) {
            return rc;
        }

        boot_state.state = BOOT_PERSIST(idx, 2);
        (void)boot_write_status(&boot_state);
        state = 2;
    }
    if (state == 2) {
        rc = boot_erase_area(area_idx_1, sz);
        if (rc != 0) {
            return rc;
        }

        rc = boot_copy_area(boot_req->br_scratch_area_idx, area_idx_1, sz);
        if (rc != 0) {
            return rc;
        }

        boot_state.state = BOOT_PERSIST(idx + 1, 0);
        (void)boot_write_status(&boot_state);
        state = 3;
    }
    return 0;
}

/**
 * Swaps the two images in flash.  If a prior copy operation was interrupted
 * by a system reset, this function completes that operation.
 *
 * @param img1_length           The length, in bytes, of the slot 1 image.
 * @param img2_length           The length, in bytes, of the slot 2 image.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_copy_image(void)
{
    uint32_t off;
    uint32_t sz;
    int i;
    int cnt;
    int rc;
    int state_idx;

    state_idx = BOOT_PERSIST_IDX(boot_state.state);
    for (off = 0, i = 0; off < boot_state.length; off += sz, i += cnt) {
        sz = boot_copy_sz(i, boot_req->br_slot_areas[1], &cnt);
        if (i >= state_idx) {
            rc = boot_swap_areas(i, sz);
            assert(rc == 0);
        }
    }

    return 0;
}

/**
 * Builds a default boot status corresponding to all images being fully present
 * in their slots.  This function is used when a boot status is not present in
 * flash (i.e., in the usual case when the previous boot operation ran to
 * completion).
 */
static void
boot_build_status(void)
{
    uint32_t len1;
    uint32_t len2;

    if (boot_img_hdrs[0].ih_magic == IMAGE_MAGIC) {
        len1 = boot_img_hdrs[0].ih_hdr_size + boot_img_hdrs[0].ih_img_size +
          boot_img_hdrs[0].ih_tlv_size;
    } else {
        len1 = 0;
    }

    if (boot_img_hdrs[1].ih_magic == IMAGE_MAGIC) {
        len2 = boot_img_hdrs[1].ih_hdr_size + boot_img_hdrs[1].ih_img_size +
          boot_img_hdrs[0].ih_tlv_size;
    } else {
        len2 = 0;
    }
    boot_state.length = len1;
    if (len1 < len2) {
        boot_state.length = len2;
    }
    boot_state.state = 0;
}

/**
 * Prepares the booting process.  Based on the information provided in the
 * request object, this function moves images around in flash as appropriate,
 * and tells you what address to boot from.
 *
 * @param req                   Contains information about the flash layout.
 * @param rsp                   On success, indicates how booting should occur.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
boot_go(const struct boot_req *req, struct boot_rsp *rsp)
{
    struct boot_image_location image_addrs[BOOT_NUM_SLOTS];
    int slot;
    int rc;
    int i;

    /* Set the global boot request object.  The remainder of the boot process
     * will reference the global.
     */
    boot_req = req;

    /* Read the boot status to determine if an image copy operation was
     * interrupted (i.e., the system was reset before the boot loader could
     * finish its task last time).
     */
    if (boot_read_status(&boot_state)) {
        /* We are resuming an interrupted image copy. */
        /* XXX if copy has not actually started yet, validate image */
        rc = boot_copy_image();
        if (rc != 0) {
            /* We failed to put the images back together; there is really no
             * solution here.
             */
            return rc;
        }
    }

    /* Cache the flash address of each image slot. */
    for (i = 0; i < BOOT_NUM_SLOTS; i++) {
        boot_slot_addr(i, &image_addrs[i].bil_flash_id,
                       &image_addrs[i].bil_address);
    }

    /* Attempt to read an image header from each slot. */
    boot_read_image_headers(boot_img_hdrs, image_addrs, BOOT_NUM_SLOTS);

    /* Build a boot status structure indicating the flash location of each
     * image part.  This structure will need to be used if an image copy
     * operation is required.
     */
    boot_build_status();

    /* Determine which image the user wants to run, and where it is located. */
    slot = boot_select_image_slot();
    if (slot == -1) {
        /* Either there is no image vector, or none of the requested images are
         * present.  Just try booting from the first image slot.
         */
        if (boot_img_hdrs[0].ih_magic != IMAGE_MAGIC_NONE) {
            slot = 0;
        } else if (boot_img_hdrs[1].ih_magic != IMAGE_MAGIC_NONE) {
            slot = 1;
        } else {
            /* No images present. */
            return BOOT_EBADIMAGE;
        }
    }

    /*
     * If the selected image fails integrity check, try the other one.
     */
    if (boot_image_check(&boot_img_hdrs[slot], &image_addrs[slot])) {
        slot ^= 1;
        if (boot_image_check(&boot_img_hdrs[slot], &image_addrs[slot])) {
            return BOOT_EBADIMAGE;
        }
    }
    switch (slot) {
    case 0:
        rsp->br_hdr = &boot_img_hdrs[0];
        break;

    case 1:
        /* The user wants to run the image in the secondary slot.  The contents
         * of this slot need to moved to the primary slot.
         */
        rc = boot_copy_image();
        if (rc != 0) {
            /* We failed to put the images back together; there is really no
             * solution here.
             */
            return rc;
        }

        rsp->br_hdr = &boot_img_hdrs[1];
        break;

    default:
        assert(0);
        break;
    }

    /* Always boot from the primary slot. */
    rsp->br_flash_id = image_addrs[0].bil_flash_id;
    rsp->br_image_addr = image_addrs[0].bil_address;

    /* After successful boot, there should not be a status file. */
    boot_clear_status();

    /* If an image is being tested, it should only be booted into once. */
    boot_vect_write_test(NULL);

    return 0;
}
