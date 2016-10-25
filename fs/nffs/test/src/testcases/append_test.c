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

void process_inode_entry(struct nffs_inode_entry *inode_entry, int indent);

TEST_CASE(nffs_test_append)
{
    struct fs_file *file;
    uint32_t len;
    char c;
    int rc;
    int i;

    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE | FS_ACCESS_APPEND, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 0);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_write(file, "abcdefgh", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 8);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt", "abcdefgh", 8);

    rc = fs_open("/myfile.txt", FS_ACCESS_WRITE | FS_ACCESS_APPEND, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 8);

    /* File position should always be at the end of a file after an append.
     * Seek to the middle prior to writing to test this.
     */
    rc = fs_seek(file, 2);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 8);
    TEST_ASSERT(fs_getpos(file) == 2);

    rc = fs_write(file, "ijklmnop", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 16);
    TEST_ASSERT(fs_getpos(file) == 16);
    rc = fs_write(file, "qrstuvwx", 8);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 24);
    TEST_ASSERT(fs_getpos(file) == 24);
    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/myfile.txt",
                                  "abcdefghijklmnopqrstuvwx", 24);

    rc = fs_mkdir("/mydir");
    TEST_ASSERT_FATAL(rc == 0);
    rc = fs_open("/mydir/gaga.txt", FS_ACCESS_WRITE | FS_ACCESS_APPEND, &file);
    TEST_ASSERT_FATAL(rc == 0);

    /*** Repeated appends to a large file. */
    for (i = 0; i < 1000; i++) {
        rc = fs_filelen(file, &len);
        TEST_ASSERT_FATAL(rc == 0);
        TEST_ASSERT(len == i);

        c = '0' + i % 10;
        rc = fs_write(file, &c, 1);
        TEST_ASSERT_FATAL(rc == 0);
    }

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/mydir/gaga.txt",
        "01234567890123456789012345678901234567890123456789" /* 1 */
        "01234567890123456789012345678901234567890123456789" /* 2 */
        "01234567890123456789012345678901234567890123456789" /* 3 */
        "01234567890123456789012345678901234567890123456789" /* 4 */
        "01234567890123456789012345678901234567890123456789" /* 5 */
        "01234567890123456789012345678901234567890123456789" /* 6 */
        "01234567890123456789012345678901234567890123456789" /* 7 */
        "01234567890123456789012345678901234567890123456789" /* 8 */
        "01234567890123456789012345678901234567890123456789" /* 9 */
        "01234567890123456789012345678901234567890123456789" /* 10 */
        "01234567890123456789012345678901234567890123456789" /* 11 */
        "01234567890123456789012345678901234567890123456789" /* 12 */
        "01234567890123456789012345678901234567890123456789" /* 13 */
        "01234567890123456789012345678901234567890123456789" /* 14 */
        "01234567890123456789012345678901234567890123456789" /* 15 */
        "01234567890123456789012345678901234567890123456789" /* 16 */
        "01234567890123456789012345678901234567890123456789" /* 17 */
        "01234567890123456789012345678901234567890123456789" /* 18 */
        "01234567890123456789012345678901234567890123456789" /* 19 */
        "01234567890123456789012345678901234567890123456789" /* 20 */
        ,
        1000);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "abcdefghijklmnopqrstuvwx",
                .contents_len = 24,
            }, {
                .filename = "mydir",
                .is_dir = 1,
                .children = (struct nffs_test_file_desc[]) { {
                    .filename = "gaga.txt",
                    .contents =
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789"
    ,
                    .contents_len = 1000,
                }, {
                    .filename = NULL,
                } },
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_current_area_descs);
}
