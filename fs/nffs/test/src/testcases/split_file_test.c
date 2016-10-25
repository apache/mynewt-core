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

TEST_CASE(nffs_test_split_file)
{
    static char data[24 * 1024];
    int rc;
    int i;

    /*** Setup. */
    static const struct nffs_area_desc area_descs_two[] = {
            { 0x00000000, 16 * 1024 },
            { 0x00004000, 16 * 1024 },
            { 0x00008000, 16 * 1024 },
            { 0, 0 },
    };

    rc = nffs_format(area_descs_two);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < sizeof data; i++) {
        data[i] = i;
    }

    for (i = 0; i < 256; i++) {
        nffs_test_util_create_file("/myfile.txt", data, sizeof data);
        rc = fs_unlink("/myfile.txt");
        TEST_ASSERT(rc == 0);
    }

    nffs_test_util_create_file("/myfile.txt", data, sizeof data);

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
