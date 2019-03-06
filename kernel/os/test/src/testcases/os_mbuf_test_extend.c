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

TEST_CASE_SELF(os_mbuf_test_extend)
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
