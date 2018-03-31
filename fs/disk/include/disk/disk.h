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

#ifndef __DISK_H__
#define __DISK_H__

#include <stddef.h>
#include <inttypes.h>
#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISK_EOK          0  /* Success */
#define DISK_EHW          1  /* Error accessing storage medium */
#define DISK_ENOMEM       2  /* Insufficient memory */
#define DISK_ENOENT       3  /* No such file or directory */
#define DISK_EOS          4  /* OS error */
#define DISK_EUNINIT      5  /* File system not initialized */

struct disk_ops {
    int (*read)(uint8_t, uint32_t, void *, uint32_t);
    int (*write)(uint8_t, uint32_t, const void *, uint32_t);
    int (*ioctl)(uint8_t, uint32_t, void *);

    SLIST_ENTRY(disk_ops) sc_next;
};

int disk_register(const char *disk_name, const char *fs_name, struct disk_ops *dops);
struct disk_ops *disk_ops_for(const char *disk_name);
char *disk_fs_for(const char *disk_name);
char *disk_name_from_path(const char *path);
char *disk_filepath_from_path(const char *path);

#ifdef __cplusplus
}
#endif

#endif
