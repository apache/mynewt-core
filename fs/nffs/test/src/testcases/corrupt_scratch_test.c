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

TEST_CASE(nffs_test_corrupt_scratch)
{
    int non_scratch_id;
    int scratch_id;
    int rc;

    static const struct nffs_area_desc area_descs_two[] = {
        { 0x00020000, 128 * 1024 },
        { 0x00040000, 128 * 1024 },
        { 0, 0 },
    };
    nffs_current_area_descs = (struct nffs_area_desc*)area_descs_two;

    /*** Setup. */
    rc = nffs_format(area_descs_two);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/myfile.txt", "contents", 8);

    /* Copy the current contents of the non-scratch area to the scratch area.
     * This will make the scratch area look like it only partially
     * participated in a garbage collection cycle.
     */
    scratch_id = nffs_scratch_area_idx;
    non_scratch_id = scratch_id ^ 1;
    nffs_test_copy_area(area_descs_two + non_scratch_id,
                       area_descs_two + nffs_scratch_area_idx);

    /* Add some more data to the non-scratch area. */
    rc = fs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);

    /* Ensure the file system is successfully detected and valid, despite
     * corruption.
     */

    rc = nffs_misc_reset();
    TEST_ASSERT(rc == 0);

    rc = nffs_detect(area_descs_two);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(nffs_scratch_area_idx == scratch_id);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "mydir",
                .is_dir = 1,
            }, {
                .filename = "myfile.txt",
                .contents = "contents",
                .contents_len = 8,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, area_descs_two);
}
