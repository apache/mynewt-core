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

TEST_CASE(nffs_test_overwrite_one)
{
    struct fs_file *file;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_append_file("/myfile.txt", "abcdefgh", 8);

    /*** Overwrite within one block (middle). */
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 3);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 3);

    rc = fs_write(file, "12", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 5);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "abc12fgh", 8);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite within one block (start). */
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "xy", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 2);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "xyc12fgh", 8);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite within one block (end). */
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "<>", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 8);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "xyc12f<>", 8);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite one block middle, extend. */
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 4);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 4);

    rc = fs_write(file, "abcdefgh", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 12);
    TEST_ASSERT(fs_getpos(file) == 12);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "xyc1abcdefgh", 12);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    /*** Overwrite one block start, extend. */
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 12);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "abcdefghijklmnop", 16);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 16);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "abcdefghijklmnop", 16);
    nffs_test_util_assert_block_count("/myfile.txt", 1);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "abcdefghijklmnop",
                .contents_len = 16,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_current_area_descs);
}
