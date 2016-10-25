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

TEST_CASE(nffs_test_wear_level)
{
    int rc;
    int i;
    int j;

    static const struct nffs_area_desc area_descs_uniform[] = {
        { 0x00000000, 2 * 1024 },
        { 0x00020000, 2 * 1024 },
        { 0x00040000, 2 * 1024 },
        { 0x00060000, 2 * 1024 },
        { 0x00080000, 2 * 1024 },
        { 0, 0 },
    };

    /*** Setup. */
    rc = nffs_format(area_descs_uniform);
    TEST_ASSERT(rc == 0);

    /* Ensure areas rotate properly. */
    for (i = 0; i < 255; i++) {
        for (j = 0; j < nffs_num_areas; j++) {
            nffs_test_assert_area_seqs(i, nffs_num_areas - j, i + 1, j);
            nffs_gc(NULL);
        }
    }

    /* Ensure proper rollover of sequence numbers. */
    for (j = 0; j < nffs_num_areas; j++) {
        nffs_test_assert_area_seqs(255, nffs_num_areas - j, 0, j);
        nffs_gc(NULL);
    }
    for (j = 0; j < nffs_num_areas; j++) {
        nffs_test_assert_area_seqs(0, nffs_num_areas - j, 1, j);
        nffs_gc(NULL);
    }
}
