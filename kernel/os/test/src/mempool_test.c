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
#include <stdio.h>
#include <string.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

#if OS_CFG_ALIGNMENT == OS_CFG_ALIGN_4
int alignment = 4;
#else
int alignment = 8;
#endif

#if MYNEWT_VAL(SELFTEST)
/* Test memory pool structure */
struct os_mempool g_TstMempool;

/* Array of block pointers. */
void *block_array[MEMPOOL_TEST_MAX_BLOCKS];

/* Test memory pool buffer */
os_membuf_t *TstMembuf;
#endif

uint32_t TstMembufSz;

int verbose = 0;

int
mempool_test_get_pool_size(int num_blocks, int block_size)
{
    int mem_pool_size;

#if OS_CFG_ALIGNMENT == OS_CFG_ALIGN_4
    mem_pool_size = (num_blocks * ((block_size + 3)/4) * sizeof(os_membuf_t));
#else
    mem_pool_size = (num_blocks * ((block_size + 7)/8) * sizeof(os_membuf_t));
#endif

    return mem_pool_size;
}

void
os_mempool_ts_pretest(void* arg)
{
    os_init(NULL);
    sysinit();
}

void
os_mempool_ts_posttest(void* arg)
{
    return;
}

void
os_mempool_test_init(void *arg)
{
    TstMembufSz = (sizeof(os_membuf_t) *
                 OS_MEMPOOL_SIZE(NUM_MEM_BLOCKS, MEM_BLOCK_SIZE));
    TstMembuf = malloc(TstMembufSz);

    tu_suite_set_pre_test_cb(os_mempool_ts_pretest, NULL);
    tu_suite_set_post_test_cb(os_mempool_ts_posttest, NULL);
}

TEST_CASE_DECL(os_mempool_test_case)
TEST_CASE_DECL(os_mempool_test_ext_basic)
TEST_CASE_DECL(os_mempool_test_ext_nested)

TEST_SUITE(os_mempool_test_suite)
{
    os_mempool_test_case();
    os_mempool_test_ext_basic();
    os_mempool_test_ext_nested();
}
