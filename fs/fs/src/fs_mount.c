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

static SLIST_HEAD(, fs_ops) root_fops = SLIST_HEAD_INITIALIZER();

#if MYNEWT_VAL(FS_MGMT)
static uint8_t g_mgmt_initialized;
#endif

int
fs_register(struct fs_ops *fops)
{
    struct fs_ops *sc;

    SLIST_FOREACH(sc, &root_fops, sc_next) {
        if (strcmp(sc->f_name, fops->f_name) == 0) {
            return FS_EEXIST;
        }
    }

    SLIST_INSERT_HEAD(&root_fops, fops, sc_next);

#if MYNEWT_VAL(FS_MGMT)
    if (!g_mgmt_initialized) {
        fs_mgmt_register_group();
        g_mgmt_initialized = 1;
    }
#endif

    return FS_EOK;
}

struct fs_ops *
fs_ops_try_unique(void)
{
    struct fs_ops *fops = SLIST_FIRST(&root_fops);

    if (fops && !SLIST_NEXT(fops, sc_next)) {
        return fops;
    }

    return NULL;
}

struct fs_ops *
fs_ops_for(const char *fs_name)
{
    struct fs_ops *fops = NULL;
    struct fs_ops *sc;

    if (fs_name) {
        SLIST_FOREACH(sc, &root_fops, sc_next) {
            if (strcmp(sc->f_name, fs_name) == 0) {
                fops = sc;
                break;
            }
        }
    }

    return fops;
}

extern struct fs_ops not_initialized_ops;

struct fs_ops *
fs_ops_from_container(struct fops_container *container)
{
    if (!container) {
        return &not_initialized_ops;
    }
    return container->fops;
}
