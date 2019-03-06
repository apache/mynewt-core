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

TEST_CASE_SELF(os_mbuf_test_adj)
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
