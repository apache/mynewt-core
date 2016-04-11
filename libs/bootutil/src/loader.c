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
#include "hal/hal_flash.h"
#include "os/os_malloc.h"
#include "nffs/nffs.h"
#include "fs/fs.h"
#include "bootutil/loader.h"
#include "bootutil/image.h"
#include "bootutil_priv.h"

/** Number of image slots in flash; currently limited to two. */
#define BOOT_NUM_SLOTS              2

/** The request object provided by the client. */
static const struct boot_req *boot_req;

/** Image headers read from flash. */
struct image_header boot_img_hdrs[2];

/**
 * The boot status header read from the file system, or generated if not
 * present on disk.  The boot status indicates the state of the image slots in
 * case the system was restarted while images were being moved in flash.
 */
struct boot_status boot_status;

/** The entries associated with the boot status header. */
struct boot_status_entry *boot_status_entries;

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
    const struct nffs_area_desc *area_desc;
    uint8_t area_idx;

    assert(slot_num >= 0 && slot_num < BOOT_NUM_SLOTS);

    area_idx = boot_req->br_slot_areas[slot_num];
    area_desc = boot_req->br_area_descs + area_idx;
    *flash_id = area_desc->nad_flash_id;
    *address = area_desc->nad_offset;
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
            boot_vect_delete_test();
        } else {
            return slot;
        }
    }

    rc = boot_vect_read_main(&ver);
    if (rc == 0) {
        slot = boot_find_image_slot(&ver);
        if (slot == -1) {
            boot_vect_delete_main();
        } else {
            return slot;
        }
    }

    return -1;
}

/**
 * Searches the current boot status for the specified image-num,part-num pair.
 *
 * @param image_num             The image number to search for.
 * @param part_num              The part number of the specified image to
 *                                  search for.
 *
 * @return                      The area index containing the specified image
 *                              part number;
 *                              -1 if the part number is not present in flash.
 */
static int
boot_find_image_part(int image_num, int part_num)
{
    int i;

    for (i = 0; i < boot_req->br_num_image_areas; i++) {
        if (boot_status_entries[i].bse_image_num == image_num &&
            boot_status_entries[i].bse_part_num == part_num) {

            return boot_req->br_image_areas[i];
        }
    }

    return -1;
}

static int
boot_slot_to_area_idx(int slot_num)
{
    int i;
    uint8_t flash_id;
    uint32_t address;

    assert(slot_num >= 0 && slot_num < BOOT_NUM_SLOTS);

    for (i = 0; boot_req->br_area_descs[i].nad_length != 0; i++) {
        boot_slot_addr(slot_num, &flash_id, &address);
        if (boot_req->br_area_descs[i].nad_offset == address &&
          boot_req->br_area_descs[i].nad_flash_id == flash_id) {

            return i;
        }
    }

    return -1;
}

/**
 * Locates the specified area index within the array of image areas.
 *
 * @param area_idx              The area index to search for.
 *
 * @return                      The index of the element in the image area
 *                              array.  that contains the sought after area
 *                              index; -1 if the area index is not present.
 */
static int
boot_find_image_area_idx(int area_idx)
{
    int i;

    for (i = 0; i < boot_req->br_num_image_areas; i++) {
        if (boot_req->br_image_areas[i] == area_idx) {
            return i;
        }
    }

    return -1;
}

static int
boot_erase_area(int area_idx)
{
    const struct nffs_area_desc *area_desc;
    int rc;

    area_desc = boot_req->br_area_descs + area_idx;
    rc = hal_flash_erase(area_desc->nad_flash_id, area_desc->nad_offset,
                         area_desc->nad_length);
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
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_copy_area(int from_area_idx, int to_area_idx)
{
    const struct nffs_area_desc *from_area_desc;
    const struct nffs_area_desc *to_area_desc;
    uint32_t from_addr;
    uint32_t to_addr;
    uint32_t off;
    int chunk_sz;
    int rc;

    static uint8_t buf[1024];

    from_area_desc = boot_req->br_area_descs + from_area_idx;
    to_area_desc = boot_req->br_area_descs + to_area_idx;

    assert(to_area_desc->nad_length >= from_area_desc->nad_length);

    off = 0;
    while (off < from_area_desc->nad_length) {
        if (from_area_desc->nad_length - off > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = from_area_desc->nad_length - off;
        }

        from_addr = from_area_desc->nad_offset + off;
        rc = hal_flash_read(from_area_desc->nad_flash_id, from_addr, buf,
                            chunk_sz);
        if (rc != 0) {
            return rc;
        }

        to_addr = to_area_desc->nad_offset + off;
        rc = hal_flash_write(to_area_desc->nad_flash_id, to_addr, buf,
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
 * @param area_idx_1            The index of one area to swap.  This area
 *                                  must be part of the first image slot.
 * @param part_num_1            The image part number stored in the first
 *                                  area.
 * @param area_idx_2            The index of the other area to swap.  This
 *                                  area must be part of the second image
 *                                  slot.
 * @param part_num_2            The image part number stored in the second
 *                                  area.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_move_area(int from_area_idx, int to_area_idx,
                 int img_num, uint8_t part_num)
{
    int src_image_idx;
    int dst_image_idx;
    int rc;

    src_image_idx = boot_find_image_area_idx(from_area_idx);
    assert(src_image_idx != -1);

    dst_image_idx = boot_find_image_area_idx(to_area_idx);
    assert(dst_image_idx != -1);

    rc = boot_erase_area(to_area_idx);
    if (rc != 0) {
        return rc;
    }

    rc = boot_copy_area(from_area_idx, to_area_idx);
    if (rc != 0) {
        return rc;
    }

    boot_status_entries[src_image_idx].bse_image_num = BOOT_IMAGE_NUM_NONE;
    boot_status_entries[src_image_idx].bse_part_num = BOOT_IMAGE_NUM_NONE;
    boot_status_entries[dst_image_idx].bse_image_num = img_num;
    boot_status_entries[dst_image_idx].bse_part_num = part_num;
    rc = boot_write_status(&boot_status, boot_status_entries,
                           boot_req->br_num_image_areas);
    if (rc != 0) {
        return rc;
    }

    rc = boot_erase_area(from_area_idx);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * Swaps the contents of two flash areas.
 *
 * @param area_idx_1          The index of one area to swap.  This area
 *                                  must be part of the first image slot.
 * @param part_num_1            The image part number stored in the first
 *                                  area.
 * @param area_idx_2          The index of the other area to swap.  This
 *                                  area must be part of the second image
 *                                  slot.
 * @param part_num_2            The image part number stored in the second
 *                                  area.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
boot_swap_areas(int area_idx_1, int img_num_1, uint8_t part_num_1,
                  int area_idx_2, int img_num_2, uint8_t part_num_2)
{
    int scratch_image_idx;
    int image_idx_1;
    int image_idx_2;
    int rc;

    assert(area_idx_1 != area_idx_2);
    assert(area_idx_1 != boot_req->br_scratch_area_idx);
    assert(area_idx_2 != boot_req->br_scratch_area_idx);

    image_idx_1 = boot_find_image_area_idx(area_idx_1);
    assert(image_idx_1 != -1);

    image_idx_2 = boot_find_image_area_idx(area_idx_2);
    assert(image_idx_2 != -1);

    scratch_image_idx =
        boot_find_image_area_idx(boot_req->br_scratch_area_idx);
    assert(scratch_image_idx != -1);

    rc = boot_erase_area(boot_req->br_scratch_area_idx);
    if (rc != 0) {
        return rc;
    }

    rc = boot_copy_area(area_idx_2, boot_req->br_scratch_area_idx);
    if (rc != 0) {
        return rc;
    }

    boot_status_entries[scratch_image_idx] = boot_status_entries[image_idx_2];
    boot_status_entries[image_idx_2].bse_image_num = BOOT_IMAGE_NUM_NONE;
    boot_status_entries[image_idx_2].bse_part_num = BOOT_IMAGE_NUM_NONE;

    rc = boot_write_status(&boot_status, boot_status_entries,
                           boot_req->br_num_image_areas);
    if (rc != 0) {
        return rc;
    }

    rc = boot_erase_area(area_idx_2);
    if (rc != 0) {
        return rc;
    }

    rc = boot_copy_area(area_idx_1, area_idx_2);
    if (rc != 0) {
        return rc;
    }

    boot_status_entries[image_idx_2] = boot_status_entries[image_idx_1];
    boot_status_entries[image_idx_1].bse_image_num = BOOT_IMAGE_NUM_NONE;
    boot_status_entries[image_idx_1].bse_part_num = BOOT_IMAGE_NUM_NONE;
    rc = boot_write_status(&boot_status, boot_status_entries,
                           boot_req->br_num_image_areas);
    if (rc != 0) {
        return rc;
    }

    rc = boot_erase_area(area_idx_1);
    if (rc != 0) {
        return rc;
    }

    rc = boot_copy_area(boot_req->br_scratch_area_idx, area_idx_1);
    if (rc != 0) {
        return rc;
    }

    boot_status_entries[image_idx_1] = boot_status_entries[scratch_image_idx];
    boot_status_entries[scratch_image_idx].bse_image_num = BOOT_IMAGE_NUM_NONE;
    boot_status_entries[scratch_image_idx].bse_part_num = BOOT_IMAGE_NUM_NONE;
    rc = boot_write_status(&boot_status, boot_status_entries,
                           boot_req->br_num_image_areas);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static int
boot_fill_slot(int img_num, uint32_t img_length, int start_area_idx)
{
    const struct nffs_area_desc *area_desc;
    uint32_t off;
    int dst_image_area_idx;
    int src_area_idx;
    int dst_area_idx;
    int src_img_num;
    int dst_img_num;
    int part_num;
    int rc;

    part_num = 0;
    off = 0;
    while (off < img_length) {
        /* Determine which area contains the current part of the image that
         * we want to boot.
         */
        src_area_idx = boot_find_image_part(img_num, part_num);
        if (src_area_idx == -1) {
            return BOOT_EBADIMAGE;
        }

        /* Determine which area we want to copy the source to. */
        dst_area_idx = start_area_idx + part_num;

        if (src_area_idx != dst_area_idx) {
            /* Determine what is currently in the destination area. */
            dst_image_area_idx = boot_find_image_area_idx(dst_area_idx);

            if (boot_status_entries[dst_image_area_idx].bse_image_num ==
                BOOT_IMAGE_NUM_NONE) {

                /* The destination doesn't contain anything useful; we don't
                 * need to back up its contents.
                 */
                rc = boot_move_area(src_area_idx, dst_area_idx,
                                    img_num, part_num);
            } else {
                /* Swap the two areas. */
                src_img_num = img_num ^ 1;
                dst_img_num = img_num;
                rc = boot_swap_areas(src_area_idx, src_img_num, part_num,
                                     dst_area_idx, dst_img_num, part_num);
            }
            if (rc != 0) {
                return rc;
            }
        }

        area_desc = boot_req->br_area_descs + dst_area_idx;
        off += area_desc->nad_length;

        part_num++;
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
boot_copy_image(uint32_t img1_length, uint32_t img2_length)
{
    int rc;

    rc = boot_fill_slot(1, img2_length, boot_slot_to_area_idx(0));
    if (rc != 0) {
        return rc;
    }

    rc = boot_fill_slot(0, img1_length, boot_slot_to_area_idx(1));
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * Builds a single default boot status for the specified image slot.
 */
static void
boot_build_status_one(int image_num, uint8_t flash_id, uint32_t addr,
                      uint32_t length)
{
    uint32_t offset;
    int area_idx = 0;
    int part_num;
    int i;

    for (i = 0; i < boot_req->br_num_image_areas; i++) {
        area_idx = boot_req->br_image_areas[i];
        if (boot_req->br_area_descs[area_idx].nad_offset == addr &&
            boot_req->br_area_descs[area_idx].nad_flash_id == flash_id) {
            break;
        }
    }

    assert(i < boot_req->br_num_image_areas);

    offset = 0;
    part_num = 0;
    while (offset < length) {
        assert(boot_status_entries[i].bse_image_num == 0xff);
        boot_status_entries[i].bse_image_num = image_num;
        boot_status_entries[i].bse_part_num = part_num;

        offset += boot_req->br_area_descs[area_idx].nad_length;
        part_num++;
        i++;
        area_idx++;
    }

    assert(i <= boot_req->br_num_image_areas);
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
    uint8_t flash_id;
    uint32_t address;

    memset(boot_status_entries, 0xff,
           boot_req->br_num_image_areas * sizeof *boot_status_entries);

    if (boot_img_hdrs[0].ih_magic == IMAGE_MAGIC) {
        boot_status.bs_img1_length = boot_img_hdrs[0].ih_img_size;
        boot_slot_addr(0, &flash_id, &address);
        boot_build_status_one(0, flash_id, address,
                              boot_img_hdrs[0].ih_img_size);
    } else {
        boot_status.bs_img1_length = 0;
    }

    if (boot_img_hdrs[1].ih_magic == IMAGE_MAGIC) {
        boot_status.bs_img2_length = boot_img_hdrs[1].ih_img_size;
        boot_slot_addr(1, &flash_id, &address);
        boot_build_status_one(1, flash_id, address,
                              boot_img_hdrs[1].ih_img_size);
    } else {
        boot_status.bs_img2_length = 0;
    }
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
    void *tmpbuf;
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
    boot_status_entries =
        malloc(req->br_num_image_areas * sizeof *boot_status_entries);
    if (boot_status_entries == NULL) {
        return BOOT_ENOMEM;
    }
    rc = boot_read_status(&boot_status, boot_status_entries,
                          boot_req->br_num_image_areas);
    if (rc == 0) {
        /* We are resuming an interrupted image copy. */
        /* XXX if copy has not actually started yet, validate image */
        rc = boot_copy_image(boot_status.bs_img1_length,
                             boot_status.bs_img2_length);
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
    tmpbuf = malloc(BOOT_TMPBUF_SZ);
    if (!tmpbuf) {
        return BOOT_ENOMEM;
    }
    if (bootutil_img_validate(&boot_img_hdrs[slot],
        image_addrs[slot].bil_flash_id, image_addrs[slot].bil_address,
        tmpbuf, BOOT_TMPBUF_SZ)) {
        return BOOT_EBADIMAGE;
    }

    switch (slot) {
    case 0:
        rsp->br_hdr = &boot_img_hdrs[0];
        break;

    case 1:
        /* The user wants to run the image in the secondary slot.  The contents
         * of this slot need to moved to the primary slot.
         */
        rc = boot_copy_image(boot_status.bs_img1_length,
                             boot_status.bs_img2_length);

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
    fs_unlink(BOOT_PATH_STATUS);

    /* If an image is being tested, it should only be booted into once. */
    boot_vect_delete_test();

    return 0;
}
