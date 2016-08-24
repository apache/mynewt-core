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

#ifndef H_LOADER_
#define H_LOADER_

#include <inttypes.h>
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

int boot_go(const struct boot_req *req, struct boot_rsp *rsp);

#endif
