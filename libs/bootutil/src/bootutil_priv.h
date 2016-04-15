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
    uint32_t length;
    uint32_t state;
};

/**
 * The boot status header read from the file system, or generated if not
 * present on disk.  The boot status indicates the state of the image slots in
 * case the system was restarted while images were being moved in flash.
 */

struct boot_image_location {
    uint8_t bil_flash_id;
    uint32_t bil_address;
};

void boot_read_image_headers(struct image_header *out_headers,
                             const struct boot_image_location *addresses,
                             int num_addresses);
int boot_read_status(struct boot_status *);
int boot_write_status(struct boot_status *);
void boot_clear_status(void);

int bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, int slen,
    uint8_t key_id);

#endif

