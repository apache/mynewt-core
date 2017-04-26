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

TEST_CASE(nffs_test_large_unlink)
{
    /* It should not be necessary to initialize this array, but the libgcc
     * version of strcmp triggers a "Conditional jump or move depends on
     * uninitialised value(s)" valgrind warning.
     */
    char filename[256] = { 0 };
    int rc;
    int i;
    int j;
    int k;

    static char file_contents[1024 * 4];

    /*** Setup. */
    nffs_config.nc_num_inodes = 1024;
    nffs_config.nc_num_blocks = 1024;

    rc = nffs_init();
    TEST_ASSERT(rc == 0);

    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < 5; i++) {
        snprintf(filename, sizeof filename, "/dir0_%d", i);
        rc = fs_mkdir(filename);
        TEST_ASSERT(rc == 0);

        for (j = 0; j < 5; j++) {
            snprintf(filename, sizeof filename, "/dir0_%d/dir1_%d", i, j);
            rc = fs_mkdir(filename);
            TEST_ASSERT(rc == 0);

            for (k = 0; k < 5; k++) {
                snprintf(filename, sizeof filename,
                         "/dir0_%d/dir1_%d/file2_%d", i, j, k);
                nffs_test_util_create_file(filename, file_contents,
                                          sizeof file_contents);
            }
        }

        for (j = 0; j < 15; j++) {
            snprintf(filename, sizeof filename, "/dir0_%d/file1_%d", i, j);
            nffs_test_util_create_file(filename, file_contents,
                                      sizeof file_contents);
        }
    }

    for (i = 0; i < 5; i++) {
        snprintf(filename, sizeof filename, "/dir0_%d", i);
        rc = fs_unlink(filename);
        TEST_ASSERT(rc == 0);
    }

    /* The entire file system should be empty. */
    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
    } };

    nffs_test_assert_system(expected_system, nffs_current_area_descs);
}
