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

#include "mgmt/mgmt.h"
#include "fs_mgmt/fs_mgmt_impl.h"
#include "fs/fs.h"

int
fs_mgmt_impl_filelen(const char *path, size_t *out_len)
{
    struct fs_file *file;
    uint32_t file_size;
    int rc;

    rc = fs_open(path, FS_ACCESS_READ, &file);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    rc = fs_filelen(file, &file_size);
    fs_close(file);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    *out_len = file_size;

    return 0;
}

int
fs_mgmt_impl_read(const char *path, size_t offset, size_t len,
                  void *out_data, size_t *out_len)
{
    struct fs_file *file;
    uint32_t bytes_read;
    int rc;

    rc = fs_open(path, FS_ACCESS_READ, &file);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    rc = fs_seek(file, offset);
    if (rc != 0) {
        goto done;
    }

    rc = fs_read(file, len, out_data, &bytes_read);
    if (rc != 0) {
        goto done;
    }

    *out_len = bytes_read;

done:
    fs_close(file);

    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    return 0;
}

int
fs_mgmt_impl_write(const char *path, size_t offset, const void *data,
                   size_t len)
{
    struct fs_file *file;
    uint8_t access;
    int rc;
 
    access = FS_ACCESS_WRITE;
    if (offset == 0) {
        access |= FS_ACCESS_TRUNCATE;
    } else {
        access |= FS_ACCESS_APPEND;
    }

    rc = fs_open(path, access, &file);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    rc = fs_write(file, data, len);
    fs_close(file);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    return 0;
}
