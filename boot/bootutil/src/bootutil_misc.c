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
 * Reads the image trailer from the scratch area.
 */
int
boot_read_scratch_trailer(struct boot_img_trailer *bit)
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

/**
 * Reads the image trailer from a given image slot.
 */
int
boot_read_img_trailer(int slot, struct boot_img_trailer *bit)
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
boot_swap_type(void)
{
    const struct boot_swap_table *table;
    struct boot_img_trailer bit_slot0;
    struct boot_img_trailer bit_slot1;
    int rc;
    int i;

    rc = boot_read_img_trailer(0, &bit_slot0);
    assert(rc == 0);

    rc = boot_read_img_trailer(1, &bit_slot1);
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

/**
 * Write the test image version number from the boot vector.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_set_pending(void)
{
    const struct flash_area *fap;
    struct boot_img_trailer bit_slot1;
    uint32_t off;
    int area_id;
    int rc;

    rc = boot_read_img_trailer(1, &bit_slot1);
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

    rc = boot_read_img_trailer(0, &bit_slot0);
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
