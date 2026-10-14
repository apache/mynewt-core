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

#ifndef H_COREMQTT_TEST_
#define H_COREMQTT_TEST_

#include "os/mynewt.h"
#include "testutil/testutil.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_BROKER_PORT 18883

/*
 * Minimal MQTT v5.0 broker for tests, listening on 127.0.0.1. Accepts one
 * client at a time, acknowledges CONNECT, SUBSCRIBE, PUBLISH (QoS 1) and
 * PINGREQ, and echoes every PUBLISH back to the client. Runs in the socket
 * provider's task.
 */
struct test_broker {
    struct mn_socket *listen;
    struct mn_socket *conn;
    uint8_t rx[4608];
    size_t rx_len;
    /* Close the connection instead of sending CONNACK */
    volatile int close_on_connect;
    volatile int connects;
    volatile int publishes;
    volatile int pings;
    volatile int disconnects;
    /* Non-zero if the broker failed (unexpected input or send error) */
    volatile int err;
};

extern struct test_broker test_broker;

void test_broker_start(void);
void test_broker_wait_idle(void);

#ifdef __cplusplus
}
#endif

#endif
