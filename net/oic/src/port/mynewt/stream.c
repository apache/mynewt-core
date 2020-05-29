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
 * stream.c: Utility code for adaptors that implement the CoAP-over-TCP
 * protocol (Bluetooth, TCP/IP, etc.).
 */

#include "os/mynewt.h"
#include "oic/port/mynewt/stream.h"
#include "oic/messaging/coap/coap.h"

int
oc_stream_reass(struct oc_stream_reassembler *r, struct os_mbuf *om1,
                void *ep_desc, struct os_mbuf **out_pkt)
{
    struct os_mbuf_pkthdr *pkt1;
    struct os_mbuf_pkthdr *pkt2;
    struct os_mbuf *om2;
    uint8_t hdr[sizeof(struct coap_tcp_hdr32)];

    *out_pkt = NULL;

    pkt1 = OS_MBUF_PKTHDR(om1);
    assert(pkt1);

    /* Find the packet that this fragment belongs to, if any. */
    STAILQ_FOREACH(pkt2, &r->pkt_q, omp_next) {
        om2 = OS_MBUF_PKTHDR_TO_MBUF(pkt2);
        if (r->ep_match(OS_MBUF_USRHDR(om2), ep_desc)) {
            /* Data from same connection.  Append. */
            os_mbuf_concat(om2, om1);
            os_mbuf_copydata(om2, 0, sizeof(hdr), hdr);

            if (coap_tcp_msg_size(hdr, sizeof(hdr)) <= pkt2->omp_len) {
                /* Packet complete. */
                STAILQ_REMOVE(&r->pkt_q, pkt2, os_mbuf_pkthdr, omp_next);
                *out_pkt = om2;
            }

            return 0;
        }
    }

    /* New frame, need to add endpoint to the front.  Check if there is enough
     * space available. If not, allocate a new pkthdr.
     */
    if (OS_MBUF_USRHDR_LEN(om1) < r->endpoint_size) {
        om2 = os_msys_get_pkthdr(0, r->endpoint_size);
        if (!om2) {
            os_mbuf_free_chain(om1);
            return SYS_ENOMEM;
        }
        OS_MBUF_PKTHDR(om2)->omp_len = pkt1->omp_len;
        SLIST_NEXT(om2, om_next) = om1;
    } else {
        om2 = om1;
    }

    r->ep_fill(OS_MBUF_USRHDR(om2), ep_desc);
    pkt2 = OS_MBUF_PKTHDR(om2);

    os_mbuf_copydata(om2, 0, sizeof(hdr), hdr);
    if (coap_tcp_msg_size(hdr, sizeof(hdr)) > pkt2->omp_len) {
        STAILQ_INSERT_TAIL(&r->pkt_q, pkt2, omp_next);
    } else {
        *out_pkt = om2;
    }
    return 0;
}

void
oc_stream_conn_del(struct oc_stream_reassembler *r, void *ep_desc)
{
    struct os_mbuf_pkthdr *pkt;
    struct os_mbuf *m;
    struct oc_conn_ev *oce;

    STAILQ_FOREACH(pkt, &r->pkt_q, omp_next) {
        m = OS_MBUF_PKTHDR_TO_MBUF(pkt);
        if (r->ep_match(OS_MBUF_USRHDR(m), ep_desc)) {
            STAILQ_REMOVE(&r->pkt_q, pkt, os_mbuf_pkthdr, omp_next);
            os_mbuf_free_chain(m);
            break;
        }
    }

    /* Notify listeners that this connection is gone. */

    oce = oc_conn_ev_alloc();
    assert(oce);

    memset(&oce->oce_oe, 0, sizeof(oce->oce_oe));
    r->ep_fill(&oce->oce_oe, ep_desc);
    oc_conn_removed(oce);
}
