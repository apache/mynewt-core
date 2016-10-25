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

TEST_CASE(nffs_test_large_system)
{
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);
    nffs_test_util_create_tree(nffs_test_system_01);

    nffs_test_assert_system(nffs_test_system_01, nffs_current_area_descs);

    rc = fs_unlink("/lvl1dir-0000");
    TEST_ASSERT(rc == 0);

    rc = fs_unlink("/lvl1dir-0004");
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/lvl1dir-0000");
    TEST_ASSERT(rc == 0);

    nffs_test_assert_system(nffs_test_system_01_rm_1014_mk10, nffs_current_area_descs);
}
