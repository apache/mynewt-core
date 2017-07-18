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

#ifndef _NEWTMGR_H_
#define _NEWTMGR_H_

#include <tinycbor/cbor.h>
#include <inttypes.h>
#include <os/os.h>
#include <os/endian.h>

#ifdef __cplusplus
extern "C" {
#endif

struct nmgr_transport;

/**
 * Transmit function.  The supplied mbuf is always consumed, regardless of
 * return code.
 */
typedef int (*nmgr_transport_out_func_t)(struct nmgr_transport *nt,
        struct os_mbuf *m);

/**
 * MTU query function.  The supplied mbuf should contain a request received
 * from the peer whose MTU is being queried.  This function takes an mbuf
 * parameter because some transports store connection-specific information in
 * the mbuf user header (e.g., the BLE transport stores the connection handle).
 *
 * @return                      The transport's MTU;
 *                              0 if transmission is currently not possible.
 */
typedef uint16_t (*nmgr_transport_get_mtu_func_t)(struct os_mbuf *m);

struct nmgr_transport {
    struct os_mqueue nt_imq;
    nmgr_transport_out_func_t nt_output;
    nmgr_transport_get_mtu_func_t nt_get_mtu;
};

void nmgr_event_put(struct os_event *ev);
int nmgr_transport_init(struct nmgr_transport *nt,
        nmgr_transport_out_func_t output_func,
        nmgr_transport_get_mtu_func_t get_mtu_func);
int nmgr_rx_req(struct nmgr_transport *nt, struct os_mbuf *req);

#ifdef __cplusplus
}
#endif

#endif /* _NETMGR_H */
