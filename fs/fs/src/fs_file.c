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
fs_open(const char *filename, uint8_t access_flags, struct fs_file **out_file)
{
    return fs_root_ops->f_open(filename, access_flags, out_file);
}

int
fs_close(struct fs_file *file)
{
    return fs_root_ops->f_close(file);
}

int
fs_read(struct fs_file *file, uint32_t len, void *out_data, uint32_t *out_len)
{
    return fs_root_ops->f_read(file, len, out_data, out_len);
}

int
fs_write(struct fs_file *file, const void *data, int len)
{
    return fs_root_ops->f_write(file, data, len);
}

int
fs_seek(struct fs_file *file, uint32_t offset)
{
    return fs_root_ops->f_seek(file, offset);
}

uint32_t
fs_getpos(const struct fs_file *file)
{
    return fs_root_ops->f_getpos(file);
}

int
fs_filelen(const struct fs_file *file, uint32_t *out_len)
{
    return fs_root_ops->f_filelen(file, out_len);
}

int
fs_unlink(const char *filename)
{
    return fs_root_ops->f_unlink(filename);
}
