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

#include "fs/fs.h"
#include "fs/fs_if.h"
#include "fs_priv.h"
#include <string.h>

#if MYNEWT_VAL(FS_MGMT)
#include <fs_mgmt/fs_mgmt.h>
#endif

#define MAX_MOUNT_POINTS MYNEWT_VAL_FS_MAX_MOUNT_POINTS

struct mount_point mount_points[MAX_MOUNT_POINTS];

int
fs_mount(const file_system_t *fs, const char *mount_point)
{
    int free_slot = -1;
    int rc = FS_EOK;

    for (int i = 0; i < ARRAY_SIZE(mount_points); ++i) {
        if (mount_points[i].mount_point == NULL) {
            if (free_slot < 0) {
                free_slot = i;
            }
        } else if (strcmp(mount_point, mount_points[i].mount_point) == 0) {
            rc = FS_EEXIST;
            goto end;
        }
    }
    if (free_slot >= 0) {
        mount_points[free_slot].mount_point = mount_point;
        mount_points[free_slot].fs = fs;
        rc = fs->ops->f_mount ? fs->ops->f_mount(fs) : 0;
        if (rc) {
            mount_points[free_slot].mount_point = NULL;
            mount_points[free_slot].fs = NULL;
        }
    } else {
        rc = FS_ENOMEM;
    }
end:
    return rc;
}

const file_system_t *
fs_unmount_mount_point(const char *mount_point)
{
    const file_system_t *fs = NULL;

    for (int i = 0; i < ARRAY_SIZE(mount_points); ++i) {
        if (strcmp(mount_point, mount_points[i].mount_point) == 0) {
            fs = mount_points[i].fs;
            mount_points[i].mount_point = NULL;
            mount_points[i].fs = NULL;
            break;
        }
    }

    return fs;
}

int
fs_unmount_file_system(const file_system_t *fs)
{
    int rc = FS_EINVAL;

    for (int i = 0; i < ARRAY_SIZE(mount_points); ++i) {
        if (mount_points[i].fs == fs) {
            mount_points[i].mount_point = NULL;
            mount_points[i].fs = NULL;
            rc = 0;
            break;
        }
    }

    return rc;
}

#if MYNEWT_VAL(FS_MGMT)
static uint8_t g_mgmt_initialized;
#endif

int
fs_register(struct fs_ops *fops)
{
    return FS_EINVAL;
}

const file_system_t *
get_only_file_system(void)
{
    const file_system_t *fs = NULL;

    for (int i = 0; i < ARRAY_SIZE(mount_points); ++i) {
        if (mount_points[i].mount_point == NULL) {
            continue;
        }
        if (fs != NULL) {
            fs = NULL;
            break;
        }
        fs = mount_points[i].fs;
    }

    return fs;
}

const char *
file_system_path(const char *uri, const file_system_t **fs)
{
    const char *fs_path = NULL;
    const char *m;

    for (int i = 0; i < ARRAY_SIZE(mount_points); ++i) {
        if (mount_points[i].mount_point != NULL) {
            fs_path = uri;
            m = mount_points[i].mount_point;
            while (*fs_path == *m) {
                ++fs_path;
                ++m;
            }
            /* Reached end of mount point path ? */
            if (*m == '\0') {
                if (fs) {
                    *fs = mount_points[i].fs;
                }
                break;
            }
            fs_path = NULL;
        }
    }
    return fs_path;
}

extern const struct fs_ops not_initialized_ops;

const struct fs_ops *
fs_ops_from_container(struct fops_container *container)
{
    if (!container) {
        return &not_initialized_ops;
    }
    return container->fops;
}
