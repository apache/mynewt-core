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

TEST_CASE(nffs_test_overwrite_many)
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

    /*** Overwrite middle of first block. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 3);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 3);

    rc = fs_write(file, "12", 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 5);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abc12fghijklmnopqrstuvwx", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    /*** Overwrite end of first block, start of second. */
    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 3);
    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_seek(file, 6);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 6);

    rc = fs_write(file, "1234", 4);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 10);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents( "/myfile.txt",
                                   "abcdef1234klmnopqrstuvwx", 24);
    nffs_test_util_assert_block_count("/myfile.txt", 3);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "abcdef1234klmnopqrstuvwx",
                .contents_len = 24,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_current_area_descs);
}
