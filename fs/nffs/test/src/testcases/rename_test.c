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

TEST_CASE(nffs_test_rename)
{
    struct fs_file *file;
    const char contents[] = "contents";
    int rc;


    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_rename("/nonexistent.txt", "/newname.txt");
    TEST_ASSERT(rc == FS_ENOENT);

    /*** Rename file. */
    nffs_test_util_create_file("/myfile.txt", contents, sizeof contents);

    rc = fs_rename("/myfile.txt", "badname");
    TEST_ASSERT(rc == FS_EINVAL);

    rc = fs_rename("/myfile.txt", "/myfile2.txt");
    TEST_ASSERT(rc == 0);

    rc = fs_open("/myfile.txt", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == FS_ENOENT);

    nffs_test_util_assert_contents("/myfile2.txt", contents, sizeof contents);

    rc = fs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/mydir/leafdir");
    TEST_ASSERT(rc == 0);

    rc = fs_rename("/myfile2.txt", "/mydir/myfile2.txt");
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/mydir/myfile2.txt", contents,
                                  sizeof contents);

    /*** Rename directory. */
    rc = fs_rename("/mydir", "badname");
    TEST_ASSERT(rc == FS_EINVAL);

    /* Don't allow a directory to be moved into a descendent directory. */
    rc = fs_rename("/mydir", "/mydir/leafdir/a");
    TEST_ASSERT(rc == FS_EINVAL);

    rc = fs_rename("/mydir", "/mydir2");
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_contents("/mydir2/myfile2.txt", contents,
                                  sizeof contents);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "mydir2",
                .is_dir = 1,
                .children = (struct nffs_test_file_desc[]) { {
                    .filename = "leafdir",
                    .is_dir = 1,
                }, {
                    .filename = "myfile2.txt",
                    .contents = "contents",
                    .contents_len = 9,
                }, {
                    .filename = NULL,
                } },
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_current_area_descs);
}
