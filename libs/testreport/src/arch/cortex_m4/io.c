/**
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

#include <stddef.h>
#include "nffs/nffs.h"
#include "nffs/nffsutil.h"
#include "testreport_priv.h"

int
tr_io_write(const char *path, const void *contents, size_t len)
{
    int rc;

    rc = nffsutil_write_file(path, contents, len);
    if (rc != 0) {
        return -1;
    }

    return 0;
}

int
tr_io_mkdir(const char *path)
{
    int rc;

    rc = nffs_mkdir(path);
    if (rc != 0 && rc != NFFS_EEXIST) {
        return -1;
    }

    return 0;
}

int
tr_io_rmdir(const char *path)
{
    int rc;

    rc = nffs_unlink(path);
    if (rc != 0 && rc != NFFS_ENOENT) {
        return -1;
    }

    return 0;
}

int
tr_io_read(const char *path, void *out_data, size_t len, size_t *out_len)
{
    uint32_t u32;
    int rc;

    rc = nffsutil_read_file(path, 0, len, out_data, &u32);
    if (rc != 0) {
        return -1;
    }

    *out_len = u32;

    return 0;
}

int
tr_io_delete(const char *path)
{
    int rc;

    rc = nffs_unlink(path);
    if (rc != 0 && rc != NFFS_ENOENT) {
        return -1;
    }

    return 0;
}
