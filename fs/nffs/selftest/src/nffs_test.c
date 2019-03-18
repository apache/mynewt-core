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

/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "os/mynewt.h"
#include "hal/hal_flash.h"
#include "testutil/testutil.h"
#include "fs/fs.h"
#include "nffs/nffs.h"
#include "nffs/nffs_test.h"
#include "nffs_test_priv.h"
#include "nffs_priv.h"
#include "nffs_test.h"

struct nffs_area_desc nffs_selftest_area_descs[] = {
        { 0x00000000, 16 * 1024 },
        { 0x00004000, 16 * 1024 },
        { 0x00008000, 16 * 1024 },
        { 0x0000c000, 16 * 1024 },
        { 0x00010000, 64 * 1024 },
        { 0x00020000, 128 * 1024 },
        { 0x00040000, 128 * 1024 },
        { 0x00060000, 128 * 1024 },
        { 0x00080000, 128 * 1024 },
        { 0x000a0000, 128 * 1024 },
        { 0x000c0000, 128 * 1024 },
        { 0x000e0000, 128 * 1024 },
        { 0, 0 },
};

struct nffs_area_desc *save_area_descs;

static void
nffs_testcase_pre(void* arg)
{
    save_area_descs = nffs_current_area_descs;
    nffs_current_area_descs = nffs_selftest_area_descs;
}

TEST_CASE_DECL(nffs_test_unlink)
TEST_CASE_DECL(nffs_test_mkdir)
TEST_CASE_DECL(nffs_test_rename)
TEST_CASE_DECL(nffs_test_truncate)
TEST_CASE_DECL(nffs_test_append)
TEST_CASE_DECL(nffs_test_read)
TEST_CASE_DECL(nffs_test_open)
TEST_CASE_DECL(nffs_test_overwrite_one)
TEST_CASE_DECL(nffs_test_overwrite_two)
TEST_CASE_DECL(nffs_test_overwrite_three)
TEST_CASE_DECL(nffs_test_overwrite_many)
TEST_CASE_DECL(nffs_test_long_filename)
TEST_CASE_DECL(nffs_test_large_write)
TEST_CASE_DECL(nffs_test_many_children)
TEST_CASE_DECL(nffs_test_gc)
TEST_CASE_DECL(nffs_test_wear_level)
TEST_CASE_DECL(nffs_test_corrupt_scratch)
TEST_CASE_DECL(nffs_test_incomplete_block)
TEST_CASE_DECL(nffs_test_corrupt_block)
TEST_CASE_DECL(nffs_test_large_unlink)
TEST_CASE_DECL(nffs_test_large_system)
TEST_CASE_DECL(nffs_test_lost_found)
TEST_CASE_DECL(nffs_test_readdir)
TEST_CASE_DECL(nffs_test_split_file)
TEST_CASE_DECL(nffs_test_gc_on_oom)
TEST_CASE_DECL(nffs_test_cache_large_file)

static void
nffs_test_basic_cases(void)
{
    nffs_test_unlink();
    nffs_test_mkdir();
    nffs_test_rename();
    nffs_test_truncate();
    nffs_test_append();
    nffs_test_read();
    nffs_test_open();
    nffs_test_overwrite_one();
    nffs_test_overwrite_two();
    nffs_test_overwrite_three();
    nffs_test_overwrite_many();
    nffs_test_long_filename();
    nffs_test_large_write();
    nffs_test_many_children();
    nffs_test_gc();
    nffs_test_wear_level();
    nffs_test_corrupt_scratch();
    nffs_test_incomplete_block();
    nffs_test_corrupt_block();
    nffs_test_large_unlink();
    nffs_test_large_system();
    nffs_test_lost_found();
    nffs_test_readdir();
    nffs_test_split_file();
    nffs_test_gc_on_oom();
}

TEST_SUITE(nffs_test_suite_1_1)
{
    nffs_config.nc_num_cache_inodes = 1;
    nffs_config.nc_num_cache_blocks = 1;
    tu_suite_set_pre_test_cb(nffs_testcase_pre, NULL);

    nffs_test_basic_cases();
}

TEST_SUITE(nffs_test_suite_4_32)
{
    nffs_config.nc_num_cache_inodes = 4;
    nffs_config.nc_num_cache_blocks = 32;
    tu_suite_set_pre_test_cb(nffs_testcase_pre, NULL);

    nffs_test_basic_cases();
}

TEST_SUITE(nffs_test_suite_32_1024)
{
    nffs_config.nc_num_cache_inodes = 32;
    nffs_config.nc_num_cache_blocks = 1024;
    tu_suite_set_pre_test_cb(nffs_testcase_pre, NULL);

    nffs_test_basic_cases();
}

TEST_SUITE(nffs_suite_cache)
{
    nffs_config.nc_num_cache_inodes = 4;
    nffs_config.nc_num_cache_blocks = 64;
    tu_suite_set_pre_test_cb(nffs_testcase_pre, NULL);

    nffs_test_cache_large_file();
}

int
main(void)
{
    nffs_config.nc_num_inodes = 1024 * 8;
    nffs_config.nc_num_blocks = 1024 * 20;

    nffs_test_suite_1_1();
    nffs_test_suite_4_32();
    nffs_test_suite_32_1024();

    nffs_suite_cache();

    return tu_any_failed;
}
