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
    int trailingspace_check;
    int om_len_check;
    int pkthdr_len_check;

    os_mbuf_test_setup();

    /*** Series of successful extensions. */
    om = os_mbuf_get_pkthdr(&os_mbuf_pool, 10);
    TEST_ASSERT_FATAL(om != NULL);

    trailingspace_check = MBUF_TEST_POOL_BUF_SIZE - sizeof(struct os_mbuf) -
                          sizeof(struct os_mbuf_pkthdr) - 10;
    om_len_check = 0;
    pkthdr_len_check = 10 + sizeof(struct os_mbuf_pkthdr);

    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == trailingspace_check);
    TEST_ASSERT(SLIST_NEXT(om, om_next) == NULL);
    os_mbuf_test_misc_assert_sane(om, NULL, om_len_check, om_len_check, pkthdr_len_check);

    v = os_mbuf_extend(om, 20);
    trailingspace_check -= 20;
    om_len_check += 20;
    TEST_ASSERT(v != NULL);
    TEST_ASSERT(v == om->om_data);
    TEST_ASSERT(om->om_len == 20);

    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == trailingspace_check);
    TEST_ASSERT(SLIST_NEXT(om, om_next) == NULL);
    os_mbuf_test_misc_assert_sane(om, NULL, om_len_check, om_len_check, pkthdr_len_check);

    v = os_mbuf_extend(om, 100);
    trailingspace_check -= 100;
    om_len_check += 100;
    TEST_ASSERT(v != NULL);
    TEST_ASSERT(v == om->om_data + 20);
    TEST_ASSERT(om->om_len == om_len_check);

    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == trailingspace_check);
    TEST_ASSERT(SLIST_NEXT(om, om_next) == NULL);
    os_mbuf_test_misc_assert_sane(om, NULL, om_len_check, om_len_check, pkthdr_len_check);

    v = os_mbuf_extend(om, 101);
    trailingspace_check -= 101;
    om_len_check += 101;
    TEST_ASSERT(v != NULL);
    TEST_ASSERT(v == om->om_data + 120);
    TEST_ASSERT(om->om_len == om_len_check);

    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == trailingspace_check);
    TEST_ASSERT(SLIST_NEXT(om, om_next) == NULL);
    os_mbuf_test_misc_assert_sane(om, NULL, om_len_check, om_len_check, pkthdr_len_check);

    v = os_mbuf_extend(om, trailingspace_check);
    om_len_check += trailingspace_check;
    trailingspace_check = 0;
    TEST_ASSERT(v != NULL);
    TEST_ASSERT(v == om->om_data + 221);
    TEST_ASSERT(om->om_len == om_len_check);

    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == trailingspace_check);
    TEST_ASSERT(SLIST_NEXT(om, om_next) == NULL);
    os_mbuf_test_misc_assert_sane(om, NULL, om_len_check, om_len_check, pkthdr_len_check);

    /* Overflow into next buffer. */
    v = os_mbuf_extend(om, 1);
    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == trailingspace_check);
    TEST_ASSERT(SLIST_NEXT(om, om_next) != NULL);

    TEST_ASSERT(v == SLIST_NEXT(om, om_next)->om_data);
    TEST_ASSERT(om->om_len == om_len_check);
    TEST_ASSERT(SLIST_NEXT(om, om_next)->om_len == 1);
    os_mbuf_test_misc_assert_sane(om, NULL, om_len_check, om_len_check + 1, pkthdr_len_check);

    /*** Attempt to extend by an amount larger than max buf size fails. */
    v = os_mbuf_extend(om, MBUF_TEST_POOL_BUF_SIZE + 1);
    TEST_ASSERT(v == NULL);
    TEST_ASSERT(OS_MBUF_TRAILINGSPACE(om) == 0);
    TEST_ASSERT(SLIST_NEXT(om, om_next) != NULL);

    TEST_ASSERT(om->om_len == om_len_check);
    TEST_ASSERT(SLIST_NEXT(om, om_next)->om_len == 1);
    os_mbuf_test_misc_assert_sane(om, NULL, om_len_check, om_len_check + 1, pkthdr_len_check);
}
