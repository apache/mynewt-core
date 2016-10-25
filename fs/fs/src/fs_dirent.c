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
fs_opendir(const char *path, struct fs_dir **out_dir)
{
    return fs_root_ops->f_opendir(path, out_dir);
}

int
fs_readdir(struct fs_dir *dir, struct fs_dirent **out_dirent)
{
    return fs_root_ops->f_readdir(dir, out_dirent);
}

int
fs_closedir(struct fs_dir *dir)
{
    return fs_root_ops->f_closedir(dir);
}

int
fs_dirent_name(const struct fs_dirent *dirent, size_t max_len,
  char *out_name, uint8_t *out_name_len)
{
    return fs_root_ops->f_dirent_name(dirent, max_len, out_name, out_name_len);
}

int
fs_dirent_is_dir(const struct fs_dirent *dirent)
{
    return fs_root_ops->f_dirent_is_dir(dirent);
}
