/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ffs/ffs.h"
#include "ffs/ffsutil.h"

int
ffsutil_read_file(const char *path, uint32_t offset, uint32_t len, void *dst,
                  uint32_t *out_len)
{
    struct ffs_file *file;
    int rc;

    rc = ffs_open(path, FFS_ACCESS_READ, &file);
    if (rc != 0) {
        goto done;
    }

    rc = ffs_read(file, len, dst, out_len);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    ffs_close(file);
    return rc;
}

int
ffsutil_write_file(const char *path, const void *data, uint32_t len)
{
    struct ffs_file *file;
    int rc;

    rc = ffs_open(path, FFS_ACCESS_WRITE | FFS_ACCESS_TRUNCATE, &file);
    if (rc != 0) {
        goto done;
    }

    rc = ffs_write(file, data, len);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    ffs_close(file);
    return rc;
}
