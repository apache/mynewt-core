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

TEST_CASE(nffs_test_cache_large_file)
{
    static char data[NFFS_BLOCK_MAX_DATA_SZ_MAX * 5];
    struct fs_file *file;
    uint8_t b;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/myfile.txt", data, sizeof data);
    nffs_cache_clear();

    /* Opening a file should not cause any blocks to get cached. */
    rc = fs_open("/myfile.txt", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt", 0, 0);

    /* Cache first block. */
    rc = fs_seek(file, nffs_block_max_data_sz * 0);
    TEST_ASSERT(rc == 0);
    rc = fs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 0,
                                     nffs_block_max_data_sz * 1);

    /* Cache second block. */
    rc = fs_seek(file, nffs_block_max_data_sz * 1);
    TEST_ASSERT(rc == 0);
    rc = fs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 0,
                                     nffs_block_max_data_sz * 2);


    /* Cache fourth block; prior cache should get erased. */
    rc = fs_seek(file, nffs_block_max_data_sz * 3);
    TEST_ASSERT(rc == 0);
    rc = fs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 3,
                                     nffs_block_max_data_sz * 4);

    /* Cache second and third blocks. */
    rc = fs_seek(file, nffs_block_max_data_sz * 1);
    TEST_ASSERT(rc == 0);
    rc = fs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 1,
                                     nffs_block_max_data_sz * 4);

    /* Cache fifth block. */
    rc = fs_seek(file, nffs_block_max_data_sz * 4);
    TEST_ASSERT(rc == 0);
    rc = fs_read(file, 1, &b, NULL);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_cache_range("/myfile.txt",
                                     nffs_block_max_data_sz * 1,
                                     nffs_block_max_data_sz * 5);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);
}
