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

/**
 * This file provides an interface to the boot loader.  Functions defined in
 * this file should only be called while the boot loader is running.
 */

#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "sysflash/sysflash.h"
#include "flash_map/flash_map.h"
#include <hal/hal_flash.h>
#include <os/os_malloc.h>
#include "bootutil/bootutil.h"
#include "bootutil/image.h"
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

static int boot_erase_area(int area_idx, uint32_t sz);
static uint32_t boot_copy_sz(int max_idx, int *cnt);

struct boot_status_table {
    /** * For each field, a value of 0 means "any". */
    uint32_t bst_magic_slot0;
    uint32_t bst_magic_scratch;
    uint8_t bst_copy_done_slot0;

    uint8_t bst_status_source;
};

/**
 * This set of tables maps image trailer contents to swap status location.
 * When searching for a match, these tables must be iterated sequentially.
 */
static const struct boot_status_table boot_status_tables[] = {
    {
        /*           | slot-0     | scratch    |
         * ----------+------------+------------|
         *     magic | 0x12344321 | 0x******** |
         * copy-done | 0x01       | N/A        |
         * ----------+------------+------------'
         * status: none                        |
         * ------------------------------------'
         */
        .bst_magic_slot0 =      BOOT_IMG_MAGIC,
        .bst_magic_scratch =    0,
        .bst_copy_done_slot0 =  0x01,
        .bst_status_source =    BOOT_STATUS_SOURCE_NONE,
    },

    {
        /*           | slot-0     | scratch    |
         * ----------+------------+------------|
         *     magic | 0x12344321 | 0x******** |
         * copy-done | 0xff       | N/A        |
         * ----------+------------+------------'
         * status: slot 0                      |
         * ------------------------------------'
         */
        .bst_magic_slot0 =      BOOT_IMG_MAGIC,
        .bst_magic_scratch =    0,
        .bst_copy_done_slot0 =  0xff,
        .bst_status_source =    BOOT_STATUS_SOURCE_SLOT0,
    },

    {
        /*           | slot-0     | scratch    |
         * ----------+------------+------------|
         *     magic | 0x******** | 0x12344321 |
         * copy-done | 0x**       | N/A        |
         * ----------+------------+------------'
         * status: scratch                     |
         * ------------------------------------'
         */
        .bst_magic_slot0 =      0,
        .bst_magic_scratch =    BOOT_IMG_MAGIC,
        .bst_copy_done_slot0 =  0,
        .bst_status_source =    BOOT_STATUS_SOURCE_SCRATCH,
    },

    {
        /*           | slot-0     | scratch    |
         * ----------+------------+------------|
         *     magic | 0xffffffff | 0xffffffff |
         * copy-done | 0xff       | N/A        |
         * ----------+------------+------------|
         * status: slot 0                      |
         * ------------------------------------+-------------------------------+
         * This represents one of two cases:                                   |
         * o No swaps ever (no status to read anyway, so no harm in checking). |
         * o Mid-revert; status in slot 0.                                     |
         * --------------------------------------------------------------------'
         */
        .bst_magic_slot0 =      0xffffffff,
        .bst_magic_scratch =    0,
        .bst_copy_done_slot0 =  0xff,
        .bst_status_source =    BOOT_STATUS_SOURCE_SLOT0,
    },
};

#define BOOT_STATUS_TABLES_COUNT \
    (sizeof boot_status_tables / sizeof boot_status_tables[0])

static int
boot_status_source(void)
{
    const struct boot_status_table *table;
    struct boot_img_trailer bit_scratch;
    struct boot_img_trailer bit_slot0;
    struct boot_img_trailer bit_slot1;
    int rc;
    int i;

    rc = boot_read_img_trailer(0, &bit_slot0);
    assert(rc == 0);

    rc = boot_read_img_trailer(1, &bit_slot1);
    assert(rc == 0);

    rc = boot_read_scratch_trailer(&bit_scratch);
    assert(rc == 0);

    for (i = 0; i < BOOT_STATUS_TABLES_COUNT; i++) {
        table = boot_status_tables + i;

        if ((table->bst_magic_slot0     == 0    ||
             table->bst_magic_slot0     == bit_slot0.bit_copy_start)   &&
            (table->bst_magic_scratch   == 0    ||
             table->bst_magic_scratch   == bit_scratch.bit_copy_start) &&
            (table->bst_copy_done_slot0 == 0    ||
             table->bst_copy_done_slot0 == bit_slot0.bit_copy_done)) {

            return table->bst_status_source;
        }
    }

    return BOOT_STATUS_SOURCE_NONE;
}

static int
boot_partial_swap_type(void)
{
    int swap_type;

    swap_type = boot_swap_type();
    switch (swap_type) {
    case BOOT_SWAP_TYPE_NONE:
        return BOOT_SWAP_TYPE_REVERT;

    case BOOT_SWAP_TYPE_REVERT:
        return BOOT_SWAP_TYPE_TEST;

    default:
        assert(0);
        return BOOT_SWAP_TYPE_REVERT;
    }
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
    loc->bil_flash_id = area_desc->fa_device_id;
    loc->bil_address = area_desc->fa_off;
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
static int
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

static void
boot_read_image_headers(void)
{
    struct boot_img *b;
    int i;

    for (i = 0; i < BOOT_NUM_SLOTS; i++) {
        b = &boot_img[i];
        boot_slot_addr(i, &b->loc);
        boot_read_image_header(&b->loc, &b->hdr);
        b->area = boot_req->br_img_sz;
    }
}

int
boot_build_request(struct boot_req *preq, int area_descriptor_max)
{
    int cnt;
    int total;
    int rc;
    const struct flash_area *fap;
    struct flash_area *descs = preq->br_area_descs;
    uint8_t *img_starts = preq->br_slot_areas;

    cnt = area_descriptor_max;
    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_0, &cnt, descs);
    img_starts[0] = 0;
    total = cnt;

    flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    preq->br_img_sz = fap->fa_size;

    cnt = area_descriptor_max - total;
    if (cnt < 0) {
        return -1;
    }

    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_1, &cnt,
                               &descs[total]);
    if (rc != 0) {
        return -2;
    }
    img_starts[1] = total;
    total += cnt;

    cnt = area_descriptor_max - total;
    if (cnt < 0) {
        return -3;
    }

    rc = flash_area_to_sectors(FLASH_AREA_IMAGE_SCRATCH, &cnt,
                               &descs[total]);
    if(rc != 0) {
        return -4;
    }

    preq->br_scratch_area_idx = total;
    total += cnt;
    preq->br_num_image_areas = total;
    return 0;
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
static void
boot_magic_loc(int slot_num, uint8_t *flash_id, uint32_t *off)
{
    struct boot_img *b;

    b = &boot_img[slot_num];
    *flash_id = b->loc.bil_flash_id;
    *off = b->area + b->loc.bil_address - sizeof(struct boot_img_trailer);
}

static void
boot_scratch_loc(uint8_t *flash_id, uint32_t *off)
{
    struct flash_area *scratch;
    int cnt;

    scratch = &boot_req->br_area_descs[boot_req->br_scratch_area_idx];
    *flash_id = scratch->fa_device_id;

    /* Calculate where the boot status would be if it was copied to scratch. */
    *off = boot_copy_sz(boot_req->br_slot_areas[1], &cnt);
    *off += (scratch->fa_off - sizeof(struct boot_img_trailer));
}

static uint32_t
boot_status_off(uint32_t trailer_off, int status_idx, int status_state,
                int elem_sz)
{
    uint32_t status_start;
    int idx_sz;

    status_start = trailer_off - boot_status_sz(elem_sz);

    idx_sz = BOOT_STATUS_STATE_COUNT * elem_sz;
    return status_start +
           status_idx * idx_sz +
           status_state * elem_sz;
}

/**
 * Reads the status of a partially-completed swap, if any.  This is necessary
 * to recover in case the boot lodaer was reset in the middle of a swap
 * operation.
 */
static void
boot_read_status_bytes(struct boot_status *bs, uint8_t flash_id,
                       uint32_t trailer_off)
{
    uint32_t status_sz;
    uint32_t off;
    uint8_t status;
    int found;
    int i;

    status_sz = boot_status_sz(bs->elem_sz);
    off = trailer_off - status_sz;

    found = 0;
    for (i = 0; i < status_sz; i++) {
        hal_flash_read(flash_id, off + i * bs->elem_sz,
                       &status, sizeof status);
        if (status == 0xff) {
            if (found) {
                break;
            }
        } else if (!found) {
            found = 1;
        }
    }

    if (found) {
        i--;
        bs->idx = i / BOOT_STATUS_STATE_COUNT;
        bs->state = i % BOOT_STATUS_STATE_COUNT;
    }
}

static uint8_t
boot_status_elem_sz(void)
{
    const struct flash_area *scratch;
    uint8_t elem_sz;
    uint8_t align;

    /* Figure out what size to write update status update as.  The size depends
     * on what the minimum write size is for scratch area, active image slot.
     * We need to use the bigger of those 2 values.
     */
    elem_sz = hal_flash_align(boot_img[0].loc.bil_flash_id);

    scratch = &boot_req->br_area_descs[boot_req->br_scratch_area_idx];
    align = hal_flash_align(scratch->fa_device_id);
    if (align > elem_sz) {
        elem_sz = align;
    }

    return elem_sz;
}

/**
 * Reads the boot status from the flash.  The boot status contains
 * the current state of an interrupted image copy operation.  If the boot
 * status is not present, or it indicates that previous copy finished,
 * there is no operation in progress.
 */
static int
boot_read_status(struct boot_status *boot_state)
{
    uint32_t off;
    uint8_t flash_id;
    int status_loc;

    memset(boot_state, 0, sizeof *boot_state);

    boot_state->elem_sz = boot_status_elem_sz();

    status_loc = boot_status_source();
    switch (status_loc) {
    case BOOT_STATUS_SOURCE_NONE:
        break;

    case BOOT_STATUS_SOURCE_SCRATCH:
        boot_scratch_loc(&flash_id, &off);
        boot_read_status_bytes(boot_state, flash_id, off);
        break;

    case BOOT_STATUS_SOURCE_SLOT0:
        boot_magic_loc(0, &flash_id, &off);
        boot_read_status_bytes(boot_state, flash_id, off);
        break;

    default:
        assert(0);
        break;
    }

    return boot_state->idx != 0 || boot_state->state != 0;
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
    uint32_t trailer_off;
    uint32_t status_off;
    uint8_t flash_id;

    if (bs->idx == 0) {
        /* Write to scratch. */
        boot_scratch_loc(&flash_id, &trailer_off);
    } else {
        /* Write to slot 0. */
        boot_magic_loc(0, &flash_id, &trailer_off);
    }

    status_off = boot_status_off(trailer_off, bs->idx, bs->state, bs->elem_sz);
    hal_flash_write(flash_id, status_off, &bs->state, 1);

    return 0;
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
        tmpbuf, BOOT_TMPBUF_SZ, NULL, 0, NULL)) {
        return BOOT_EBADIMAGE;
    }
    return 0;
}


static int
split_image_check(struct image_header *app_hdr,
                  struct boot_image_location *app_loc,
                  struct image_header *loader_hdr,
                  struct boot_image_location *loader_loc)
{
    static void *tmpbuf;
    uint8_t loader_hash[32];

    if (!tmpbuf) {
        tmpbuf = malloc(BOOT_TMPBUF_SZ);
        if (!tmpbuf) {
            return BOOT_ENOMEM;
        }
    }
    if (bootutil_img_validate(loader_hdr, loader_loc->bil_flash_id,
        loader_loc->bil_address, tmpbuf, BOOT_TMPBUF_SZ,
        NULL, 0, loader_hash)) {

        return BOOT_EBADIMAGE;
    }

    if (bootutil_img_validate(app_hdr, app_loc->bil_flash_id,
                              app_loc->bil_address,
        tmpbuf, BOOT_TMPBUF_SZ, loader_hash, 32, NULL)) {
        return BOOT_EBADIMAGE;
    }
    return 0;
}

static int
boot_validate_slot1(void)
{
    if (boot_img[1].hdr.ih_magic == 0xffffffff ||
        boot_img[1].hdr.ih_flags & IMAGE_F_NON_BOOTABLE) {

        /* No bootable image in slot 1; continue booting from slot 0. */
        return -1;
    }

    if (boot_img[1].hdr.ih_magic != IMAGE_MAGIC ||
        boot_image_check(&boot_img[1].hdr, &boot_img[1].loc) != 0) {

        /* Image in slot 1 is invalid.  Erase the image and continue booting
         * from slot 0.
         */
        boot_erase_area(boot_req->br_slot_areas[1], boot_img[1].area);
        return -1;
    }

    /* Image in slot 1 is valid. */
    return 0;
}

/**
 * Determines which swap operation to perform, if any.  If it is determined
 * that a swap operation is required, the image in the second slot is checked
 * for validity.  If the image in the second slot is invalid, it is erased, and
 * a swap type of "none" is indicated.
 *
 * @return                      The type of swap to perform (BOOT_SWAP_TYPE...)
 */
static int
boot_validated_swap_type(void)
{
    int swap_type;
    int rc;

    swap_type = boot_swap_type();
    if (swap_type == BOOT_SWAP_TYPE_NONE) {
        /* Continue using slot 0. */
        return BOOT_SWAP_TYPE_NONE;
    }

    /* Boot loader wants to switch to slot 1.  Ensure image is valid. */
    rc = boot_validate_slot1();
    if (rc != 0) {
        return BOOT_SWAP_TYPE_NONE;
    }

    return swap_type;
}

/**
 * Calculates the size of the swap status and image trailer at the end of each
 * image slot.
 */
static int
boot_meta_sz(int status_elem_sz)
{
    return sizeof (struct boot_img_trailer) + boot_status_sz(status_elem_sz);
}

/**
 * How many sectors starting from sector[idx] can fit inside scratch.
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
 * Erases one area.
 *
 * @param area_idx              The index of the area.
 * @param sz                    The number of bytes to erase.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_erase_area(int area_idx, uint32_t sz)
{
    const struct flash_area *area_desc;
    int rc;

    area_desc = boot_req->br_area_descs + area_idx;
    rc = hal_flash_erase(area_desc->fa_device_id, area_desc->fa_off, sz);
    if (rc != 0) {
        return BOOT_EFLASH;
    }
    return 0;
}

/**
 * Copies the contents of one area to another.  The destination area must
 * be erased prior to this function being called.
 *
 * @param from_area_idx         The index of the source area.
 * @param to_area_idx           The index of the destination area.
 * @param sz                    The number of bytes to move.
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
        rc = hal_flash_read(from_area_desc->fa_device_id, from_addr, buf,
                            chunk_sz);
        if (rc != 0) {
            return rc;
        }

        to_addr = to_area_desc->fa_off + off;
        rc = hal_flash_write(to_area_desc->fa_device_id, to_addr, buf,
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
 * @param idx                   The index of first slot to exchange. This area
 *                                  must be part of the first image slot.
 * @param sz                    The number of bytes swap.
 *
 * @param end_area              Whether this is the last sector in the source
 *                                  and destination slots (0/1).
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_swap_areas(int idx, uint32_t sz, int end_area,
                struct boot_status *boot_state)
{
    int area_idx_0;
    int area_idx_1;
    int rc;

    area_idx_0 = boot_req->br_slot_areas[0] + idx;
    area_idx_1 = boot_req->br_slot_areas[1] + idx;
    assert(area_idx_0 != area_idx_1);
    assert(area_idx_0 != boot_req->br_scratch_area_idx);
    assert(area_idx_1 != boot_req->br_scratch_area_idx);

    if (boot_state->state == 0) {
        rc = boot_erase_area(boot_req->br_scratch_area_idx, sz);
        if (rc != 0) {
            return rc;
        }

        rc = boot_copy_area(area_idx_1, boot_req->br_scratch_area_idx, sz);
        if (rc != 0) {
            return rc;
        }

        boot_state->state = 1;
        (void)boot_write_status(boot_state);
    }
    if (boot_state->state == 1) {
        rc = boot_erase_area(area_idx_1, sz);
        if (rc != 0) {
            return rc;
        }

        rc = boot_copy_area(area_idx_0, area_idx_1,
          end_area ? (sz - boot_meta_sz(boot_state->elem_sz)) : sz);
        if (rc != 0) {
            return rc;
        }

        boot_state->state = 2;
        (void)boot_write_status(boot_state);
    }
    if (boot_state->state == 2) {
        rc = boot_erase_area(area_idx_0, sz);
        if (rc != 0) {
            return rc;
        }

        rc = boot_copy_area(boot_req->br_scratch_area_idx, area_idx_0, sz);
        if (rc != 0) {
            return rc;
        }

        boot_state->idx++;
        boot_state->state = 0;
        (void)boot_write_status(boot_state);
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
boot_copy_image(struct boot_status *boot_state)
{
    uint32_t sz;
    int i;
    int end_area = 1;
    int cnt;
    int cur_idx;

    for (i = boot_req->br_slot_areas[1], cur_idx = 0; i > 0; cur_idx++) {
        sz = boot_copy_sz(i, &cnt);
        i -= cnt;
        if (cur_idx >= boot_state->idx) {
            boot_swap_areas(i, sz, end_area, boot_state);
        }
        end_area = 0;
    }

    return 0;
}

/**
 * Marks a test image in slot 0 as fully copied.
 */
static int
boot_finalize_test_swap(void)
{
    struct boot_img_trailer bit;
    uint32_t off;
    uint8_t flash_id;
    int rc;

    boot_magic_loc(0, &flash_id, &off);
    off += offsetof(struct boot_img_trailer, bit_copy_done);

    bit.bit_copy_done = 1;
    rc = hal_flash_write(flash_id, off, &bit.bit_copy_done, 1);

    return rc;
}

/**
 * Marks a reverted image in slot 0 as confirmed.  This is necessary to ensure
 * the status bytes from the image revert operation don't get processed on a
 * subsequent boot.
 */
static int
boot_finalize_revert_swap(void)
{
    struct boot_img_trailer bit;
    uint32_t off;
    uint8_t flash_id;
    int rc;

    boot_magic_loc(0, &flash_id, &off);

    bit.bit_copy_start = BOOT_IMG_MAGIC;
    bit.bit_copy_done = 1;
    bit.bit_img_ok = 1;
    rc = hal_flash_write(flash_id, off, &bit, sizeof bit);

    return rc;
}

uint32_t
boot_status_sz(int elem_sz)
{
    return BOOT_STATUS_MAX_ENTRIES * BOOT_STATUS_STATE_COUNT * elem_sz;
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
    struct boot_status boot_state;
    int partial_swap;
    int swap_type;
    int slot;
    int rc;

    /* Set the global boot request object.  The remainder of the boot process
     * will reference the global.
     */
    boot_req = req;

    /* Attempt to read an image header from each slot. */
    boot_read_image_headers();

    /* Determine if we rebooted in the middle of an image swap operation. */
    partial_swap = boot_read_status(&boot_state);
    if (partial_swap) {
        /* Complete the partial swap. */
        rc = boot_copy_image(&boot_state);
        assert(rc == 0);

        swap_type = boot_partial_swap_type();
    } else {
        swap_type = boot_validated_swap_type();
        if (swap_type != BOOT_SWAP_TYPE_NONE) {
            rc = boot_copy_image(&boot_state);
            assert(rc == 0);
        }
    }

    switch (swap_type) {
    case BOOT_SWAP_TYPE_NONE:
        slot = 0;
        break;

    case BOOT_SWAP_TYPE_TEST:
        slot = 1;
        boot_finalize_test_swap();
        break;

    case BOOT_SWAP_TYPE_REVERT:
        slot = 1;
        boot_finalize_revert_swap();
        break;

    default:
        assert(0);
        slot = 0;
        break;
    }

    /* Always boot from the primary slot. */
    rsp->br_flash_id = boot_img[0].loc.bil_flash_id;
    rsp->br_image_addr = boot_img[0].loc.bil_address;
    rsp->br_hdr = &boot_img[slot].hdr;

    return 0;
}

#define SPLIT_AREA_DESC_MAX     (255)

int
split_go(int loader_slot, int split_slot, void **entry)
{
    int rc;
    /** Areas representing the beginning of image slots. */
    uint8_t img_starts[2];
    struct flash_area *descs;
    uint32_t entry_val;
    struct boot_req req = {
        .br_slot_areas = img_starts,
    };

    descs = calloc(SPLIT_AREA_DESC_MAX, sizeof(struct flash_area));
    if (descs == NULL) {
        return SPLIT_GO_ERR;
    }

    req.br_area_descs = descs;

    rc = boot_build_request(&req, SPLIT_AREA_DESC_MAX);
    if (rc != 0) {
        rc = SPLIT_GO_ERR;
        goto split_app_go_end;
    }

    boot_req = &req;

    boot_read_image_headers();

    /* Don't check the bootable image flag because we could really
      * call a bootable or non-bootable image.  Just validate that
      * the image check passes which is distinct from the normal check */
    rc = split_image_check(&boot_img[split_slot].hdr,
                           &boot_img[split_slot].loc,
                           &boot_img[loader_slot].hdr,
                           &boot_img[loader_slot].loc);
    if (rc != 0) {
        rc = SPLIT_GO_NON_MATCHING;
        goto split_app_go_end;
    }

    entry_val = (uint32_t) boot_img[split_slot].loc.bil_address +
                         (uint32_t)  boot_img[split_slot].hdr.ih_hdr_size;
    *entry = (void*) entry_val;
    rc = SPLIT_GO_OK;

split_app_go_end:
    free(descs);
    return rc;
}

#if MYNEWT_VAL(TEST)

/**
 * Used by unit tests.  This allows a test to call boot loader functions
 * without starting the boot loader.
 */
void
boot_req_set(struct boot_req *req)
{
    boot_req = req;
}

#endif
