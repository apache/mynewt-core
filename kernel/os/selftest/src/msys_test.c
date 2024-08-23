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

#include <string.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "mem/mem.h"
#include "os_test_priv.h"
#include "msys_test.h"
#include "os_priv.h"

os_membuf_t msys_mbuf_membuf1[OS_MEMPOOL_SIZE(MSYS_TEST_POOL_BIG_BUF_SIZE, MSYS_TEST_POOL_BIG_BUF_COUNT)];
os_membuf_t msys_mbuf_membuf2[OS_MEMPOOL_SIZE(MSYS_TEST_POOL_SMALL_BUF_SIZE, MSYS_TEST_POOL_SMALL_BUF_COUNT)];
os_membuf_t msys_mbuf_membuf3[OS_MEMPOOL_SIZE(MSYS_TEST_POOL_MED_BUF_SIZE, MSYS_TEST_POOL_MED_BUF_COUNT)];

struct os_mempool msys_mempool1;
struct os_mempool msys_mempool2;
struct os_mempool msys_mempool3;

struct os_mbuf_pool msys_mbuf_pool1;
struct os_mbuf_pool msys_mbuf_pool2;
struct os_mbuf_pool msys_mbuf_pool3;

static void
os_msys_init_pool(void *data, struct os_mempool *mempool,
                  struct os_mbuf_pool *mbuf_pool,
                  int block_count, int block_size, char *name)
{
    int rc;

    rc = mem_init_mbuf_pool(data, mempool, mbuf_pool, block_count, block_size, name);
    TEST_ASSERT_FATAL(rc == 0, "mem_init_mbuf_pool failed");

    rc = os_msys_register(mbuf_pool);
    TEST_ASSERT_FATAL(rc == 0, "os_msys_register failed");
}

extern STAILQ_HEAD(, os_mempool) g_os_mempool_list;

void
os_msys_test_setup(int pool_count, struct msys_context *context)
{
    /* Preserve default state of pools and msys in case other cases use it */
    context->mbuf_list = *get_msys_pool_list();
    context->mempool_list = *(struct os_mempool_list *)&g_os_mempool_list;

    os_mempool_module_init();
    os_msys_reset();

    /* Add 1 to 3 mem pools to msys */
    if (pool_count > 0) {
        os_msys_init_pool(msys_mbuf_membuf1, &msys_mempool1, &msys_mbuf_pool1, MSYS_TEST_POOL_BIG_BUF_COUNT,
                          MSYS_TEST_POOL_BIG_BUF_SIZE, "msys_big");
    }
    if (pool_count > 1) {
        os_msys_init_pool(msys_mbuf_membuf2, &msys_mempool2, &msys_mbuf_pool2, MSYS_TEST_POOL_SMALL_BUF_COUNT,
                          MSYS_TEST_POOL_SMALL_BUF_SIZE, "msys_small");
    }
    if (pool_count > 2) {
        os_msys_init_pool(msys_mbuf_membuf3, &msys_mempool3, &msys_mbuf_pool3, MSYS_TEST_POOL_MED_BUF_COUNT,
                          MSYS_TEST_POOL_MED_BUF_SIZE, "msys_med");
    }
}

void
os_msys_test_teardown(struct msys_context *context)
{
    /* Restore default mpools and msys */
    *get_msys_pool_list() = context->mbuf_list;
    memcpy(&g_os_mempool_list, &context->mempool_list, sizeof(context->mempool_list));
}

TEST_CASE_DECL(os_msys_test_limit1)
TEST_CASE_DECL(os_msys_test_limit2)
TEST_CASE_DECL(os_msys_test_limit3)
TEST_CASE_DECL(os_msys_test_alloc1)

TEST_SUITE(os_msys_test_suite)
{
    os_msys_test_limit1();
    os_msys_test_limit2();
    os_msys_test_limit3();
    os_msys_test_alloc1();
}
