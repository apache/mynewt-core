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

struct fs_ops *fops_from_filename(const char *);

static const struct fs_ops *
fops_from_dir(const struct fs_dir *dir)
{
    return fs_ops_from_container((struct fops_container *) dir);
}

static inline const struct fs_ops *
fops_from_dirent(const struct fs_dirent *dirent)
{
    return fs_ops_from_container((struct fops_container *) dirent);
}

int
fs_opendir(const char *path, struct fs_dir **out_dir)
{
    const file_system_t *fs;
    const char *fs_path;
    get_file_system_path(path, &fs, &fs_path);

    return fs->ops->f_opendir(fs_path, out_dir);
}

int
fs_readdir(struct fs_dir *dir, struct fs_dirent **out_dirent)
{
    const struct fs_ops *fops = fops_from_dir(dir);
    return fops->f_readdir(dir, out_dirent);
}

int
fs_closedir(struct fs_dir *dir)
{
    const struct fs_ops *fops = fops_from_dir(dir);
    return fops->f_closedir(dir);
}

int
fs_dirent_name(const struct fs_dirent *dirent, size_t max_len,
  char *out_name, uint8_t *out_name_len)
{
    const struct fs_ops *fops = fops_from_dirent(dirent);
    return fops->f_dirent_name(dirent, max_len, out_name, out_name_len);
}

int
fs_dirent_is_dir(const struct fs_dirent *dirent)
{
    const struct fs_ops *fops = fops_from_dirent(dirent);
    return fops->f_dirent_is_dir(dirent);
}
