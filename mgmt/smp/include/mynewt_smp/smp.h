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

#ifndef _SMP_H_
#define _SMP_H_

/**
 * \defgroup SMP SMP server and transport
 * @{
 */

#include <tinycbor/cbor.h>
#include <inttypes.h>
#include "os/mynewt.h"
#include "smp/smp.h"

#ifdef __cplusplus
extern "C" {
#endif

struct smp_transport;
extern const struct mgmt_streamer_cfg g_smp_cbor_cfg;
smp_tx_rsp_fn smp_tx_rsp;

/**
 * Transmit function. The supplied mbuf is always consumed, regardless of
 * return code.
 */
typedef int (*smp_transport_out_func_t)(struct os_mbuf *m);

/**
 * MTU query function.  The supplied mbuf should contain a request received
 * from the peer whose MTU is being queried.  This function takes an mbuf
 * parameter because some transports store connection-specific information in
 * the mbuf user header (e.g., the BLE transport stores the connection handle).
 *
 * @return                      The transport's MTU;
 *                              0 if transmission is currently not possible.
 */
typedef uint16_t (*smp_transport_get_mtu_func_t)(struct os_mbuf *m);

struct smp_transport {
    struct smp_streamer st_streamer;
    struct os_mqueue st_imq;
    smp_transport_out_func_t st_output;
    smp_transport_get_mtu_func_t st_get_mtu;
};

void smp_event_put(struct os_event *ev);
int smp_transport_init(struct smp_transport *st,
        smp_transport_out_func_t output_func,
        smp_transport_get_mtu_func_t get_mtu_func);
int smp_rx_req(struct smp_transport *st, struct os_mbuf *req);
struct os_eventq *mgmt_evq_get(void);

#ifdef __cplusplus
}
#endif

/**
 * @} SMP
 */

#endif /* _SMP_H */
