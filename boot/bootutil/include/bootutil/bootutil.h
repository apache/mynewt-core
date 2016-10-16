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

#ifndef H_BOOTUTIL_
#define H_BOOTUTIL_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOOT_STATUS_SOURCE_NONE    0
#define BOOT_STATUS_SOURCE_SCRATCH 1
#define BOOT_STATUS_SOURCE_SLOT0   2

#define BOOT_SWAP_TYPE_NONE     0
#define BOOT_SWAP_TYPE_TEST     1
#define BOOT_SWAP_TYPE_REVERT   2

int boot_status_source(void);
int boot_swap_type(void);
int boot_partial_swap_type(void);

int boot_vect_read_test(int *slot);
int boot_vect_read_main(int *slot);
int boot_set_pending(int slot);
int boot_set_confirmed(void);

void boot_set_image_slot_split(void);

struct image_header;

/** A request object instructing the boot loader how to proceed. */
struct boot_req {
    /**
     * Array of area descriptors indicating the layout of flash(es); must
     * be terminated with a 0-length element.
     */
    struct flash_area *br_area_descs;

    /**
     * Array of indices of elements in the br_area_descs array; indicates which
     * areas represent the beginning of an image slot.  These are indices
     * to br_area_descs array.
     */
    uint8_t *br_slot_areas;

    /**
     * The number of image areas (i.e., the size of the br_image_areas array).
     */
    uint8_t br_num_image_areas;

    /** The area to use as the image scratch area, index is
	index to br_area_descs array, of the  */
    uint8_t br_scratch_area_idx;

    /** Size of the image slot */
    uint32_t br_img_sz;
};

/**
 * A response object provided by the boot loader code; indicates where to jump
 * to execute the main image.
 */
struct boot_rsp {
    /** A pointer to the header of the image to be executed. */
    const struct image_header *br_hdr;

    /**
     * The flash offset of the image to execute.  Indicates the position of
     * the image header.
     */
    uint8_t br_flash_id;
    uint32_t br_image_addr;
};

/* you must have pre-allocated all the entries within this structure */
int
boot_build_request(struct boot_req *preq, int area_descriptor_max);

int
boot_go(const struct boot_req *req, struct boot_rsp *rsp);


#define SPLIT_GO_OK                 (0)
#define SPLIT_GO_NON_MATCHING       (-1)
#define SPLIT_GO_ERR                (-2)
int
split_go(int loader_slot, int split_slot, void **entry);

#ifdef __cplusplus
}
#endif

#endif
