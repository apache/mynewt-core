/**
 * Copyright (c) 2015 Runtime Inc.
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

#include <string.h>

#define MBUF_TEST_POOL_BUF_SIZE (256)
#define MBUF_TEST_POOL_BUF_COUNT (10) 

os_membuf_t os_mbuf_membuf[OS_MEMPOOL_SIZE(MBUF_TEST_POOL_BUF_SIZE, 
        MBUF_TEST_POOL_BUF_COUNT)];

struct os_mbuf_pool os_mbuf_pool; 
struct os_mempool os_mbuf_mempool; 

static void 
os_mbuf_test_setup(void)
{
    int rc;

    rc = os_mempool_init(&os_mbuf_mempool, MBUF_TEST_POOL_BUF_COUNT, 
            MBUF_TEST_POOL_BUF_SIZE, &os_mbuf_membuf[0], "mbuf_pool");
    TEST_ASSERT_FATAL(rc == 0, "Error creating memory pool %d", rc);

    rc = os_mbuf_pool_init(&os_mbuf_pool, &os_mbuf_mempool, 0, 
            MBUF_TEST_POOL_BUF_SIZE, MBUF_TEST_POOL_BUF_COUNT);
    TEST_ASSERT_FATAL(rc == 0, "Error creating mbuf pool %d", rc);
}



TEST_CASE(os_mbuf_test_case_1)
{
    struct os_mbuf *m; 
    int rc;

    m = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m != NULL, "Error allocating mbuf");

    rc = os_mbuf_free(&os_mbuf_pool, m);
    TEST_ASSERT_FATAL(rc == 0, "Error free'ing mbuf %d", rc);
}

TEST_CASE(os_mbuf_test_case_2) 
{
    struct os_mbuf *m;
    struct os_mbuf *m2;
    struct os_mbuf *dup;
    int rc;

    /* Test first allocating and duplicating a single mbuf */
    m = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m != NULL, "Error allocating mbuf");

    dup = os_mbuf_dup(&os_mbuf_pool, m);
    TEST_ASSERT_FATAL(dup != NULL, "NULL mbuf returned from dup");
    TEST_ASSERT_FATAL(dup != m, "duplicate matches original.");

    rc = os_mbuf_free(&os_mbuf_pool, m);
    TEST_ASSERT_FATAL(rc == 0, "Error free'ing mbuf m %d", rc);
    
    rc = os_mbuf_free(&os_mbuf_pool, dup);
    TEST_ASSERT_FATAL(rc == 0, "Error free'ing mbuf dup %d", rc);

    m = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m != NULL, "Error allocating mbuf"); 

    m2 = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m2 != NULL, "Error allocating mbuf");

    SLIST_NEXT(m, om_next) = m2; 

    dup = os_mbuf_dup(&os_mbuf_pool, m);
    TEST_ASSERT_FATAL(dup != NULL, "NULL mbuf returned from dup");
    TEST_ASSERT_FATAL(dup != m, "Duplicate matches original");
    TEST_ASSERT_FATAL(SLIST_NEXT(dup, om_next) != NULL, 
            "NULL chained element, duplicate should match original");

    rc = os_mbuf_free_chain(&os_mbuf_pool, m);
    TEST_ASSERT_FATAL(rc == 0, "Cannot free mbuf chain %d", rc);

    rc = os_mbuf_free_chain(&os_mbuf_pool, dup);
    TEST_ASSERT_FATAL(rc == 0, "Cannot free mbuf chain %d", rc);
}

TEST_CASE(os_mbuf_test_case_3) 
{
    struct os_mbuf *m; 
    int rc;
    uint8_t databuf[] = {0xa, 0xb, 0xc, 0xd};
    uint8_t cmpbuf[] = {0xff, 0xff, 0xff, 0xff};

    m = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m != NULL, "Error allocating mbuf");

    rc = os_mbuf_append(&os_mbuf_pool, m, databuf, sizeof(databuf));
    TEST_ASSERT_FATAL(rc == 0, "Cannot add %d bytes to mbuf", 
            sizeof(databuf));
    TEST_ASSERT_FATAL(m->om_len == sizeof(databuf), 
            "Length doesn't match size appended %d vs %d", m->om_len, 
            sizeof(databuf));

    memcpy(cmpbuf, OS_MBUF_DATA(m, uint8_t *), m->om_len);
    TEST_ASSERT_FATAL(memcmp(cmpbuf, databuf, sizeof(databuf)) == 0, 
            "Databuf doesn't match cmpbuf");
}

TEST_SUITE(os_mbuf_test_case_4)
{
}

TEST_SUITE(os_mbuf_test_suite)
{
    os_mbuf_test_setup();

    os_mbuf_test_case_1();
    os_mbuf_test_case_2();
    os_mbuf_test_case_3();
}
