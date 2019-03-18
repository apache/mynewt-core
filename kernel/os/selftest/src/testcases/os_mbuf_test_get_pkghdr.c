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

TEST_CASE_SELF(os_mbuf_test_get_pkthdr)
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
