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
#include "os_test_priv.h"

/* 
 * NOTE: currently, the buffer size cannot be changed as some tests are
 * hard-coded for this size.
 */
#define MBUF_TEST_POOL_BUF_SIZE     (256)
#define MBUF_TEST_POOL_BUF_COUNT    (10)

#define MBUF_TEST_DATA_LEN          (1024)

os_membuf_t os_mbuf_membuf[OS_MEMPOOL_SIZE(MBUF_TEST_POOL_BUF_SIZE,
        MBUF_TEST_POOL_BUF_COUNT)];

struct os_mbuf_pool os_mbuf_pool;
struct os_mempool os_mbuf_mempool;
uint8_t os_mbuf_test_data[MBUF_TEST_DATA_LEN];

void
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

void
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

TEST_CASE_DECL(os_mbuf_test_alloc)
TEST_CASE_DECL(os_mbuf_test_dup)
TEST_CASE_DECL(os_mbuf_test_append)
TEST_CASE_DECL(os_mbuf_test_pullup)
TEST_CASE_DECL(os_mbuf_test_extend)
TEST_CASE_DECL(os_mbuf_test_adj)
TEST_CASE_DECL(os_mbuf_test_get_pkthdr)

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
