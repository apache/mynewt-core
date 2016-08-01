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
#include <config/config.h>
#include <os/os.h>
#include "bootutil/image.h"
#include "bootutil_priv.h"

#ifdef USE_STATUS_FILE
#include <fs/fs.h>
#include <fs/fsutil.h>
#endif

static int boot_conf_set(int argc, char **argv, char *val);

static struct image_version boot_main;
static struct image_version boot_test;
#ifndef USE_STATUS_FILE
static struct boot_status boot_saved;
#endif

static struct conf_handler boot_conf_handler = {
    .ch_name = "boot",
    .ch_get = NULL,
    .ch_set = boot_conf_set,
    .ch_commit = NULL,
    .ch_export = NULL,
};

static int
boot_conf_set(int argc, char **argv, char *val)
{
    int rc;
    int len;

    if (argc == 1) {
        if (!strcmp(argv[0], "main")) {
            len = sizeof(boot_main);
            if (val) {
                rc = conf_bytes_from_str(val, &boot_main, &len);
            } else {
                memset(&boot_main, 0, len);
                rc = 0;
            }
        } else if (!strcmp(argv[0], "test")) {
            len = sizeof(boot_test);
            if (val) {
                rc = conf_bytes_from_str(val, &boot_test, &len);
            } else {
                memset(&boot_test, 0, len);
                rc = 0;
            }
#ifndef USE_STATUS_FILE
        } else if (!strcmp(argv[0], "status")) {
            if (!val) {
                boot_saved.state = 0;
                rc = 0;
            } else {
                rc = conf_value_from_str(val, CONF_INT32,
                  &boot_saved.state, sizeof(boot_saved.state));
            }
        } else if (!strcmp(argv[0], "len")) {
            conf_value_from_str(val, CONF_INT32, &boot_saved.length,
              sizeof(boot_saved.length));
            rc = 0;
#endif
        } else {
            rc = OS_ENOENT;
        }
    } else {
        rc = OS_ENOENT;
    }
    return rc;
}

static int
boot_vect_read_one(struct image_version *dst, struct image_version *src)
{
    if (src->iv_major == 0 && src->iv_minor == 0 &&
      src->iv_revision == 0 && src->iv_build_num == 0) {
        return BOOT_EBADVECT;
    }
    memcpy(dst, src, sizeof(*dst));
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
    return boot_vect_read_one(out_ver, &boot_test);
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
    return boot_vect_read_one(out_ver, &boot_main);
}

static int
boot_vect_write_one(const char *name, struct image_version *ver)
{
    char str[CONF_STR_FROM_BYTES_LEN(sizeof(struct image_version))];
    char *to_store;

    if (!ver) {
        to_store = NULL;
    } else {
        if (!conf_str_from_bytes(ver, sizeof(*ver), str, sizeof(str))) {
            return -1;
        }
        to_store = str;
    }
    return conf_save_one(name, to_store);
}

/**
 * Write the test image version number from the boot vector.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_write_test(struct image_version *ver)
{
    if (!ver) {
        memset(&boot_test, 0, sizeof(boot_test));
        return boot_vect_write_one("boot/test", NULL);
    } else {
        memcpy(&boot_test, ver, sizeof(boot_test));
        return boot_vect_write_one("boot/test", &boot_test);
    }
}

/**
 * Deletes the main image version number from the boot vector.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
boot_vect_write_main(struct image_version *ver)
{
    if (!ver) {
        memset(&boot_main, 0, sizeof(boot_main));
        return boot_vect_write_one("boot/main", NULL);
    } else {
        memcpy(&boot_main, ver, sizeof(boot_main));
        return boot_vect_write_one("boot/main", &boot_main);
    }
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

void
bootutil_cfg_register(void)
{
    conf_register(&boot_conf_handler);
}

#ifndef USE_STATUS_FILE
int
boot_read_status(struct boot_status *bs)
{
    conf_load();

    *bs = boot_saved;
    return (boot_saved.state != 0);
}

/**
 * Writes the supplied boot status to the flash file system.  The boot status
 * contains the current state of an in-progress image copy operation.
 *
 * @param status                The boot status base to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
boot_write_status(struct boot_status *bs)
{
    char str[12];
    int rc;

    rc = conf_save_one("boot/len",
      conf_str_from_value(CONF_INT32, &bs->length, str, sizeof(str)));
    if (rc) {
        return rc;
    }
    return conf_save_one("boot/status",
      conf_str_from_value(CONF_INT32, &bs->state, str, sizeof(str)));
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
    conf_save_one("boot/status", NULL);
}

#else

/**
 * Reads the boot status from the flash file system.  The boot status contains
 * the current state of an interrupted image copy operation.  If the boot
 * status is not present in the file system, the implication is that there is
 * no copy operation in progress.
 */
int
boot_read_status(struct boot_status *bs)
{
    int rc;
    uint32_t bytes_read;

    conf_load();

    rc = fsutil_read_file(BOOT_PATH_STATUS, 0, sizeof(*bs),
      bs, &bytes_read);
    if (rc || bytes_read != sizeof(*bs)) {
        memset(bs, 0, sizeof(*bs));
        return 0;
    }
    return 1;
}

int
boot_write_status(struct boot_status *bs)
{
    int rc;

    /*
     * XXX point of failure.
     */
    rc = fsutil_write_file(BOOT_PATH_STATUS, bs, sizeof(*bs));
    if (rc) {
        rc = BOOT_EFILE;
    }
    return rc;
}

void
boot_clear_status(void)
{
    fs_unlink(BOOT_PATH_STATUS);
}
#endif
