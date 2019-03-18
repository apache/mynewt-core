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

#include <stdio.h>
#include <string.h>

#include "os/mynewt.h"
#include "testutil/testutil.h"

#include "mn_socket/mn_socket.h"
#include "mn_sock_test.h"

#define MB_CNT 10
#define MB_SZ  512
static uint8_t test_mbuf_area[MB_CNT * MB_SZ];
static struct os_mempool test_mbuf_mpool;
static struct os_mbuf_pool test_mbuf_pool;

TEST_CASE_DECL(inet_pton_test)
TEST_CASE_DECL(inet6_pton_test)
TEST_CASE_DECL(inet_ntop_test)
TEST_CASE_DECL(socket_tests)

static void
mn_socket_test_init(void *arg)
{
    int rc;

    rc = os_mempool_init(&test_mbuf_mpool, MB_CNT, MB_SZ,
                         test_mbuf_area, "mb");
    TEST_ASSERT_FATAL(rc == 0);

    rc = os_mbuf_pool_init(&test_mbuf_pool, &test_mbuf_mpool,
                           MB_CNT, MB_CNT);
    TEST_ASSERT_FATAL(rc == 0);

    rc = os_msys_register(&test_mbuf_pool);
    TEST_ASSERT_FATAL(rc == 0);
}

TEST_SUITE(mn_socket_test_all)
{
    tu_suite_set_pre_test_cb(mn_socket_test_init, NULL);

    inet_pton_test();
    inet6_pton_test();
    inet_ntop_test();
    socket_tests();
}

int
main(int argc, char **argv)
{
    mn_socket_test_all();
    return tu_any_failed;
}
