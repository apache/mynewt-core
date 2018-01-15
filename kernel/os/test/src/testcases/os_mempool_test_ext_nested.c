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

static int num_frees;

static os_error_t
put_cb(struct os_mempool_ext *mpe, void *block, void *arg)
{
    int *elem;
    int rc;

    num_frees++;

    /* Only do work on the first free to avoid infinite recursion. */
    if (num_frees == 1) {
        /* Try to allocate and free within callback. */
        elem = os_memblock_get(&mpe->mpe_mp);
        TEST_ASSERT(elem != NULL);

        rc = os_memblock_put(&mpe->mpe_mp, elem);
        TEST_ASSERT(rc == 0);
    }

    /* Actually free block. */
    return os_memblock_put_from_cb(&mpe->mpe_mp, block);
}

TEST_CASE(os_mempool_test_ext_nested)
{
    uint8_t buf[OS_MEMPOOL_BYTES(10, 32)];
    struct os_mempool_ext pool;
    int *elem;
    int rc;

    rc = os_mempool_ext_init(&pool, 10, 32, buf, "test_ext_nested");
    TEST_ASSERT_FATAL(rc == 0);

    pool.mpe_put_cb = put_cb;

    elem = os_memblock_get(&pool.mpe_mp);
    TEST_ASSERT_FATAL(elem != NULL, "Error allocating block");

    rc = os_memblock_put(&pool.mpe_mp, elem);
    TEST_ASSERT_FATAL(rc == 0, "Error freeing block %d", rc);

    /* Verify callback was called within callback. */
    TEST_ASSERT(num_frees == 2);
}
