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

#include "nffs_test_utils.h"

TEST_CASE(nffs_test_open)
{
    struct fs_file *file;
    struct fs_dir *dir;
    int rc;

    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Fail to open an invalid path (not rooted). */
    rc = fs_open("file", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FS_EINVAL);

    /*** Fail to open a directory (root directory). */
    rc = fs_open("/", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FS_EINVAL);

    /*** Fail to open a nonexistent file for reading. */
    rc = fs_open("/1234", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FS_ENOENT);

    /*** Fail to open a child of a nonexistent directory. */
    rc = fs_open("/dir/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == FS_ENOENT);
    rc = fs_opendir("/dir", &dir);
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_mkdir("/dir");
    TEST_ASSERT(rc == 0);

    /*** Fail to open a directory. */
    rc = fs_open("/dir", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FS_EINVAL);

    /*** Successfully open an existing file for reading. */
    nffs_test_util_create_file("/dir/file.txt", "1234567890", 10);
    rc = fs_open("/dir/file.txt", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    /*** Successfully open an nonexistent file for writing. */
    rc = fs_open("/dir/file2.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    /*** Ensure the file can be reopened. */
    rc = fs_open("/dir/file.txt", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);
}
