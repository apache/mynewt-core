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

TEST_CASE_SELF(os_mbuf_test_pullup)
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
