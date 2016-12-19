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
#include "fs/fs_if.h"
#include "fs_priv.h"
#include <string.h>

static SLIST_HEAD(, fs_ops) root_fops = SLIST_HEAD_INITIALIZER();

#if MYNEWT_VAL(FS_CLI)
static int g_cli_initialized = 0;
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

#if MYNEWT_VAL(FS_CLI)
    if (!g_cli_initialized) {
        fs_cli_init();
        g_cli_initialized = 1;
    }
#endif

    return FS_EOK;
}

struct fs_ops *
fs_ops_for(const char *fs_name)
{
    struct fs_ops *fops = NULL;
    struct fs_ops *sc;

    SLIST_FOREACH(sc, &root_fops, sc_next) {
        if (strcmp(sc->f_name, fs_name) == 0) {
            fops = sc;
            break;
        }
    }

    return fops;
}
