/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "testutil/testutil.h"
#include "os/os.h"
#include "os_test_priv.h"

#define MBUF_TEST_POOL_BUF_SIZE (256)
#define MBUF_TEST_POOL_BUF_COUNT (10) 

os_membuf_t mbuf_mempool[OS_MEMPOOL_SIZE(MBUF_TEST_POOL_BUF_SIZE, 
        MBUF_TEST_POOL_BUF_COUNT)];

TEST_CASE(os_mbuf_test_case_1)
{
    struct os_mbuf_pool omp; 
    struct os_mempool mp;
    struct os_mbuf *m; 
    int rc;

    rc = os_mempool_init(&mp, MBUF_TEST_POOL_BUF_COUNT, 
            MBUF_TEST_POOL_BUF_SIZE, &mbuf_mempool[0], "mbuf_pool");
    TEST_ASSERT_FATAL(rc == 0, "Error creating memory pool %d", rc);

    rc = os_mbuf_pool_init(&omp, &mp, 0, MBUF_TEST_POOL_BUF_SIZE, MBUF_TEST_POOL_BUF_COUNT); 
    TEST_ASSERT_FATAL(rc == 0, "Error creating mbuf pool %d", rc);

    m = os_mbuf_get(&omp, 0);
    TEST_ASSERT_FATAL(m != NULL, "Error allocating mbuf");

    rc = os_mbuf_free(&omp, m);
    TEST_ASSERT_FATAL(rc == 0, "Error free'ing mbuf %d", rc);
}


TEST_SUITE(os_mbuf_test_suite)
{
    os_mbuf_test_case_1();
}
