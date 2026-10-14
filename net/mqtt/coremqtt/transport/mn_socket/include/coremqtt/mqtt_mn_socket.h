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

#ifndef H_MQTT_MN_SOCKET_
#define H_MQTT_MN_SOCKET_

#include <stdint.h>
#include "os/mynewt.h"
#include "mn_socket/mn_socket.h"
#include "transport_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * coreMQTT network context for a plain TCP connection over Mynewt sockets.
 *
 * coreMQTT leaves the definition of struct NetworkContext to the
 * transport; including this header selects this transport. Fields are
 * private to the transport implementation.
 */
struct NetworkContext {
    struct mn_socket *sock;
    /* Received data not yet consumed by coreMQTT */
    struct os_mbuf *rx_om;
    struct os_sem rx_sem;
    struct os_sem tx_sem;
    /* Error reported by a socket callback, 0 if none */
    volatile int err;
    uint32_t recv_timeout_ms;
};

/**
 * Opens a TCP connection to an MQTT broker.
 *
 * Blocks until the connection is established, fails or times out. Must be
 * called from task context.
 *
 * @param ctx         Network context to initialize.
 * @param addr        Broker address (struct mn_sockaddr_in or
 *                    struct mn_sockaddr_in6).
 * @param timeout_ms  Connection timeout in milliseconds.
 *
 * @return 0 on success, MN_E* error code otherwise.
 */
int mqtt_mn_socket_connect(NetworkContext_t *ctx,
                           const struct mn_sockaddr *addr, uint32_t timeout_ms);

/**
 * Closes the connection and frees buffered data.
 *
 * @param ctx  Network context opened with mqtt_mn_socket_connect().
 */
void mqtt_mn_socket_disconnect(NetworkContext_t *ctx);

/**
 * Sets how long the receive function waits for data.
 *
 * @param ctx         Network context.
 * @param timeout_ms  Receive timeout in milliseconds, 0 to never block.
 */
void mqtt_mn_socket_set_recv_timeout(NetworkContext_t *ctx, uint32_t timeout_ms);

/**
 * Fills a coreMQTT transport interface for this transport.
 *
 * @param ctx  Network context.
 * @param ti   Transport interface to pass to MQTT_Init().
 */
void mqtt_mn_socket_transport(NetworkContext_t *ctx, TransportInterface_t *ti);

/* coreMQTT transport functions, see transport_interface.h */
int32_t mqtt_mn_socket_recv(NetworkContext_t *ctx, void *buf, size_t len);
int32_t mqtt_mn_socket_send(NetworkContext_t *ctx, const void *buf, size_t len);
int32_t mqtt_mn_socket_writev(NetworkContext_t *ctx, TransportOutVector_t *iov,
                              size_t iov_cnt);

#ifdef __cplusplus
}
#endif

#endif
