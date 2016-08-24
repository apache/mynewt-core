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

/** Info about image slots. */
static struct boot_img {
    struct image_header hdr;
    struct boot_image_location loc;
    uint32_t area;
} boot_img[BOOT_NUM_SLOTS];

static struct boot_status boot_state;

static int boot_erase_area(int area_idx, uint32_t sz);
static uint32_t boot_copy_sz(int max_idx, int *cnt);

void
boot_req_set(struct boot_req *req)
{
    boot_req = req;
}

/**
 * Calculates the flash offset of the specified image slot.
 *
 * @param slot_num              The number of the slot to calculate.
 * @param loc                   The flash location of the slot.
 *
 */
static void
boot_slot_addr(int slot_num, struct boot_image_location *loc)
{
    const struct flash_area *area_desc;
    uint8_t area_idx;

    area_idx = boot_req->br_slot_areas[slot_num];
    area_desc = boot_req->br_area_descs + area_idx;
    loc->bil_flash_id = area_desc->fa_flash_id;
    loc->bil_address = area_desc->fa_off;
}

/*
 * Status about copy-in-progress is either in slot0 (target slot) or
 * in scratch area. It is in scratch area if the process is currently
 * moving the last sector within image.
 *
 * If the copy-in-progress status is within the image slot, it will
 * be at the end of the area.
 * If the sector containing the boot copy status is in scratch, it's
 * offset from beginning of scratch depends on how much of the image
 * fits inside the scratch area.
 *
 * We start copy from the end of image, so boot-copy-status is in
 * scratch when the first area is being moved. Otherwise it will be
 * in slot 0.
 */
void
boot_magic_loc(int slot_num, uint8_t *flash_id, uint32_t *off)
{
    struct boot_img *b;

    b = &boot_img[slot_num];
    *flash_id = b->loc.bil_flash_id;
    *off = b->area + b->loc.bil_address - sizeof(struct boot_img_trailer);
}

void
boot_scratch_loc(uint8_t *flash_id, uint32_t *off)
{
    struct flash_area *scratch;
    int cnt;

    scratch = &boot_req->br_area_descs[boot_req->br_scratch_area_idx];
    *flash_id = scratch->fa_flash_id;

    /*
     * Calculate where the boot status would be, if it was copied to scratch.
     */
    *off = boot_copy_sz(boot_req->br_slot_areas[1], &cnt);
    *off += (scratch->fa_off - sizeof(struct boot_img_trailer));
}

void
boot_slot_magic(int slot_num, struct boot_img_trailer *bit)
{
    uint32_t off;
    uint8_t flash_id;

    boot_magic_loc(slot_num, &flash_id, &off);
    memset(bit, 0xff, sizeof(*bit));
    hal_flash_read(flash_id, off, bit, sizeof(*bit));
}

void
boot_scratch_magic(struct boot_img_trailer *bit)
{
    uint32_t off;
    uint8_t flash_id;

    boot_scratch_loc(&flash_id, &off);
    memset(bit, 0xff, sizeof(*bit));
    hal_flash_read(flash_id, off, bit, sizeof(*bit));
}

/*
 * Gather info about image in a given slot.
 */
void
boot_image_info(void)
{
    int i;
    struct boot_img *b;
    struct flash_area *scratch;

    for (i = 0; i < BOOT_NUM_SLOTS; i++) {
        b = &boot_img[i];
        boot_slot_addr(i, &b->loc);
        boot_read_image_header(&b->loc, &b->hdr);
        b->area = boot_req->br_img_sz;
    }

    /*
     * Figure out what size to write update status update as.
     * The size depends on what the minimum write size is for scratch
     * area, active image slot. We need to use the bigger of those 2
     * values.
     */
    boot_state.elem_sz = hal_flash_align(boot_img[0].loc.bil_flash_id);

    scratch = &boot_req->br_area_descs[boot_req->br_scratch_area_idx];
    i = hal_flash_align(scratch->fa_flash_id);
    if (i > boot_state.elem_sz) {
        boot_state.elem_sz = i;
    }
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

/**
 * Selects a slot number to boot from.
 *
 * @return                      The slot number to boot from on success;
 *                              -1 if an appropriate slot could not be
 *                              determined.
 */
static int
boot_select_image_slot(void)
{
    /*
     * Check for swap magic. Check the integrity of the suggested image.
     */
    int rc;
    int i;
    struct boot_img *b;
    struct boot_img_trailer bit;

    boot_slot_magic(0, &bit);
    if (bit.bit_copy_start == BOOT_IMG_MAGIC && bit.bit_copy_done != 0xff &&
      bit.bit_img_ok == 0xff) {
        /*
         * Copied the image successfully, but image was not confirmed as good.
         * We need to go back to another image.
         */
        boot_vect_write_test(FLASH_AREA_IMAGE_1);
    }
    for (i = 1; i < BOOT_NUM_SLOTS; i++) {
        b = &boot_img[i];
        boot_slot_magic(i, &bit);
        if (bit.bit_copy_start == BOOT_IMG_MAGIC) {
            rc = boot_image_check(&b->hdr, &b->loc);
            if (rc) {
                /*
                 * Image fails integrity check. Erase it.
                 */
                boot_erase_area(boot_req->br_slot_areas[i], b->area);
            } else {
                return i;
            }
        }
    }
    return 0;
}

static int
boot_status_sz(void)
{
    return sizeof(struct boot_img_trailer) + 32 * sizeof(uint32_t);
}

/*
 * How many sectors starting from sector[idx] can fit inside scratch.
 *
 */
static uint32_t
boot_copy_sz(int max_idx, int *cnt)
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
    for (i = max_idx - 1; i >= 0; i--) {
        if (sz + boot_req->br_area_descs[i].fa_size > scratch_sz) {
            break;
        }
        sz += boot_req->br_area_descs[i].fa_size;
        *cnt = *cnt + 1;
    }
    return sz;
}

/**
 * Erase one area.  The destination area must
 * be erased prior to this function being called.
 *
 * @param area_idx            The index of the area.
 * @param sz                  The number of bytes to erase.
 *
 * @return                      0 on success; nonzero on failure.
 */
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
 * Swaps the contents of two flash areas belonging to images.
 *
 * @param area_idx            The index of first slot to exchange. This area
 *                                  must be part of the first image slot.
 * @param sz                  The number of bytes swap.
 *
 * @param end_area            Boolean telling whether this includes this
 *                                  area has last slots.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_swap_areas(int idx, uint32_t sz, int end_area)
{
    int area_idx_1;
    int area_idx_2;
    int rc;

    area_idx_1 = boot_req->br_slot_areas[0] + idx;
    area_idx_2 = boot_req->br_slot_areas[1] + idx;
    assert(area_idx_1 != area_idx_2);
    assert(area_idx_1 != boot_req->br_scratch_area_idx);
    assert(area_idx_2 != boot_req->br_scratch_area_idx);

    if (boot_state.state == 0) {
        rc = boot_erase_area(boot_req->br_scratch_area_idx, sz);
        if (rc != 0) {
            return rc;
        }

        rc = boot_copy_area(area_idx_2, boot_req->br_scratch_area_idx, sz);
        if (rc != 0) {
            return rc;
        }

        boot_state.state = 1;
        (void)boot_write_status(&boot_state);
    }
    if (boot_state.state == 1) {
        rc = boot_erase_area(area_idx_2, sz);
        if (rc != 0) {
            return rc;
        }

        rc = boot_copy_area(area_idx_1, area_idx_2,
          end_area ? (sz - boot_status_sz()) : sz);
        if (rc != 0) {
            return rc;
        }

        boot_state.state = 2;
        (void)boot_write_status(&boot_state);
    }
    if (boot_state.state == 2) {
        rc = boot_erase_area(area_idx_1, sz);
        if (rc != 0) {
            return rc;
        }

        rc = boot_copy_area(boot_req->br_scratch_area_idx, area_idx_1, sz);
        if (rc != 0) {
            return rc;
        }

        boot_state.idx++;
        boot_state.state = 0;
        (void)boot_write_status(&boot_state);
    }
    return 0;
}

/**
 * Swaps the two images in flash.  If a prior copy operation was interrupted
 * by a system reset, this function completes that operation.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_copy_image(void)
{
    uint32_t sz;
    int i;
    int end_area = 1;
    int cnt;
    int cur_idx;

    for (i = boot_req->br_slot_areas[1], cur_idx = 0; i > 0; cur_idx++) {
        sz = boot_copy_sz(i, &cnt);
        i -= cnt;
        if (cur_idx >= boot_state.idx) {
            boot_swap_areas(i, sz, end_area);
        }
        end_area = 0;
    }
    boot_clear_status();

    return 0;
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
    int slot;
    int rc;

    /* Set the global boot request object.  The remainder of the boot process
     * will reference the global.
     */
    boot_req = req;

    /* Attempt to read an image header from each slot. */
    boot_image_info();

    /* Read the boot status to determine if an image copy operation was
     * interrupted (i.e., the system was reset before the boot loader could
     * finish its task last time).
     */
    if (boot_read_status(&boot_state)) {
        /* We are resuming an interrupted image copy. */
        rc = boot_copy_image();
        if (rc != 0) {
            /* We failed to put the images back together; there is really no
             * solution here.
             */
            return rc;
        }
    }

    /*
     * Check if we should initiate copy, or revert back to earlier image.
     *
     */
    slot = boot_select_image_slot();
    if (slot == -1) {
        return BOOT_EBADIMAGE;
    }

    if (slot) {
        boot_state.idx = 0;
        boot_state.state = 0;
        rc = boot_copy_image();
        if (rc) {
            return rc;
        }
    }

    /* Always boot from the primary slot. */
    rsp->br_flash_id = boot_img[0].loc.bil_flash_id;
    rsp->br_image_addr = boot_img[0].loc.bil_address;
    rsp->br_hdr = &boot_img[slot].hdr;

    return 0;
}
