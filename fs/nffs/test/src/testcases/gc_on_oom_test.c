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

TEST_CASE(nffs_test_gc_on_oom)
{
    int rc;

    /*** Setup. */
    /* Ensure all areas are the same size. */
    static const struct nffs_area_desc area_descs_two[] = {
            { 0x00000000, 16 * 1024 },
            { 0x00004000, 16 * 1024 },
            { 0x00008000, 16 * 1024 },
            { 0, 0 },
    };

    rc = nffs_format(area_descs_two);
    TEST_ASSERT_FATAL(rc == 0);

    /* Leak block entries until only four are left. */
    /* XXX: This is ridiculous.  Need to fix nffs configuration so that the
     * caller passes a config object rather than writing to a global variable.
     */
    while (nffs_block_entry_pool.mp_num_free != 4) {
        nffs_block_entry_alloc();
    }

    /*** Write 4 data blocks. */
    struct nffs_test_block_desc blocks[4] = { {
        .data = "1",
        .data_len = 1,
    }, {
        .data = "2",
        .data_len = 1,
    }, {
        .data = "3",
        .data_len = 1,
    }, {
        .data = "4",
        .data_len = 1,
    } };

    nffs_test_util_create_file_blocks("/myfile.txt", blocks, 4);

    TEST_ASSERT_FATAL(nffs_block_entry_pool.mp_num_free == 0);

    /* Attempt another one-byte write.  This should trigger a garbage
     * collection cycle, resulting in the four blocks being collated.  The
     * fifth write consumes an additional block, resulting in 2 out of 4 blocks
     * in use.
     */
    nffs_test_util_append_file("/myfile.txt", "5", 1);

    TEST_ASSERT_FATAL(nffs_block_entry_pool.mp_num_free == 2);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "myfile.txt",
                .contents = "12345",
                .contents_len = 5,
            }, {
                .filename = NULL,
            } },
    } };

    nffs_test_assert_system(expected_system, area_descs_two);
}
