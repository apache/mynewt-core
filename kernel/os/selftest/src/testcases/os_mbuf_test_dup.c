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

TEST_CASE_SELF(os_mbuf_test_dup)
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
