/**
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

    rc = os_mbuf_pool_init(&os_mbuf_pool, &os_mbuf_mempool,
            MBUF_TEST_POOL_BUF_SIZE, MBUF_TEST_POOL_BUF_COUNT);
    TEST_ASSERT_FATAL(rc == 0, "Error creating mbuf pool %d", rc);
}



TEST_CASE(os_mbuf_test_case_1)
{
    struct os_mbuf *m; 
    int rc;

    os_mbuf_test_setup();

    m = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m != NULL, "Error allocating mbuf");

    rc = os_mbuf_free(m);
    TEST_ASSERT_FATAL(rc == 0, "Error free'ing mbuf %d", rc);
}

TEST_CASE(os_mbuf_test_case_2) 
{
    struct os_mbuf *m;
    struct os_mbuf *m2;
    struct os_mbuf *dup;
    int rc;

    os_mbuf_test_setup();

    /* Test first allocating and duplicating a single mbuf */
    m = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m != NULL, "Error allocating mbuf");

    dup = os_mbuf_dup(m);
    TEST_ASSERT_FATAL(dup != NULL, "NULL mbuf returned from dup");
    TEST_ASSERT_FATAL(dup != m, "duplicate matches original.");

    rc = os_mbuf_free(m);
    TEST_ASSERT_FATAL(rc == 0, "Error free'ing mbuf m %d", rc);
    
    rc = os_mbuf_free(dup);
    TEST_ASSERT_FATAL(rc == 0, "Error free'ing mbuf dup %d", rc);

    m = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m != NULL, "Error allocating mbuf"); 

    m2 = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m2 != NULL, "Error allocating mbuf");

    SLIST_NEXT(m, om_next) = m2; 

    dup = os_mbuf_dup(m);
    TEST_ASSERT_FATAL(dup != NULL, "NULL mbuf returned from dup");
    TEST_ASSERT_FATAL(dup != m, "Duplicate matches original");
    TEST_ASSERT_FATAL(SLIST_NEXT(dup, om_next) != NULL, 
            "NULL chained element, duplicate should match original");

    rc = os_mbuf_free_chain(m);
    TEST_ASSERT_FATAL(rc == 0, "Cannot free mbuf chain %d", rc);

    rc = os_mbuf_free_chain(dup);
    TEST_ASSERT_FATAL(rc == 0, "Cannot free mbuf chain %d", rc);
}

TEST_CASE(os_mbuf_test_case_3) 
{
    struct os_mbuf *m; 
    int rc;
    uint8_t databuf[] = {0xa, 0xb, 0xc, 0xd};
    uint8_t cmpbuf[] = {0xff, 0xff, 0xff, 0xff};

    os_mbuf_test_setup();

    m = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m != NULL, "Error allocating mbuf");

    rc = os_mbuf_append(m, databuf, sizeof(databuf));
    TEST_ASSERT_FATAL(rc == 0, "Cannot add %d bytes to mbuf", 
            sizeof(databuf));
    TEST_ASSERT_FATAL(m->om_len == sizeof(databuf), 
            "Length doesn't match size appended %d vs %d", m->om_len, 
            sizeof(databuf));

    memcpy(cmpbuf, OS_MBUF_DATA(m, uint8_t *), m->om_len);
    TEST_ASSERT_FATAL(memcmp(cmpbuf, databuf, sizeof(databuf)) == 0, 
            "Databuf doesn't match cmpbuf");
}

static void
os_mbuf_test_misc_assert_contiguous(struct os_mbuf *om, void *data, int len)
{
    TEST_ASSERT_FATAL(om != NULL);

    if (OS_MBUF_IS_PKTHDR(om)) {
        TEST_ASSERT(OS_MBUF_PKTLEN(om) == len);
    }
    TEST_ASSERT(om->om_len == len);
    TEST_ASSERT(memcmp(om->om_data, data, len) == 0);
}

TEST_CASE(os_mbuf_test_pullup)
{
    struct os_mbuf *om;
    struct os_mbuf *om2;
    uint8_t data[256];
    int rc;
    int i;

    os_mbuf_test_setup();

    for (i = 0; i < sizeof data; i++) {
        data[i] = i;
    }

    /*** Free when too much data is requested. */
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL);

    om = os_mbuf_pullup(om, 1);
    TEST_ASSERT(om == NULL);

    /*** No effect when all data is already at the start. */
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL);

    rc = os_mbuf_append(om, data, 1);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_test_misc_assert_contiguous(om, data, 1);

    om = os_mbuf_pullup(om, 1);
    os_mbuf_test_misc_assert_contiguous(om, data, 1);

    /*** Spread data across four mbufs. */
    om2 = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om2 != NULL);
    rc = os_mbuf_append(om2, data + 1, 1);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_concat(om, om2);

    om2 = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om2 != NULL);
    rc = os_mbuf_append(om2, data + 2, 1);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_concat(om, om2);

    om2 = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om2 != NULL);
    rc = os_mbuf_append(om2, data + 3, 1);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_concat(om, om2);

    TEST_ASSERT_FATAL(OS_MBUF_PKTLEN(om) == 4);

    om = os_mbuf_pullup(om, 4);
    os_mbuf_test_misc_assert_contiguous(om, data, 4);

    os_mbuf_free_chain(om);

    /*** Require an allocation. */
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL);

    om->om_data += 100;
    rc = os_mbuf_append(om, data, 100);
    TEST_ASSERT_FATAL(rc == 0);

    om2 = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om2 != NULL);
    rc = os_mbuf_append(om2, data + 100, 100);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_concat(om, om2);

    om = os_mbuf_pullup(om, 200);
    os_mbuf_test_misc_assert_contiguous(om, data, 200);
}

TEST_SUITE(os_mbuf_test_suite)
{
    os_mbuf_test_case_1();
    os_mbuf_test_case_2();
    os_mbuf_test_case_3();
    os_mbuf_test_pullup();
}
