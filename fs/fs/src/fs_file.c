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

#include <string.h>
#include <stdlib.h>

#include "fs_priv.h"

static int
fake_open(const char *filename, uint8_t access_flags,
          struct fs_file **out_file)
{
    return FS_EUNINIT;
}

static int
fake_close(struct fs_file *file)
{
    return FS_EUNINIT;
}

static int
fake_read(struct fs_file *file, uint32_t len, void *out_data,
          uint32_t *out_len)
{
    return FS_EUNINIT;
}

static int
fake_write(struct fs_file *file, const void *data, int len)
{
    return FS_EUNINIT;
}

static int
fake_flush(struct fs_file *file)
{
    return FS_EUNINIT;
}

static int
fake_seek(struct fs_file *file, uint32_t offset)
{
    return FS_EUNINIT;
}

static uint32_t
fake_getpos(const struct fs_file *file)
{
    return FS_EUNINIT;
}

static int
fake_filelen(const struct fs_file *file, uint32_t *out_len)
{
    return FS_EUNINIT;
}

static int
fake_unlink(const char *filename)
{
    return FS_EUNINIT;
}

static int
fake_rename(const char *from, const char *to)
{
    return FS_EUNINIT;
}

static int
fake_mkdir(const char *path)
{
    return FS_EUNINIT;
}

static int
fake_opendir(const char *path, struct fs_dir **out_dir)
{
    return FS_EUNINIT;
}

static int
fake_readdir(struct fs_dir *dir, struct fs_dirent **out_dirent)
{
    return FS_EUNINIT;
}

static int
fake_closedir(struct fs_dir *dir)
{
    return FS_EUNINIT;
}

static int fake_dirent_name(const struct fs_dirent *dirent, size_t max_len,
                            char *out_name, uint8_t *out_name_len)
{
    return FS_EUNINIT;
}

static int
fake_dirent_is_dir(const struct fs_dirent *dirent)
{
    return FS_EUNINIT;
}

const struct fs_ops not_initialized_ops = {
    .f_open          = &fake_open,
    .f_close         = &fake_close,
    .f_read          = &fake_read,
    .f_write         = &fake_write,
    .f_flush         = &fake_flush,
    .f_seek          = &fake_seek,
    .f_getpos        = &fake_getpos,
    .f_filelen       = &fake_filelen,
    .f_unlink        = &fake_unlink,
    .f_rename        = &fake_rename,
    .f_mkdir         = &fake_mkdir,
    .f_opendir       = &fake_opendir,
    .f_readdir       = &fake_readdir,
    .f_closedir      = &fake_closedir,
    .f_dirent_name   = &fake_dirent_name,
    .f_dirent_is_dir = &fake_dirent_is_dir,
};

const file_system_t fake_fs = {
    .ops = &not_initialized_ops,
    .name = "fakefs",
};

const struct fs_ops *
safe_fs_ops_for(const char *fs_name)
{
    const struct fs_ops *fops;

    fops = fs_ops_for(fs_name);
    if (fops == NULL) {
        fops = &not_initialized_ops;
    }

    return fops;
}

void
get_file_system_path(const char *path, const file_system_t **fs, const char **fs_path)
{
    *fs_path = file_system_path(path, fs);
    if (*fs_path == NULL) {
        *fs = get_only_file_system();
        if (*fs == NULL) {
            *fs = &fake_fs;
        }
        *fs_path = path;
    }
}

static inline const struct fs_ops *
fops_from_file(const struct fs_file *file)
{
    return fs_ops_from_container((struct fops_container *) file);
}

int
fs_open(const char *filename, uint8_t access_flags, struct fs_file **out_file)
{
    const file_system_t *fs;
    const char *fs_filename;

    get_file_system_path(filename, &fs, &fs_filename);
    return fs->ops->f_open(fs_filename, access_flags, out_file);
}

int
fs_close(struct fs_file *file)
{
    const struct fs_ops *fops = fops_from_file(file);
    return fops->f_close(file);
}

int
fs_read(struct fs_file *file, uint32_t len, void *out_data, uint32_t *out_len)
{
    const struct fs_ops *fops = fops_from_file(file);
    return fops->f_read(file, len, out_data, out_len);
}

int
fs_write(struct fs_file *file, const void *data, int len)
{
    const struct fs_ops *fops = fops_from_file(file);
    return fops->f_write(file, data, len);
}

int
fs_seek(struct fs_file *file, uint32_t offset)
{
    const struct fs_ops *fops = fops_from_file(file);
    return fops->f_seek(file, offset);
}

uint32_t
fs_getpos(const struct fs_file *file)
{
    const struct fs_ops *fops = fops_from_file(file);
    return fops->f_getpos(file);
}

int
fs_filelen(const struct fs_file *file, uint32_t *out_len)
{
    const struct fs_ops *fops = fops_from_file(file);
    return fops->f_filelen(file, out_len);
}

int
fs_unlink(const char *filename)
{
    const file_system_t *fs;
    const char *fs_filename;

    get_file_system_path(filename, &fs, &fs_filename);
    return fs->ops->f_unlink(fs_filename);
}

int
fs_flush(struct fs_file *file)
{
    const struct fs_ops *fops = fops_from_file(file);
    return fops->f_flush(file);
}
