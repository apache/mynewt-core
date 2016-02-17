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

#ifndef H_BOOTUTIL_PRIV_
#define H_BOOTUTIL_PRIV_

#include "bootutil/image.h"
struct image_header;

#define BOOT_EFLASH     1
#define BOOT_EFILE      2
#define BOOT_EBADIMAGE  3
#define BOOT_EBADVECT   4
#define BOOT_EBADSTATUS 5
#define BOOT_ENOMEM     6

#define BOOT_IMAGE_NUM_NONE     0xff

#define BOOT_PATH_MAIN      "/boot/main"
#define BOOT_PATH_TEST      "/boot/test"
#define BOOT_PATH_STATUS    "/boot/status"

#define BOOT_TMPBUF_SZ  256

struct boot_status {
    uint32_t bs_img1_length;
    uint32_t bs_img2_length;
    /* Followed by sequence of boot status entries; file size indicates number
     * of entries.
     */
};

struct boot_status_entry {
    uint8_t bse_image_num;
    uint8_t bse_part_num;
};

struct boot_image_location {
    uint8_t bil_flash_id;
    uint32_t bil_address;
};

int boot_vect_read_test(struct image_version *out_ver);
int boot_vect_read_main(struct image_version *out_ver);
int boot_vect_delete_test(void);
int boot_vect_delete_main(void);
void boot_read_image_headers(struct image_header *out_headers,
                             const struct boot_image_location *addresses,
                             int num_addresses);
int boot_read_status(struct boot_status *out_status,
                     struct boot_status_entry *out_entries,
                     int num_areas);
int boot_write_status(const struct boot_status *status,
                      const struct boot_status_entry *entries,
                      int num_areas);
void boot_clear_status(void);

#endif

