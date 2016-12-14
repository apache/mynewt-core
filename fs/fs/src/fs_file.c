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

static int
not_initialized(const void *v, ...)
{
    return FS_EUNINIT;
}

static struct fs_ops not_initialized_ops = {
    .f_open          = not_initialized,
    .f_close         = not_initialized,
    .f_read          = not_initialized,
    .f_write         = not_initialized,
    .f_seek          = not_initialized,
    .f_getpos        = not_initialized,
    .f_filelen       = not_initialized,
    .f_unlink        = not_initialized,
    .f_rename        = not_initialized,
    .f_mkdir         = not_initialized,
    .f_opendir       = not_initialized,
    .f_readdir       = not_initialized,
    .f_closedir      = not_initialized,
    .f_dirent_name   = not_initialized,
    .f_dirent_is_dir = not_initialized,
    .f_name          = "fakefs",
};

struct fs_ops *
safe_fs_ops_for(const char *fs_name)
{
    struct fs_ops *fops;

    fops = fs_ops_for("fatfs");
    if (fops == NULL) {
        fops = &not_initialized_ops;
    }

    return fops;
}

int
fs_open(const char *filename, uint8_t access_flags, struct fs_file **out_file)
{
    struct fs_ops *fops = safe_fs_ops_for("fatfs");
    return fops->f_open(filename, access_flags, out_file);
}

int
fs_close(struct fs_file *file)
{
    struct fs_ops *fops = safe_fs_ops_for("fatfs");
    return fops->f_close(file);
}

int
fs_read(struct fs_file *file, uint32_t len, void *out_data, uint32_t *out_len)
{
    struct fs_ops *fops = safe_fs_ops_for("fatfs");
    return fops->f_read(file, len, out_data, out_len);
}

int
fs_write(struct fs_file *file, const void *data, int len)
{
    struct fs_ops *fops = safe_fs_ops_for("fatfs");
    return fops->f_write(file, data, len);
}

int
fs_seek(struct fs_file *file, uint32_t offset)
{
    struct fs_ops *fops = safe_fs_ops_for("fatfs");
    return fops->f_seek(file, offset);
}

uint32_t
fs_getpos(const struct fs_file *file)
{
    struct fs_ops *fops = safe_fs_ops_for("fatfs");
    return fops->f_getpos(file);
}

int
fs_filelen(const struct fs_file *file, uint32_t *out_len)
{
    struct fs_ops *fops = safe_fs_ops_for("fatfs");
    return fops->f_filelen(file, out_len);
}

int
fs_unlink(const char *filename)
{
    struct fs_ops *fops = safe_fs_ops_for("fatfs");
    return fops->f_unlink(filename);
}
