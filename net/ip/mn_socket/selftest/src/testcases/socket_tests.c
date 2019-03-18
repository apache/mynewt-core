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

#include "mn_sock_test.h"

TEST_CASE_TASK(socket_tests)
{
    os_sem_init(&test_sem, 0);

    sock_open_close();
    sock_listen();
    sock_tcp_connect();
    sock_udp_data();
    sock_tcp_data();
    sock_itf_list();
    sock_udp_ll();
    sock_udp_mcast_v4();
    sock_udp_mcast_v6();
}
