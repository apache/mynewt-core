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

#include "os_test_priv.h"

static struct os_mempool_ext *freed_pool;
static void *freed_block;

static os_error_t
put_cb(struct os_mempool_ext *mpe, void *block, void *arg)
{
    /* Verify block was not freed before this callback gets called. */
    TEST_ASSERT(mpe->mpe_mp.mp_num_free == mpe->mpe_mp.mp_num_blocks - 1);

    /* Remember pool that block got freed to. */
    freed_pool = mpe;
    freed_block = block;

    /* Actually free block. */
    return os_memblock_put_from_cb(&mpe->mpe_mp, block);
}

TEST_CASE(os_mempool_test_ext_basic)
{
    uint8_t buf[OS_MEMPOOL_BYTES(10, 32)];
    struct os_mempool_ext pool;
    int *ip;
    int rc;

    rc = os_mempool_ext_init(&pool, 10, 32, buf, "test_ext_basic");
    TEST_ASSERT_FATAL(rc == 0);

    /*** No callback. */
    ip = os_memblock_get(&pool.mpe_mp);
    TEST_ASSERT_FATAL(ip != NULL, "Error allocating block");

    rc = os_memblock_put(&pool.mpe_mp, ip);
    TEST_ASSERT_FATAL(rc == 0, "Error freeing block %d", rc);

    TEST_ASSERT(freed_pool == NULL);

    /*** With callback. */
    pool.mpe_put_cb = put_cb;

    ip = os_memblock_get(&pool.mpe_mp);
    TEST_ASSERT_FATAL(ip != NULL, "Error allocating block");

    rc = os_memblock_put(&pool.mpe_mp, ip);
    TEST_ASSERT_FATAL(rc == 0, "Error freeing block %d", rc);

    /*** No callback; ensure old callback doesn't get called. */
    freed_pool = NULL;
    freed_block = NULL;
    pool.mpe_put_cb = NULL;

    ip = os_memblock_get(&pool.mpe_mp);
    TEST_ASSERT_FATAL(ip != NULL, "Error allocating block");

    rc = os_memblock_put(&pool.mpe_mp, ip);
    TEST_ASSERT_FATAL(rc == 0, "Error freeing block %d", rc);

    TEST_ASSERT(freed_pool == NULL);
    TEST_ASSERT(freed_block == NULL);
}
