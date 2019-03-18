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

static uint16_t
omtw_chain_len(const struct os_mbuf *om)
{
    const struct os_mbuf *cur;
    uint16_t len;

    len = 0;
    for (cur = om; cur != NULL; cur = SLIST_NEXT(cur, om_next)) {
        len += cur->om_len;
    }

    return len;
}

TEST_CASE_SELF(os_mbuf_test_widen)
{
    struct os_mbuf *om;
    int rc;
    int i;

    os_mbuf_test_setup();

    om = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL, "Error allocating mbuf");
    os_mbuf_test_misc_assert_sane(om, NULL, 0, 0, 0);

    /*** Invalid offset. */
    rc = os_mbuf_widen(om, 1, 10);
    TEST_ASSERT_FATAL(rc == SYS_EINVAL);

    /*** No pkthdr; widen within one buffer. */
    rc = os_mbuf_append(om, os_mbuf_test_data, 5);
    TEST_ASSERT_FATAL(rc == 0);

    rc = os_mbuf_widen(om, 3, 5);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT(om->om_len == 10);
    TEST_ASSERT(os_mbuf_cmpf(om, 0, os_mbuf_test_data, 3) == 0);
    TEST_ASSERT(os_mbuf_cmpf(om, 8, os_mbuf_test_data + 3, 2) == 0);

    /*** No pkthdr; widen across several buffers. */
    os_mbuf_free_chain(om);
    om = os_mbuf_get(&os_mbuf_pool, 0);

    rc = os_mbuf_append(om, os_mbuf_test_data, 10);
    TEST_ASSERT_FATAL(rc == 0);

    rc = os_mbuf_widen(om, 8, 490);
    TEST_ASSERT_FATAL(rc == 0);

    TEST_ASSERT(omtw_chain_len(om) == 500);
    TEST_ASSERT(os_mbuf_cmpf(om, 0, os_mbuf_test_data, 8) == 0);
    TEST_ASSERT(os_mbuf_cmpf(om, 498, os_mbuf_test_data + 8, 2) == 0);

    /*** No pkthdr; small widen, many mbufs. */
    os_mbuf_free_chain(om);
    om = os_mbuf_get(&os_mbuf_pool, 0);

    os_mbuf_append(om, os_mbuf_test_data, 300);
    rc = os_mbuf_widen(om, 200, 5);
    TEST_ASSERT_FATAL(rc == 0);

    TEST_ASSERT(omtw_chain_len(om) == 305);
    TEST_ASSERT(os_mbuf_cmpf(om, 0, os_mbuf_test_data, 200) == 0);
    TEST_ASSERT(os_mbuf_cmpf(om, 205, os_mbuf_test_data + 200, 100) == 0);

    /*** Pkthdr; widen within one buffer. */
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL);

    rc = os_mbuf_append(om, os_mbuf_test_data, 12);
    TEST_ASSERT_FATAL(rc == 0);

    rc = os_mbuf_widen(om, 7, 4);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT(OS_MBUF_PKTLEN(om) == 16);
    TEST_ASSERT(om->om_len == 16);
    TEST_ASSERT(os_mbuf_cmpf(om, 0, os_mbuf_test_data, 7) == 0);
    TEST_ASSERT(os_mbuf_cmpf(om, 11, os_mbuf_test_data + 7, 5) == 0);

    /*** Pkthdr; widen across several buffers. */
    os_mbuf_free_chain(om);
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL);

    rc = os_mbuf_append(om, os_mbuf_test_data, 52);
    TEST_ASSERT_FATAL(rc == 0);

    rc = os_mbuf_widen(om, 38, 830);
    TEST_ASSERT_FATAL(rc == 0);

    TEST_ASSERT(OS_MBUF_PKTLEN(om) == 882);
    TEST_ASSERT(omtw_chain_len(om) == 882);
    TEST_ASSERT(os_mbuf_cmpf(om, 0, os_mbuf_test_data, 38) == 0);
    TEST_ASSERT(os_mbuf_cmpf(om, 868, os_mbuf_test_data + 38, 14) == 0);

    /*** Pkthdr; widen at edge. */
    os_mbuf_free_chain(om);
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL);

    os_mbuf_append(om, os_mbuf_test_data, 200);
    rc = os_mbuf_widen(om, 200, 5);
    TEST_ASSERT_FATAL(rc == 0);

    TEST_ASSERT(OS_MBUF_PKTLEN(om) == 205);
    TEST_ASSERT(omtw_chain_len(om) == 205);
    TEST_ASSERT(os_mbuf_cmpf(om, 0, os_mbuf_test_data, 200) == 0);

    /*** Ensure no memory leaks. */
    for (i = 0; i < 100; i++) {
        os_mbuf_free_chain(om);
        om = os_mbuf_get_pkthdr(&os_mbuf_pool, 0);
        TEST_ASSERT_FATAL(om != NULL);

        os_mbuf_append(om, os_mbuf_test_data, 10);
        rc = os_mbuf_widen(om, 5, 10000);
        TEST_ASSERT_FATAL(rc == SYS_ENOMEM);
    }
}
