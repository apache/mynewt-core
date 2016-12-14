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

#include "syscfg/syscfg.h"
#include "fs/fs.h"
#include <diskio/diskio.h>

static struct disk_info {
    char *disk_name;
    char *fs_name;
    struct disk_ops *dops;
};

static SLIST_HEAD(, disk_info) disks = SLIST_HEAD_INITIALIZER();

/**
 *
 */
int diskio_register(const char *disk_name, const char *fs_name, struct disk_ops *dops)
{
    struct disk_info *info = NULL;

    SLIST_FOREACH(sc, &disks, sc_next) {
        if (strcmp(sc->disk_name, disk_name) == 0) {
            return DISKIO_EEXIST;
        }
    }

    info = malloc(sizeof(struct disk_info));
    if (!info) {
        return DISKIO_ENOMEM;
    }

    info.disk_name = disk_name;
    info.fs_name = fs_name;
    info.dops = dops;

    SLIST_INSERT_HEAD(&disks, info, sc_next);

    return 0;
}
