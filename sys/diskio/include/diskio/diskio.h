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

#ifndef __DISKIO_H__
#define __DISKIO_H__

#include <stddef.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISKIO_EOK          0  /* Success */
#define DISKIO_EHW          1  /* Error accessing storage medium */
#define DISKIO_ENOMEM       2  /* Insufficient memory */
#define DISKIO_ENOENT       3  /* No such file or directory */
#define DISKIO_EOS          4  /* OS error */
#define DISKIO_EUNINIT      5  /* File system not initialized */

struct disk_ops {
    int (*read)(int, uint32_t, void *, uint32_t);
    int (*write)(int, uint32_t, const void *, uint32_t);
    int (*ioctl)(int, uint32_t, const void *);

    SLIST_ENTRY(disk_ops) sc_next;
};

int diskio_register(const char *disk_name, const char *fs_name, struct disk_ops *dops);

#ifdef __cplusplus
}
#endif

#endif
