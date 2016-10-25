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

TEST_CASE(nffs_test_mkdir)
{
    struct fs_file *file;
    int rc;

    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/a/b/c/d");
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_mkdir("asdf");
    TEST_ASSERT(rc == FS_EINVAL);

    rc = fs_mkdir("/a");
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/a/b");
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/a/b/c");
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/a/b/c/d");
    TEST_ASSERT(rc == 0);

    rc = fs_open("/a/b/c/d/myfile.txt", FS_ACCESS_WRITE, &file);
    TEST_ASSERT(rc == 0);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "a",
                .is_dir = 1,
                .children = (struct nffs_test_file_desc[]) { {
                    .filename = "b",
                    .is_dir = 1,
                    .children = (struct nffs_test_file_desc[]) { {
                        .filename = "c",
                        .is_dir = 1,
                        .children = (struct nffs_test_file_desc[]) { {
                            .filename = "d",
                            .is_dir = 1,
                            .children = (struct nffs_test_file_desc[]) { {
                                .filename = "myfile.txt",
                                .contents = NULL,
                                .contents_len = 0,
                            }, {
                                .filename = NULL,
                            } },
                        }, {
                            .filename = NULL,
                        } },
                    }, {
                        .filename = NULL,
                    } },
                }, {
                    .filename = NULL,
                } },
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, nffs_current_area_descs);
}
