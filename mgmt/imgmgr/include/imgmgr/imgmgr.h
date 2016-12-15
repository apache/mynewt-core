/*
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

#ifndef _IMGMGR_H_
#define _IMGMGR_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMGMGR_NMGR_OP_STATE        0
#define IMGMGR_NMGR_OP_UPLOAD       1
#define IMGMGR_NMGR_OP_FILE         2
#define IMGMGR_NMGR_OP_CORELIST     3
#define IMGMGR_NMGR_OP_CORELOAD     4

#define IMGMGR_NMGR_MAX_MSG         400
#define IMGMGR_NMGR_MAX_NAME		64
#define IMGMGR_NMGR_MAX_VER         25  /* 255.255.65535.4294967295\0 */

#define IMGMGR_HASH_LEN             32

extern int boot_current_slot;

struct image_version;

/*
 * Parse version string in src, and fill in ver.
 */
int imgr_ver_parse(char *src, struct image_version *ver);

/*
 * Take version and convert it to string in dst.
 */
int imgr_ver_str(struct image_version *ver, char *dst);

/*
 * Given flash_map slot id, read in image_version and/or image hash.
 */
int imgr_read_info(int area_id, struct image_version *ver, uint8_t *hash, uint32_t *flags);

/*
 * Returns version number of current image (if available).
 */
int imgr_my_version(struct image_version *ver);

uint8_t imgmgr_state_flags(int query_slot);
int imgmgr_state_slot_in_use(int slot);
int imgmgr_state_test_slot(int slot);

#ifdef __cplusplus
}
#endif

#endif /* _IMGMGR_H */
