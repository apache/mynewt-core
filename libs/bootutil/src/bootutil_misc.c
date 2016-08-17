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

#include <string.h>
#include <inttypes.h>
#include <hal/hal_flash.h>
#include <hal/flash_map.h>
#include <os/os.h>
#include "bootutil/image.h"
#include "bootutil_priv.h"

/**
 * Retrieves from the boot vector the version number of the test image (i.e.,
 * the image that has not been proven stable, and which will only run once).
 *
 * @param out_ver           On success, the test version gets written here.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_read_test(struct image_version *out_ver)
{
    return 0;
}

/**
 * Retrieves from the boot vector the version number of the main image.
 *
 * @param out_ver           On success, the main version gets written here.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_read_main(struct image_version *out_ver)
{
    return 0;
}

/**
 * Write the test image version number from the boot vector.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_write_test(int slot)
{
    const struct flash_area *fap;
    uint32_t off;
    uint32_t magic;
    int rc;

    rc = flash_area_open(slot, &fap);
    if (rc) {
        return rc;
    }

    off = fap->fa_size - sizeof(struct boot_img_trailer);
    magic = BOOT_IMG_MAGIC;

    return flash_area_write(fap, off, &magic, sizeof(magic));
}

/**
 * Deletes the main image version number from the boot vector.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_write_main(struct image_version *ver)
{
    return 0;
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

