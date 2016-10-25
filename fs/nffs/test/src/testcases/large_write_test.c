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

TEST_CASE(nffs_test_large_write)
{
    static char data[NFFS_BLOCK_MAX_DATA_SZ_MAX * 5];
    int rc;
    int i;

    static const struct nffs_area_desc area_descs_two[] = {
        { 0x00020000, 128 * 1024 },
        { 0x00040000, 128 * 1024 },
        { 0, 0 },
    };

    /*** Setup. */
    rc = nffs_format(area_descs_two);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < sizeof data; i++) {
        data[i] = i;
    }

    nffs_test_util_create_file("/myfile.txt", data, sizeof data);

    /* Ensure large write was split across the appropriate number of data
     * blocks.
     */
    TEST_ASSERT(nffs_test_util_block_count("/myfile.txt") ==
           sizeof data / NFFS_BLOCK_MAX_DATA_SZ_MAX);

    /* Garbage collect and then ensure the large file is still properly divided
     * according to max data block size.
     */
    nffs_gc(NULL);
    TEST_ASSERT(nffs_test_util_block_count("/myfile.txt") ==
           sizeof data / NFFS_BLOCK_MAX_DATA_SZ_MAX);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = data,
                .contents_len = sizeof data,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, area_descs_two);
}
