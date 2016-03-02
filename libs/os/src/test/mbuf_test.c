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

/* 
 * NOTE: currently, the buffer size cannot be changed as some tests are
 * hard-coded for this size.
 */
#define MBUF_TEST_POOL_BUF_SIZE     (256)
#define MBUF_TEST_POOL_BUF_COUNT    (10)

#define MBUF_TEST_DATA_LEN          (1024)

static os_membuf_t os_mbuf_membuf[OS_MEMPOOL_SIZE(MBUF_TEST_POOL_BUF_SIZE,
        MBUF_TEST_POOL_BUF_COUNT)];

static struct os_mbuf_pool os_mbuf_pool;
static struct os_mempool os_mbuf_mempool;
static uint8_t os_mbuf_test_data[MBUF_TEST_DATA_LEN];

static void
os_mbuf_test_setup(void)
{
    int rc;
    int i;

    rc = os_mempool_init(&os_mbuf_mempool, MBUF_TEST_POOL_BUF_COUNT,
            MBUF_TEST_POOL_BUF_SIZE, &os_mbuf_membuf[0], "mbuf_pool");
    TEST_ASSERT_FATAL(rc == 0, "Error creating memory pool %d", rc);

    rc = os_mbuf_pool_init(&os_mbuf_pool, &os_mbuf_mempool,
            MBUF_TEST_POOL_BUF_SIZE, MBUF_TEST_POOL_BUF_COUNT);
    TEST_ASSERT_FATAL(rc == 0, "Error creating mbuf pool %d", rc);

    for (i = 0; i < sizeof os_mbuf_test_data; i++) {
        os_mbuf_test_data[i] = i;
    }
}

static void
os_mbuf_test_misc_assert_sane(struct os_mbuf *om, void *data,
                              int buflen, int pktlen, int pkthdr_len)
{
    uint8_t *data_min;
    uint8_t *data_max;
    int totlen;
    int i;

    TEST_ASSERT_FATAL(om != NULL);

    if (OS_MBUF_IS_PKTHDR(om)) {
        TEST_ASSERT(OS_MBUF_PKTLEN(om) == pktlen);
    }

    totlen = 0;
    for (i = 0; om != NULL; i++) {
        if (i == 0) {
            TEST_ASSERT(om->om_len == buflen);
            TEST_ASSERT(om->om_pkthdr_len == pkthdr_len);
        }

        data_min = om->om_databuf + om->om_pkthdr_len;
        data_max = om->om_databuf + om->om_omp->omp_databuf_len - om->om_len;
        TEST_ASSERT(om->om_data >= data_min && om->om_data <= data_max);

        if (data != NULL) {
            TEST_ASSERT(memcmp(om->om_data, data + totlen, om->om_len) == 0);
        }

        totlen += om->om_len;
        om = SLIST_NEXT(om, om_next);
    }

    TEST_ASSERT(totlen == pktlen);
}


TEST_CASE(os_mbuf_test_alloc)
{
    struct os_mbuf *m;
    int rc;

    os_mbuf_test_setup();

    m = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m != NULL, "Error allocating mbuf");

    rc = os_mbuf_free(m);
    TEST_ASSERT_FATAL(rc == 0, "Error free'ing mbuf %d", rc);
}

TEST_CASE(os_mbuf_test_get_pkthdr)
{
    struct os_mbuf *m;
 
    os_mbuf_test_setup();

#if (MBUF_TEST_POOL_BUF_SIZE <= 256)
    m = os_mbuf_get_pkthdr(&os_mbuf_pool, MBUF_TEST_POOL_BUF_SIZE - 1);
    TEST_ASSERT_FATAL(m == NULL, "Error: should not have returned mbuf");
#endif

    m = os_mbuf_get(&os_mbuf_pool, MBUF_TEST_POOL_BUF_SIZE);
    TEST_ASSERT_FATAL(m == NULL, "Error: should not have returned mbuf");
}


TEST_CASE(os_mbuf_test_dup)
{
    struct os_mbuf *om;
    struct os_mbuf *om2;
    struct os_mbuf *dup;
    int rc;

    os_mbuf_test_setup();

    /* Test first allocating and duplicating a single mbuf */
    om = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL, "Error allocating mbuf");

    rc = os_mbuf_append(om, os_mbuf_test_data, 200);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data, 200, 200, 0);

    dup = os_mbuf_dup(om);
    TEST_ASSERT_FATAL(dup != NULL, "NULL mbuf returned from dup");
    TEST_ASSERT_FATAL(dup != om, "duplicate matches original.");
    os_mbuf_test_misc_assert_sane(dup, os_mbuf_test_data, 200, 200, 0);

    rc = os_mbuf_free(om);
    TEST_ASSERT_FATAL(rc == 0, "Error free'ing mbuf om %d", rc);

    rc = os_mbuf_free(dup);
    TEST_ASSERT_FATAL(rc == 0, "Error free'ing mbuf dup %d", rc);

    om = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL, "Error allocating mbuf");
    rc = os_mbuf_append(om, os_mbuf_test_data, 200);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data, 200, 200, 0);

    om2 = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om2 != NULL, "Error allocating mbuf");
    rc = os_mbuf_append(om2, os_mbuf_test_data + 200, 200);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_test_misc_assert_sane(om2, os_mbuf_test_data + 200, 200, 200, 0);

    os_mbuf_concat(om, om2);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data, 200, 400, 0);

    dup = os_mbuf_dup(om);
    TEST_ASSERT_FATAL(dup != NULL, "NULL mbuf returned from dup");
    TEST_ASSERT_FATAL(dup != om, "Duplicate matches original");
    TEST_ASSERT_FATAL(SLIST_NEXT(dup, om_next) != NULL,
            "NULL chained element, duplicate should match original");

    os_mbuf_test_misc_assert_sane(dup, os_mbuf_test_data, 200, 400, 0);

    rc = os_mbuf_free_chain(om);
    TEST_ASSERT_FATAL(rc == 0, "Cannot free mbuf chain %d", rc);

    rc = os_mbuf_free_chain(dup);
    TEST_ASSERT_FATAL(rc == 0, "Cannot free mbuf chain %d", rc);
}

TEST_CASE(os_mbuf_test_append)
{
    struct os_mbuf *om;
    int rc;
    uint8_t databuf[] = {0xa, 0xb, 0xc, 0xd};
    uint8_t cmpbuf[] = {0xff, 0xff, 0xff, 0xff};

    os_mbuf_test_setup();

    om = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL, "Error allocating mbuf");
    os_mbuf_test_misc_assert_sane(om, NULL, 0, 0, 0);

    rc = os_mbuf_append(om, databuf, sizeof(databuf));
    TEST_ASSERT_FATAL(rc == 0, "Cannot add %d bytes to mbuf",
            sizeof(databuf));
    os_mbuf_test_misc_assert_sane(om, databuf, sizeof databuf, sizeof databuf,
                                  0);

    memcpy(cmpbuf, OS_MBUF_DATA(om, uint8_t *), om->om_len);
    TEST_ASSERT_FATAL(memcmp(cmpbuf, databuf, sizeof(databuf)) == 0,
            "Databuf doesn't match cmpbuf");
}

TEST_CASE(os_mbuf_test_extend)
{
    struct os_mbuf *om;
    void *v;

    os_mbuf_test_setup();

    /*** Series of successful extensions. */
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om != NULL);

    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == 222);
    TEST_ASSERT(SLIST_NEXT(om, om_next) == NULL);
    os_mbuf_test_misc_assert_sane(om, NULL, 0, 0, 18);

    v = os_mbuf_extend(om, 20);
    TEST_ASSERT(v != NULL);
    TEST_ASSERT(v == om->om_data);
    TEST_ASSERT(om->om_len == 20);

    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == 202);
    TEST_ASSERT(SLIST_NEXT(om, om_next) == NULL);
    os_mbuf_test_misc_assert_sane(om, NULL, 20, 20, 18);

    v = os_mbuf_extend(om, 100);
    TEST_ASSERT(v != NULL);
    TEST_ASSERT(v == om->om_data + 20);
    TEST_ASSERT(om->om_len == 120);

    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == 102);
    TEST_ASSERT(SLIST_NEXT(om, om_next) == NULL);
    os_mbuf_test_misc_assert_sane(om, NULL, 120, 120, 18);

    v = os_mbuf_extend(om, 101);
    TEST_ASSERT(v != NULL);
    TEST_ASSERT(v == om->om_data + 120);
    TEST_ASSERT(om->om_len == 221);

    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == 1);
    TEST_ASSERT(SLIST_NEXT(om, om_next) == NULL);
    os_mbuf_test_misc_assert_sane(om, NULL, 221, 221, 18);

    v = os_mbuf_extend(om, 1);
    TEST_ASSERT(v != NULL);
    TEST_ASSERT(v == om->om_data + 221);
    TEST_ASSERT(om->om_len == 222);

    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == 0);
    TEST_ASSERT(SLIST_NEXT(om, om_next) == NULL);
    os_mbuf_test_misc_assert_sane(om, NULL, 222, 222, 18);

    /* Overflow into next buffer. */
    v = os_mbuf_extend(om, 1);
    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == 0);
    TEST_ASSERT(SLIST_NEXT(om, om_next) != NULL);

    TEST_ASSERT(v == SLIST_NEXT(om, om_next)->om_data);
    TEST_ASSERT(om->om_len == 222);
    TEST_ASSERT(SLIST_NEXT(om, om_next)->om_len == 1);
    os_mbuf_test_misc_assert_sane(om, NULL, 222, 223, 18);

    /*** Attempt to extend by an amount larger than max buf size fails. */
    v = os_mbuf_extend(om, 257);
    TEST_ASSERT(v == NULL);
    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == 0);
    TEST_ASSERT(SLIST_NEXT(om, om_next) != NULL);

    TEST_ASSERT(om->om_len == 222);
    TEST_ASSERT(SLIST_NEXT(om, om_next)->om_len == 1);
    os_mbuf_test_misc_assert_sane(om, NULL, 222, 223, 18);
}

TEST_CASE(os_mbuf_test_pullup)
{
    struct os_mbuf *om;
    struct os_mbuf *om2;
    int rc;

    os_mbuf_test_setup();

    /*** Free when too much os_mbuf_test_data is requested. */
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om != NULL);

    om = os_mbuf_pullup(om, 1);
    TEST_ASSERT(om == NULL);

    /*** No effect when all os_mbuf_test_data is already at the start. */
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om != NULL);

    rc = os_mbuf_append(om, os_mbuf_test_data, 1);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data, 1, 1, 18);

    om = os_mbuf_pullup(om, 1);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data, 1, 1, 18);

    /*** Spread os_mbuf_test_data across four mbufs. */
    om2 = os_mbuf_get(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om2 != NULL);
    rc = os_mbuf_append(om2, os_mbuf_test_data + 1, 1);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_concat(om, om2);

    om2 = os_mbuf_get(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om2 != NULL);
    rc = os_mbuf_append(om2, os_mbuf_test_data + 2, 1);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_concat(om, om2);

    om2 = os_mbuf_get(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om2 != NULL);
    rc = os_mbuf_append(om2, os_mbuf_test_data + 3, 1);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_concat(om, om2);

    TEST_ASSERT_FATAL(OS_MBUF_PKTLEN(om) == 4);

    om = os_mbuf_pullup(om, 4);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data, 4, 4, 18);

    os_mbuf_free_chain(om);

    /*** Require an allocation. */
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om != NULL);

    om->om_data += 100;
    rc = os_mbuf_append(om, os_mbuf_test_data, 100);
    TEST_ASSERT_FATAL(rc == 0);

    om2 = os_mbuf_get(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om2 != NULL);
    rc = os_mbuf_append(om2, os_mbuf_test_data + 100, 100);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_concat(om, om2);

    om = os_mbuf_pullup(om, 200);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data, 200, 200, 18);

    /*** Partial pullup. */
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om != NULL);

    om->om_data += 100;
    rc = os_mbuf_append(om, os_mbuf_test_data, 100);
    TEST_ASSERT_FATAL(rc == 0);

    om2 = os_mbuf_get(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om2 != NULL);
    rc = os_mbuf_append(om2, os_mbuf_test_data + 100, 100);
    TEST_ASSERT_FATAL(rc == 0);
    os_mbuf_concat(om, om2);

    om = os_mbuf_pullup(om, 150);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data, 150, 200, 18);
}

TEST_CASE(os_mbuf_test_adj)
{
    struct os_mbuf *om;
    int rc;

    os_mbuf_test_setup();

    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om != NULL);

    rc = os_mbuf_append(om, os_mbuf_test_data, sizeof os_mbuf_test_data);
    TEST_ASSERT_FATAL(rc == 0);

    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data, 222,
                                  sizeof os_mbuf_test_data, 18);

    /* Remove from the front. */
    os_mbuf_adj(om, 10);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data + 10, 212,
                                  sizeof os_mbuf_test_data - 10, 18);

    /* Remove from the back. */
    os_mbuf_adj(om, -10);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data + 10, 212,
                                  sizeof os_mbuf_test_data - 20, 18);

    /* Remove entire first buffer. */
    os_mbuf_adj(om, 212);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data + 222, 0,
                                  sizeof os_mbuf_test_data - 232, 18);

    /* Remove next buffer. */
    os_mbuf_adj(om, 256);
    os_mbuf_test_misc_assert_sane(om, os_mbuf_test_data + 478, 0,
                                  sizeof os_mbuf_test_data - 488, 18);

    /* Remove more data than is present. */
    os_mbuf_adj(om, 1000);
    os_mbuf_test_misc_assert_sane(om, NULL, 0, 0, 18);
}

TEST_SUITE(os_mbuf_test_suite)
{
    os_mbuf_test_alloc();
    os_mbuf_test_dup();
    os_mbuf_test_append();
    os_mbuf_test_pullup();
    os_mbuf_test_extend();
    os_mbuf_test_adj();
    os_mbuf_test_get_pkthdr();
}
