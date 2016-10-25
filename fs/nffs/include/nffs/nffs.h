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

#ifndef H_NFFS_
#define H_NFFS_

#include <stddef.h>
#include <inttypes.h>
#include "fs/fs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NFFS_FILENAME_MAX_LEN   256  /* Does not require null terminator. */
#define NFFS_MAX_AREAS          256

struct nffs_config {
    /** Maximum number of inodes; default=1024. */
    uint32_t nc_num_inodes;

    /** Maximum number of data blocks; default=4096. */
    uint32_t nc_num_blocks;

    /** Maximum number of open files; default=4. */
    uint32_t nc_num_files;

    /** Maximum number of open directories; default=4. */
    uint32_t nc_num_dirs;

    /** Inode cache size; default=4. */
    uint32_t nc_num_cache_inodes;

    /** Data block cache size; default=64. */
    uint32_t nc_num_cache_blocks;
};

extern struct nffs_config nffs_config;

struct nffs_area_desc {
    uint32_t nad_offset;    /* Flash offset of start of area. */
    uint32_t nad_length;    /* Size of area, in bytes. */
    uint8_t nad_flash_id;   /* Logical flash id */
};

int nffs_init(void);
int nffs_detect(const struct nffs_area_desc *area_descs);
int nffs_format(const struct nffs_area_desc *area_descs);

int nffs_misc_desc_from_flash_area(int idx, int *cnt, struct nffs_area_desc *nad);

#ifdef __cplusplus
}
#endif

#endif
