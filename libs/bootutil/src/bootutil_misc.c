/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <inttypes.h>
#include "hal/hal_flash.h"
#include "fs/fs.h"
#include "fs/fsutil.h"
#include "bootutil/crc32.h"
#include "bootutil/image.h"
#include "bootutil_priv.h"

static int
boot_vect_read_one(struct image_version *ver, const char *path)
{
    uint32_t bytes_read;
    int rc;

    rc = fsutil_read_file(path, 0, sizeof *ver, ver, &bytes_read);
    if (rc != 0 || bytes_read != sizeof *ver) {
        return BOOT_EBADVECT;
    }

    return 0;
}

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
    int rc;

    rc = boot_vect_read_one(out_ver, BOOT_PATH_TEST);
    return rc;
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
    int rc;

    rc = boot_vect_read_one(out_ver, BOOT_PATH_MAIN);
    return rc;
}

/**
 * Deletes the test image version number from the boot vector.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_delete_test(void)
{
    int rc;

    rc = fs_unlink(BOOT_PATH_TEST);
    return rc;
}

/**
 * Deletes the main image version number from the boot vector.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_delete_main(void)
{
    int rc;

    rc = fs_unlink(BOOT_PATH_MAIN);
    return rc;
}

static int
boot_read_image_header(struct image_header *out_hdr,
                       const struct boot_image_location *loc)
{
    int rc;

    rc = hal_flash_read(loc->bil_flash_id, loc->bil_address, out_hdr,
                        sizeof *out_hdr);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    if (out_hdr->ih_magic != IMAGE_MAGIC) {
        return BOOT_EBADIMAGE;
    }

    return 0;
}

/**
 * Reads the header of each image present in flash.  Headers corresponding to
 * empty image slots are filled with 0xff bytes.
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
void
boot_read_image_headers(struct image_header *out_headers,
                        const struct boot_image_location *addresses,
                        int num_addresses)
{
    struct image_header *hdr;
    int rc;
    int i;

    for (i = 0; i < num_addresses; i++) {
        hdr = out_headers + i;
        rc = boot_read_image_header(hdr, &addresses[i]);
        if (rc != 0 || hdr->ih_magic != IMAGE_MAGIC) {
            memset(hdr, 0xff, sizeof *hdr);
        }
    }
}

/**
 * Reads the boot status from the flash file system.  The boot status contains
 * the current state of an interrupted image copy operation.  If the boot
 * status is not present in the file system, the implication is that there is
 * no copy operation in progress.
 *
 * @param out_status            On success, the boot status gets written here.
 * @param out_entries           On success, the array of boot entries gets
 *                                  written here.
 * @param num_areas             The number of flash areas capable of storing
 *                                  image data.  This is equal to the length of
 *                                  the out_entries array.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
boot_read_status(struct boot_status *out_status,
                 struct boot_status_entry *out_entries,
                 int num_areas)
{
    struct fs_file *file;
    uint32_t bytes_read;
    int rc;
    int i;

    rc = fs_open(BOOT_PATH_STATUS, FS_ACCESS_READ, &file);
    if (rc != 0) {
        rc = BOOT_EBADSTATUS;
        goto done;
    }

    rc = fs_read(file, sizeof *out_status, out_status, &bytes_read);
    if (rc != 0 || bytes_read != sizeof *out_status) {
        rc = BOOT_EBADSTATUS;
        goto done;
    }

    rc = fs_read(file, num_areas * sizeof *out_entries, out_entries,
                   &bytes_read);
    if (rc != 0 || bytes_read != num_areas * sizeof *out_entries) {
        rc = BOOT_EBADSTATUS;
        goto done;
    }

    if (out_status->bs_img1_length == 0xffffffff) {
        out_status->bs_img1_length = 0;
    }
    if (out_status->bs_img2_length == 0xffffffff) {
        out_status->bs_img2_length = 0;
    }

    for (i = 0; i < num_areas; i++) {
        if (out_entries[i].bse_image_num == 0 &&
            out_status->bs_img1_length == 0) {

            rc = BOOT_EBADSTATUS;
            goto done;
        }
        if (out_entries[i].bse_image_num == 1 &&
            out_status->bs_img2_length == 0) {

            rc = BOOT_EBADSTATUS;
            goto done;
        }
    }

    rc = 0;

done:
    fs_close(file);
    return rc;
}

/**
 * Writes the supplied boot status to the flash file system.  The boot status
 * contains the current state of an in-progress image copy operation.
 *
 * @param status                The boot status base to write.
 * @param entries               The array of boot status entries to write.
 * @param num_areas             The number of flash areas capable of storing
 *                                  image data.  This is equal to the length of
 *                                  the entries array.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
boot_write_status(const struct boot_status *status,
                  const struct boot_status_entry *entries,
                  int num_areas)
{
    struct fs_file *file;
    int rc;

    rc = fs_open(BOOT_PATH_STATUS, FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE, &file);
    if (rc != 0) {
        rc = BOOT_EFILE;
        goto done;
    }

    rc = fs_write(file, status, sizeof *status);
    if (rc != 0) {
        rc = BOOT_EFILE;
        goto done;
    }

    rc = fs_write(file, entries, num_areas * sizeof *entries);
    if (rc != 0) {
        rc = BOOT_EFILE;
        goto done;
    }

    rc = 0;

done:
    fs_close(file);
    return rc;
}

/**
 * Erases the boot status from the flash file system.  The boot status
 * contains the current state of an in-progress image copy operation.  By
 * erasing the boot status, it is implied that there is no copy operation in
 * progress.
 */
void
boot_clear_status(void)
{
    fs_unlink(BOOT_PATH_STATUS);
}
