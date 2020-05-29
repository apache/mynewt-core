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

/**
 * Interface for the TCP/IPv4 CoAP transport.
 *
 * Unlike other transports, this one does not listen for new connections on its
 * own.  Instead, the application needs to populate the transport with
 * connections to be managed using oc_tcp4_add_conn().
 */

#ifndef H_MYNEWT_TCP4_
#define H_MYNEWT_TCP4_

#include "oic/port/mynewt/ip.h"

struct oc_endpoint_tcp {
    struct oc_endpoint_ip ep_ip;
    struct mn_socket *sock;
};

extern uint8_t oc_tcp4_transport_id;

typedef void oc_tcp4_err_fn(struct mn_socket *mn, int status, void *arg);

/**
 * Populates `ep` with the endpoint corresponding to the given socket.
 */
int oc_tcp4_ep_create(struct oc_endpoint_tcp *ep, struct mn_socket *sock);

/**
 * Adds the given socket to the list of managed CoAP TCP connections.  After
 * adding a socket, the application should not directly read from or write to
 * the socket.
 *
 * @param sock                  The socket to add.
 * @param on_err                Callback that gets called when the connection
 *                                  gets into a bad state.
 * @param arg                   Optional argument to pass to the on_err callback.
 *
 * @return                      0 on success; SYS_E[...] code on error.
 */
int oc_tcp4_add_conn(struct mn_socket *sock, oc_tcp4_err_fn *on_err,
                     void *arg);

/**
 * Removes the given socket to the list of managed CoAP TCP connections.
 *
 * @param sock                  The socket to remove.
 *
 * @return                      0 on success; SYS_E[...] code on error.
 */
int oc_tcp4_del_conn(struct mn_socket *sock);

#endif
