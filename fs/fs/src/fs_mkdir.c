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
#include <fs/fs.h>
#include <fs/fs_if.h>

#include "fs_priv.h"

int
fs_rename(const char *from, const char *to)
{
    const file_system_t *fs1;
    const file_system_t *fs2;
    const char *fs_from;
    const char *fs_to;

    get_file_system_path(from, &fs1, &fs_from);
    get_file_system_path(to, &fs2, &fs_to);

    if (fs1 == fs2) {
        return fs1->ops->f_rename(fs_from, fs_to);
    }

    return FS_EINVAL;
}

int
fs_mkdir(const char *path)
{
    const file_system_t *fs;
    const char *fs_path;

    get_file_system_path(path, &fs, &fs_path);

    return fs->ops->f_mkdir(fs_path);
}
