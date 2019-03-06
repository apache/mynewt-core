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
#ifndef _MN_SOCK_TEST_H
#define _MN_SOCK_TEST_H

#include <stdio.h>
#include <string.h>

#include "os/mynewt.h"
#include "testutil/testutil.h"

#include "mn_socket/mn_socket.h"

extern struct os_sem test_sem;

void sock_open_close(void);
void sock_listen(void);
void sock_tcp_connect(void);
void sock_udp_data(void);
void sock_tcp_data(void);
void sock_itf_list(void);
void sock_udp_ll(void);
void sock_udp_mcast_v4(void);
void sock_udp_mcast_v6(void);

#endif /* _MN_SOCK_TEST_H */
