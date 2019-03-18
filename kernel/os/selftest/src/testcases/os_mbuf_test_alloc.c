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

TEST_CASE_SELF(os_mbuf_test_alloc)
{
    struct os_mbuf *m;
    int rc;

    os_mbuf_test_setup();

    m = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(m != NULL, "Error allocating mbuf");

    rc = os_mbuf_free(m);
    TEST_ASSERT_FATAL(rc == 0, "Error free'ing mbuf %d", rc);
}
