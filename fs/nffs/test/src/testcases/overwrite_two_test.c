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

TEST_CASE(nffs_test_overwrite_two)
{
    struct nffs_test_block_desc *blocks = (struct nffs_test_block_desc[]) { {
        .data = "abcdefgh",
        .data_len = 8,
    }, {
        .data = "ijklmnop",
        .data_len = 8,
    } };

    struct fs_file *file;
    int rc;


    /*** Setup. */
    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Overwrite two blocks (middle). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 7);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 7);

    rc = fs_write(file, "123", 3);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 10);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "abcdefg123klmnop", 16);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks (start). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "ABCDEFGHIJ", 10);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 10);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "ABCDEFGHIJklmnop", 16);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks (end). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234567890", 10);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 16);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "abcdef1234567890", 16);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks middle, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234567890!@#$", 14);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 20);
    TEST_ASSERT(fs_getpos(file) == 20);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "abcdef1234567890!@#$", 20);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    /*** Overwrite two blocks start, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 2);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 20);
    TEST_ASSERT(fs_getpos(file) == 20);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt", "1234567890!@#$%^&*()", 20);
    nffs_test_util_assert_block_count("/myfile.txt", 2);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "1234567890!@#$%^&*()",
                .contents_len = 20,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_current_area_descs);
}
