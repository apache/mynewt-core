/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef OC_CONNECTIVITY_H
#define OC_CONNECTIVITY_H

#include <stdint.h>
#include <assert.h>

#include "oic/port/mynewt/config.h"
#include "oic/port/mynewt/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * OC endpoint data structure comes in different variations,
 * depending on type of transport.
 */
struct oc_ep_hdr {
    uint8_t oe_type:3;          /* index to oc_transports array */
    uint8_t oe_flags:5;         /* OC_ENDPOINT flags */
};

#define OC_ENDPOINT_MULTICAST   (1 << 0)
#define OC_ENDPOINT_SECURED     (1 << 1)

/*
 * Use this when reserving memory for oc_endpoint of unknown type.
 */
typedef struct oc_endpoint {
    struct oc_ep_hdr ep;
    uint8_t _res[23];          /* based on size of oc_endpoint_ip6 */
} oc_endpoint_t;

/*
 * Plain oc_endpoint for multicast target.
 */
struct oc_endpoint_plain {
    struct oc_ep_hdr ep;
};

static inline int
oc_endpoint_size(struct oc_endpoint *oe)
{
    assert(oc_transports[oe->ep.oe_type]);
    return oc_transports[oe->ep.oe_type]->ot_ep_size(oe);
}

/*
 * Whether transport uses TCP-style headers or not.
 */
static inline int
oc_endpoint_use_tcp(struct oc_endpoint *oe)
{
    return oc_transports[oe->ep.oe_type]->ot_flags & OC_TRANSPORT_USE_TCP;
}

/*
 * Whether underlying transport has connections or not.
 * This normally is indicated by whether TCP style header are used or not.
 * Allowing transport to override that.
 */
static inline int
oc_endpoint_has_conn(struct oc_endpoint *oe)
{
    const struct oc_transport *ot;

    ot = oc_transports[oe->ep.oe_type];
    if (ot->ot_ep_has_conn) {
        return ot->ot_ep_has_conn(oe);
    }
    return oc_endpoint_use_tcp(oe);
}

#define OC_MBUF_ENDPOINT(m) ((struct oc_endpoint *) OS_MBUF_USRHDR(m))

#ifdef OC_SECURITY
uint16_t oc_connectivity_get_dtls_port(void);
#endif /* OC_SECURITY */

/*
 * Callback mechanism for connection-oriented transports, to be
 * called when new connection is established, and when an existing
 * connection is closed
 */
#define OC_ENDPOINT_CONN_EV_OPEN        1
#define OC_ENDPOINT_CONN_EV_CLOSE       2
struct oc_conn_cb {
    SLIST_ENTRY(oc_conn_cb) occ_next;
    void (*occ_func)(struct oc_endpoint *, int ev);
};
void oc_conn_cb_register(struct oc_conn_cb *cb);

struct oc_conn_ev {
    STAILQ_ENTRY(oc_conn_ev) oce_next;
    struct oc_endpoint oce_oe;
    int oce_type;
};
/*
 * Called by underlying connection-oriented transports to notify
 * about connection state changes.
 */
struct oc_conn_ev *oc_conn_ev_alloc(void);
void oc_conn_created(struct oc_conn_ev *);
void oc_conn_removed(struct oc_conn_ev *);

enum oc_resource_properties oc_get_trans_security(const struct oc_endpoint *oe);
int oc_connectivity_init(void);
void oc_connectivity_shutdown(void);

void oc_send_buffer(struct os_mbuf *);
void oc_send_multicast_message(struct os_mbuf *);

void oc_recv_message(struct os_mbuf *m);

#ifdef __cplusplus
}
#endif

#endif /* OC_CONNECTIVITY_H */
