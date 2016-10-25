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

const struct fs_ops *fs_root_ops = NULL;

int
fs_register(const struct fs_ops *fops)
{
    if (fs_root_ops) {
        return FS_EEXIST;
    }
    fs_root_ops = fops;

#if MYNEWT_VAL(FS_CLI)
    fs_cli_init();
#endif

    return FS_EOK;
}
