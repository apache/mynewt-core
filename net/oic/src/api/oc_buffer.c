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

#include <stdint.h>
#include <stdio.h>

#include "os/mynewt.h"

#include "oic/port/mynewt/config.h"
#include "messaging/coap/engine.h"

#ifdef OC_SECURITY
#include "security/oc_dtls.h"
#endif

#include "oic/oc_buffer.h"
#include "oic/port/mynewt/adaptor.h"
#include "oic/port/mynewt/transport.h"

static struct os_mqueue oc_inq;
static struct os_mqueue oc_outq;

struct os_mbuf *
oc_allocate_mbuf(struct oc_endpoint *oe)
{
    struct os_mbuf *m;
    int ep_size;

    ep_size = oc_endpoint_size(oe);

    /* get a packet header */
    m = os_msys_get_pkthdr(0, ep_size);
    if (!m) {
        return NULL;
    }
    memcpy(OC_MBUF_ENDPOINT(m), oe, ep_size);
    return m;
}

void
oc_recv_message(struct os_mbuf *m)
{
    int rc;

    rc = os_mqueue_put(&oc_inq, oc_evq_get(), m);
    assert(rc == 0);
}

void
oc_send_message(struct os_mbuf *m)
{
    int rc;

    rc = os_mqueue_put(&oc_outq, oc_evq_get(), m);
    assert(rc == 0);
}

static void
oc_buffer_tx(struct os_event *ev)
{
    struct os_mbuf *m;

    while ((m = os_mqueue_get(&oc_outq)) != NULL) {
        STAILQ_NEXT(OS_MBUF_PKTHDR(m), omp_next) = NULL;
        OC_LOG_DEBUG("oc_buffer_tx: ");
        OC_LOG_ENDPOINT(LOG_LEVEL_DEBUG, OC_MBUF_ENDPOINT(m));
#ifdef OC_CLIENT
        if (OC_MBUF_ENDPOINT(m)->ep.oe_flags & OC_ENDPOINT_MULTICAST) {
            oc_send_multicast_message(m);
        } else {
#endif
#ifdef OC_SECURITY
            /* XXX convert this */
            if (OC_MBUF_ENDPOINT(m)->flags & SECURED) {
                OC_LOG_DEBUG("oc_buffer_tx: DTLS\n");

                if (!oc_sec_dtls_connected(oe)) {
                    oc_process_post(&oc_dtls_handler,
                                    oc_events[INIT_DTLS_CONN_EVENT], m);
                } else {
                    oc_process_post(&oc_dtls_handler,
                                    oc_events[RI_TO_DTLS_EVENT], m);
                }
            } else
#endif
            {
                oc_send_buffer(m);
            }
#ifdef OC_CLIENT
        }
#endif
    }
}

static void
oc_buffer_rx(struct os_event *ev)
{
    struct os_mbuf *m;
#if defined(OC_SECURITY)
    uint8_t b;
#endif

    while ((m = os_mqueue_get(&oc_inq)) != NULL) {
        OC_LOG_DEBUG("oc_buffer_rx: ");
        OC_LOG_ENDPOINT(LOG_LEVEL_DEBUG, OC_MBUF_ENDPOINT(m));

#ifdef OC_SECURITY
        /*
         * XXX make sure first byte is within first mbuf
         */
        b = m->om_data[0];
        if (b > 19 && b < 64) {
            OC_LOG_DEBUG("oc_buffer_rx: encrypted request\n");
            oc_process_post(&oc_dtls_handler, oc_events[UDP_TO_DTLS_EVENT], m);
        } else {
            coap_receive(m);
        }
#else
        coap_receive(&m);
#endif
        if (m) {
            os_mbuf_free_chain(m);
        }
    }
}

void
oc_buffer_init(void)
{
    os_mqueue_init(&oc_inq, oc_buffer_rx, NULL);
    os_mqueue_init(&oc_outq, oc_buffer_tx, NULL);
}

