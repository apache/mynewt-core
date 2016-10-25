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

TEST_CASE(nffs_test_overwrite_three)
{
    struct nffs_test_block_desc *blocks = (struct nffs_test_block_desc[]) { {
        .data = "abcdefgh",
        .data_len = 8,
    }, {
        .data = "ijklmnop",
        .data_len = 8,
    }, {
        .data = "qrstuvwx",
        .data_len = 8,
    } };

    struct fs_file *file;
    int rc;


    /*** Setup. */
    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    /*** Overwrite three blocks (middle). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234567890!@", 12);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 18);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@stuvwx", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks (start). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 20);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "1234567890!@#$%^&*()uvwx", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks (end). */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234567890!@#$%^&*", 18);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 24);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@#$%^&*", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks middle, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234567890!@#$%^&*()", 20);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 26);
    TEST_ASSERT(fs_getpos(file) == 26);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234567890!@#$%^&*()", 26);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite three blocks start, extend. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "1234567890!@#$%^&*()abcdefghij", 30);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 30);
    TEST_ASSERT(fs_getpos(file) == 30);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "1234567890!@#$%^&*()abcdefghij", 30);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "1234567890!@#$%^&*()abcdefghij",
                .contents_len = 30,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_current_area_descs);
}
