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
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "flash_map/flash_map.h"
#include "os/os.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"

int boot_current_slot;

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
         * status: slot0                       |
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

struct boot_swap_table {
    /** * For each field, a value of 0 means "any". */
    uint32_t bsw_magic_slot0;
    uint32_t bsw_magic_slot1;
    uint8_t bsw_image_ok_slot0;

    uint8_t bsw_swap_type;
};

/**
 * This set of tables maps image trailer contents to swap operation type.
 * When searching for a match, these tables must be iterated sequentially.
 */
static const struct boot_swap_table boot_swap_tables[] = {
    {
        /*          | slot-0     | slot-1     |
         *----------+------------+------------|
         *    magic | 0xffffffff | 0xffffffff |
         * image-ok | 0x**       | N/A        |
         * ---------+------------+------------'
         * swap: none                         |
         * -----------------------------------'
         */
        .bsw_magic_slot0 =      0xffffffff,
        .bsw_magic_slot1 =      0xffffffff,
        .bsw_image_ok_slot0 =   0,
        .bsw_swap_type =        BOOT_SWAP_TYPE_NONE,
    },

    {
        /*          | slot-0     | slot-1     |
         *----------+------------+------------|
         *    magic | 0x******** | 0x12344321 |
         * image-ok | 0x**       | N/A        |
         * ---------+------------+------------'
         * swap: test                         |
         * -----------------------------------'
         */
        .bsw_magic_slot0 =      0,
        .bsw_magic_slot1 =      0x12344321,
        .bsw_image_ok_slot0 =   0,
        .bsw_swap_type =        BOOT_SWAP_TYPE_TEST,
    },

    {
        /*          | slot-0     | slot-1     |
         *----------+------------+------------|
         *    magic | 0x12344321 | 0xffffffff |
         * image-ok | 0xff       | N/A        |
         * ---------+------------+------------'
         * swap: revert (test image running)  |
         * -----------------------------------'
         */
        .bsw_magic_slot0 =      0x12344321,
        .bsw_magic_slot1 =      0xffffffff,
        .bsw_image_ok_slot0 =   0xff,
        .bsw_swap_type =        BOOT_SWAP_TYPE_REVERT,
    },

    {
        /*          | slot-0     | slot-1     |
         *----------+------------+------------|
         *    magic | 0x12344321 | 0xffffffff |
         * image-ok | 0x01       | N/A        |
         * ---------+------------+------------'
         * swap: none (confirmed test image)  |
         * -----------------------------------'
         */
        .bsw_magic_slot0 =      0x12344321,
        .bsw_magic_slot1 =      0xffffffff,
        .bsw_image_ok_slot0 =   0x01,
        .bsw_swap_type =        BOOT_SWAP_TYPE_NONE,
    },
};

#define BOOT_SWAP_TABLES_COUNT \
    (sizeof boot_swap_tables / sizeof boot_swap_tables[0])

/**
 * Reads the image trailer from a given image slot.
 */
static int
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

/**
 * Reads the image trailer from the scratch area.
 */
static int
boot_vect_read_scratch_trailer(struct boot_img_trailer *bit)
{
    int rc;
    const struct flash_area *fap;
    uint32_t off;

    rc = flash_area_open(FLASH_AREA_IMAGE_SCRATCH, &fap);
    if (rc) {
        return rc;
    }
    off = fap->fa_size - sizeof(struct boot_img_trailer);
    rc = flash_area_read(fap, off, bit, sizeof(*bit));
    flash_area_close(fap);

    return rc;
}

int
boot_status_source(void)
{
    const struct boot_status_table *table;
    struct boot_img_trailer bit_scratch;
    struct boot_img_trailer bit_slot0;
    struct boot_img_trailer bit_slot1;
    int rc;
    int i;

    rc = boot_vect_read_img_trailer(0, &bit_slot0);
    assert(rc == 0);

    rc = boot_vect_read_img_trailer(1, &bit_slot1);
    assert(rc == 0);

    rc = boot_vect_read_scratch_trailer(&bit_scratch);
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

int
boot_swap_type(void)
{
    const struct boot_swap_table *table;
    struct boot_img_trailer bit_slot0;
    struct boot_img_trailer bit_slot1;
    int rc;
    int i;

    rc = boot_vect_read_img_trailer(0, &bit_slot0);
    assert(rc == 0);

    rc = boot_vect_read_img_trailer(1, &bit_slot1);
    assert(rc == 0);

    for (i = 0; i < BOOT_SWAP_TABLES_COUNT; i++) {
        table = boot_swap_tables + i;

        if ((table->bsw_magic_slot0     == 0    ||
             table->bsw_magic_slot0     == bit_slot0.bit_copy_start)    &&
            (table->bsw_magic_slot1     == 0    ||
             table->bsw_magic_slot1     == bit_slot1.bit_copy_start)    &&
            (table->bsw_image_ok_slot0  == 0    ||
             table->bsw_image_ok_slot0  == bit_slot0.bit_img_ok)) {

            return table->bsw_swap_type;
        }
    }

    assert(0);
    return BOOT_SWAP_TYPE_NONE;
}

int
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

int
boot_schedule_test_swap(void)
{
    const struct flash_area *fap;
    struct boot_img_trailer bit_slot1;
    uint32_t off;
    int area_id;
    int rc;

    rc = boot_vect_read_img_trailer(1, &bit_slot1);
    assert(rc == 0);

    switch (bit_slot1.bit_copy_start) {
    case BOOT_IMG_MAGIC:
        /* Swap already scheduled. */
        return 0;

    case 0xffffffff:
        bit_slot1.bit_copy_start = BOOT_IMG_MAGIC;

        area_id = flash_area_id_from_image_slot(1);
        rc = flash_area_open(area_id, &fap);
        if (rc) {
            return rc;
        }

        off = fap->fa_size - sizeof(struct boot_img_trailer);
        rc = flash_area_write(fap, off, &bit_slot1,
                              sizeof bit_slot1.bit_copy_start);
        flash_area_close(fap);
        return rc;

    default:
        /* XXX: Temporary assert. */
        assert(0);
        return -1;
    }
}

/**
 * Retrieves from the slot number of the test image (i.e.,
 * the image that has not been proven stable, and which will only run once).
 *
 * @param slot              On success, the slot number of image to boot.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_read_test(int *slot)
{
    struct boot_img_trailer bit;
    int i;
    int rc;

    for (i = 0; i < 2; i++) {
        if (i == boot_current_slot) {
            continue;
        }
        rc = boot_vect_read_img_trailer(i, &bit);
        if (rc) {
            continue;
        }
        if (bit.bit_copy_start == BOOT_IMG_MAGIC) {
            *slot = i;
            return 0;
        }
    }
    return -1;
}

/**
 * Retrieves from the slot number of the main image. If this is
 * different from test image slot, next restart will revert to main.
 *
 * @param out_ver           On success, the main version gets written here.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_read_main(int *slot)
{
    int rc;
    struct boot_img_trailer bit;

    rc = boot_vect_read_img_trailer(0, &bit);
    assert(rc == 0);

    if (bit.bit_copy_start != BOOT_IMG_MAGIC || bit.bit_img_ok != 0xff) {
        /*
         * If there never was copy that took place, or if the current
         * image has been marked good, we'll keep booting it.
         */
        *slot = 0;
    } else {
        *slot = 1;
    }
    return 0;
}

/**
 * Write the test image version number from the boot vector.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_set_pending(int slot)
{
    int rc;

    assert(slot == 1);

    rc = boot_schedule_test_swap();
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
boot_set_confirmed(void)
{
    const struct flash_area *fap;
    struct boot_img_trailer bit_slot0;
    uint32_t off;
    uint8_t img_ok;
    int rc;

    rc = boot_vect_read_img_trailer(0, &bit_slot0);
    assert(rc == 0);

    if (bit_slot0.bit_copy_start != BOOT_IMG_MAGIC) {
        /* Already confirmed. */
        return 0;
    }

    if (bit_slot0.bit_copy_done == 0xff) {
        /* Swap never completed.  This is unexpected. */
        return -1;
    }

    if (bit_slot0.bit_img_ok != 0xff) {
        /* Already confirmed. */
        return 0;
    }

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    if (rc) {
        return rc;
    }

    off = fap->fa_size -
          sizeof(struct boot_img_trailer) +
          offsetof(struct boot_img_trailer, bit_img_ok);

    img_ok = 1;
    rc = flash_area_write(fap, off, &img_ok, 1);
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

uint32_t
boot_status_sz(int elem_sz)
{
    return BOOT_STATUS_MAX_ENTRIES * BOOT_STATUS_STATE_COUNT * elem_sz;
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

/*
 * How far has the copy progressed?
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

/**
 * Reads the boot status from the flash.  The boot status contains
 * the current state of an interrupted image copy operation.  If the boot
 * status is not present, or it indicates that previous copy finished,
 * there is no operation in progress.
 */
int
boot_read_status(struct boot_status *bs)
{
    uint32_t off;
    uint8_t flash_id;
    int status_loc;

    status_loc = boot_status_source();

    switch (status_loc) {
    case BOOT_STATUS_SOURCE_NONE:
        break;

    case BOOT_STATUS_SOURCE_SCRATCH:
        boot_scratch_loc(&flash_id, &off);
        boot_read_status_bytes(bs, flash_id, off);
        break;

    case BOOT_STATUS_SOURCE_SLOT0:
        boot_magic_loc(0, &flash_id, &off);
        boot_read_status_bytes(bs, flash_id, off);
        break;

    default:
        assert(0);
        break;
    }

    return bs->idx != 0 || bs->state != 0;
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

/**
 * Marks a test image in slot 0 as fully copied.
 */
int
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
int
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

void
boot_set_image_slot_split(void)
{
    boot_current_slot = 1;
}
