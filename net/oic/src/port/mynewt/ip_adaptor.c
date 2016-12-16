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

#include <syscfg/syscfg.h>
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1)
#include <assert.h>
#include <string.h>

#include <os/os.h>
#include <os/endian.h>

#include <log/log.h>
#include <mn_socket/mn_socket.h>

#include "port/oc_connectivity.h"
#include "oic/oc_log.h"
#include "api/oc_buffer.h"
#include "adaptor.h"

static void oc_event_ip(struct os_event *ev);

static struct os_event oc_sock_read_event = {
    .ev_cb = oc_event_ip,
};

#ifdef OC_SECURITY
#error This implementation does not yet support security
#endif


#define COAP_PORT_UNSECURED (5683)
/* TODO use inet_pton when its available */
static const struct mn_in6_addr coap_all_nodes_v6 = {
    .s_addr = {0xFF,0x02,0x00,0x00,0x00,0x00,0x00,0x00,
               0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFD}
};


/* sockets to use for coap unicast and multicast */
static struct mn_socket *ucast;

#if (MYNEWT_VAL(OC_SERVER) == 1)
static struct mn_socket *mcast;
#endif

static void
oc_send_buffer_ip_int(struct os_mbuf *m, int is_mcast)
{
    struct mn_sockaddr_in6 to;
    struct oc_endpoint *oe;
    struct mn_itf itf;
    struct mn_itf itf2;
    struct os_mbuf *n;
    int rc;

    assert(OS_MBUF_USRHDR_LEN(m) >= sizeof(struct oc_endpoint_ip));
    oe = OC_MBUF_ENDPOINT(m);

    to.msin6_len = sizeof(to);
    to.msin6_family = MN_AF_INET6;
    to.msin6_port = htons(oe->oe_ip.v6.port);
    to.msin6_scope_id = oe->oe_ip.v6.scope;
    memcpy(&to.msin6_addr, oe->oe_ip.v6.address, sizeof(to.msin6_addr));

    if (is_mcast) {
        memset(&itf, 0, sizeof(itf));
        memset(&itf2, 0, sizeof(itf2));

        while (1) {
            rc = mn_itf_getnext(&itf);
            if (rc) {
                break;
            }

            if (0 == (itf.mif_flags & MN_ITF_F_UP)) {
                continue;
            }

            if (!itf2.mif_idx) {
                memcpy(&itf2, &itf, sizeof(itf));
                continue;
            }

            n = os_mbuf_dup(m);
            if (!n) {
                break;
            }
            to.msin6_scope_id = itf2.mif_idx;
            rc = mn_sendto(ucast, n, (struct mn_sockaddr *) &to);
            if (rc != 0) {
                OC_LOG_ERROR("Failed to send buffer %u on itf %d\n",
                             OS_MBUF_PKTHDR(m)->omp_len, to.msin6_scope_id);
                os_mbuf_free_chain(n);
            }
        }
        if (itf2.mif_idx) {
            to.msin6_scope_id = itf2.mif_idx;
            rc = mn_sendto(ucast, m, (struct mn_sockaddr *) &to);
            if (rc != 0) {
                OC_LOG_ERROR("Failed sending buffer %u on itf %d\n",
                             OS_MBUF_PKTHDR(m)->omp_len, to.msin6_scope_id);
                os_mbuf_free_chain(m);
            }
        }
    } else {
        rc = mn_sendto(ucast, m, (struct mn_sockaddr *) &to);
        if (rc != 0) {
            OC_LOG_ERROR("Failed to send buffer %u on itf %d\n",
                         OS_MBUF_PKTHDR(m)->omp_len, to.msin6_scope_id);
            os_mbuf_free_chain(m);
        }
    }
    if (rc) {
        os_mbuf_free_chain(m);
    }
}

void
oc_send_buffer_ip(struct os_mbuf *m)
{
    oc_send_buffer_ip_int(m, 0);
}
void
oc_send_buffer_ip_mcast(struct os_mbuf *m)
{
    oc_send_buffer_ip_int(m, 1);
}

static struct os_mbuf *
oc_attempt_rx_ip_sock(struct mn_socket *rxsock)
{
    int rc;
    struct os_mbuf *m;
    struct os_mbuf *n;
    struct oc_endpoint *oe;
    struct mn_sockaddr_in6 from;

    rc = mn_recvfrom(rxsock, &n, (struct mn_sockaddr *) &from);
    if (rc != 0) {
        return NULL;
    }
    assert(OS_MBUF_IS_PKTHDR(n));

    m = os_msys_get_pkthdr(0, sizeof(struct oc_endpoint_ip));
    if (!m) {
        OC_LOG_ERROR("Could not allocate RX buffer\n");
        goto rx_attempt_err;
    }
    OS_MBUF_PKTHDR(m)->omp_len = OS_MBUF_PKTHDR(n)->omp_len;
    SLIST_NEXT(m, om_next) = n;

    oe = OC_MBUF_ENDPOINT(m);

    oe->oe_ip.flags = IP;
    memcpy(&oe->oe_ip.v6.address, &from.msin6_addr,
           sizeof(oe->oe_ip.v6.address));
    oe->oe_ip.v6.scope = from.msin6_scope_id;
    oe->oe_ip.v6.port = ntohs(from.msin6_port);

    return m;

    /* add the addr info to the message */
rx_attempt_err:
    os_mbuf_free_chain(n);
    return NULL;
}

static struct os_mbuf *
oc_attempt_rx_ip(void)
{
    struct os_mbuf *m;

    m = oc_attempt_rx_ip_sock(ucast);
#if (MYNEWT_VAL(OC_SERVER) == 1)
    if (m == NULL ) {
        m = oc_attempt_rx_ip_sock(mcast);
    }
#endif
    return m;
}

static void oc_socks_readable(void *cb_arg, int err);

union mn_socket_cb oc_sock_cbs = {
    .socket.readable = oc_socks_readable,
    .socket.writable = NULL
};

void
oc_socks_readable(void *cb_arg, int err)
{
    os_eventq_put(oc_evq_get(), &oc_sock_read_event);
}

void
oc_connectivity_shutdown_ip(void)
{
    if (ucast) {
        mn_close(ucast);
    }

#if (MYNEWT_VAL(OC_SERVER) == 1)
    if (mcast) {
        mn_close(mcast);
    }
#endif

}

static void
oc_event_ip(struct os_event *ev)
{
    struct os_mbuf *m;

    while ((m = oc_attempt_rx_ip()) != NULL) {
        oc_recv_message(m);
    }
}

int
oc_connectivity_init_ip(void)
{
    int rc;
    struct mn_sockaddr_in6 sin;
    struct mn_itf itf;

    memset(&itf, 0, sizeof(itf));

    rc = mn_socket(&ucast, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    if ( rc != 0 || !ucast ) {
        OC_LOG_ERROR("Could not create oc unicast socket\n");
        return rc;
    }
    mn_socket_set_cbs(ucast, ucast, &oc_sock_cbs);

#if (MYNEWT_VAL(OC_SERVER) == 1)
    rc = mn_socket(&mcast, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    if ( rc != 0 || !mcast ) {
        mn_close(ucast);
        OC_LOG_ERROR("Could not create oc multicast socket\n");
        return rc;
    }
    mn_socket_set_cbs(mcast, mcast, &oc_sock_cbs);
#endif

    sin.msin6_len = sizeof(sin);
    sin.msin6_family = MN_AF_INET6;
    sin.msin6_port = 0;
    sin.msin6_flowinfo = 0;
    memcpy(&sin.msin6_addr, nm_in6addr_any, sizeof(sin.msin6_addr));

    rc = mn_bind(ucast, (struct mn_sockaddr *)&sin);
    if (rc != 0) {
        OC_LOG_ERROR("Could not bind oc unicast socket\n");
        goto oc_connectivity_init_err;
    }

#if (MYNEWT_VAL(OC_SERVER) == 1)
    /* Set socket option to join multicast group on all valid interfaces */
    while (1) {
        struct mn_mreq join;

        rc = mn_itf_getnext(&itf);
        if (rc) {
            break;
        }

        if (0 == (itf.mif_flags & MN_ITF_F_UP)) {
            continue;
        }

        join.mm_addr.v6 = coap_all_nodes_v6;
        join.mm_idx = itf.mif_idx;
        join.mm_family = MN_AF_INET6;

        rc = mn_setsockopt(mcast, MN_SO_LEVEL, MN_MCAST_JOIN_GROUP, &join);
        if (rc != 0) {
            continue;
        }

        OC_LOG_DEBUG("Joined Coap mcast group on %s\n", itf.mif_name);
    }

    sin.msin6_port = htons(COAP_PORT_UNSECURED);
    rc = mn_bind(mcast, (struct mn_sockaddr *)&sin);
    if (rc != 0) {
        OC_LOG_ERROR("Could not bind oc multicast socket\n");
        goto oc_connectivity_init_err;
    }
#endif

    return 0;

oc_connectivity_init_err:
    oc_connectivity_shutdown();
    return rc;
}

#endif
