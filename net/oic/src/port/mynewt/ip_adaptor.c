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

#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "os/mynewt.h"

#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV6) == 1)

#include <log/log.h>
#include <mn_socket/mn_socket.h>
#include <stats/stats.h>

#include "oic/oc_log.h"
#include "oic/port/oc_connectivity.h"
#include "oic/port/mynewt/adaptor.h"
#include "oic/port/mynewt/transport.h"
#include "oic/port/mynewt/ip.h"

static uint8_t oc_ep_ip6_size(const struct oc_endpoint *oe);
static void oc_send_buffer_ip6(struct os_mbuf *m);
static void oc_send_buffer_ip6_mcast(struct os_mbuf *m);
static char *oc_log_ep_ip6(char *ptr, int maxlen, const struct oc_endpoint *);
static int oc_connectivity_init_ip6(void);
void oc_connectivity_shutdown_ip6(void);
static void oc_event_ip6(struct os_event *ev);

static const struct oc_transport oc_ip6_transport = {
    .ot_flags = 0,
    .ot_ep_size = oc_ep_ip6_size,
    .ot_tx_ucast = oc_send_buffer_ip6,
    .ot_tx_mcast = oc_send_buffer_ip6_mcast,
    .ot_get_trans_security = NULL,
    .ot_ep_str = oc_log_ep_ip6,
    .ot_init = oc_connectivity_init_ip6,
    .ot_shutdown = oc_connectivity_shutdown_ip6
};

static struct os_event oc_sock6_read_event = {
    .ev_cb = oc_event_ip6,
};

#define COAP_PORT_UNSECURED (5683)

/* link-local scoped address ff02::fd */
static const struct mn_in6_addr coap_all_nodes_v6 = {
    .s_addr = {
        0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFD
    }
};

STATS_SECT_START(oc_ip_stats)
    STATS_SECT_ENTRY(iframe)
    STATS_SECT_ENTRY(ibytes)
    STATS_SECT_ENTRY(ierr)
    STATS_SECT_ENTRY(oucast)
    STATS_SECT_ENTRY(omcast)
    STATS_SECT_ENTRY(obytes)
    STATS_SECT_ENTRY(oerr)
STATS_SECT_END
static STATS_SECT_DECL(oc_ip_stats) oc_ip_stats;
STATS_NAME_START(oc_ip_stats)
    STATS_NAME(oc_ip_stats, iframe)
    STATS_NAME(oc_ip_stats, ibytes)
    STATS_NAME(oc_ip_stats, ierr)
    STATS_NAME(oc_ip_stats, oucast)
    STATS_NAME(oc_ip_stats, omcast)
    STATS_NAME(oc_ip_stats, obytes)
    STATS_NAME(oc_ip_stats, oerr)
STATS_NAME_END(oc_ip_stats)

/* sockets to use for coap unicast and multicast */
static struct mn_socket *oc_ucast6;

#if (MYNEWT_VAL(OC_SERVER) == 1)
static struct mn_socket *oc_mcast6;
#endif

#ifdef OC_SECURITY
#error This implementation does not yet support security
#endif

static char *
oc_log_ep_ip6(char *ptr, int maxlen, const struct oc_endpoint *oe)
{
    const struct oc_endpoint_ip *oe_ip = (const struct oc_endpoint_ip *)oe;
    int len;

    mn_inet_ntop(MN_PF_INET6, oe_ip->v6.address, ptr, maxlen);
    len = strlen(ptr);
    snprintf(ptr + len, maxlen - len, "-%u", oe_ip->port);
    return ptr;
}

static uint8_t
oc_ep_ip6_size(const struct oc_endpoint *oe)
{
    return sizeof(struct oc_endpoint_ip);
}

static void
oc_send_buffer_ip6_int(struct os_mbuf *m, int is_mcast)
{
    struct mn_sockaddr_in6 to;
    struct oc_endpoint_ip *oe_ip;
    struct mn_itf itf;
    struct mn_itf itf2;
    struct os_mbuf *n;
    int rc;

    assert(OS_MBUF_USRHDR_LEN(m) >= sizeof(struct oc_endpoint_ip));
    oe_ip = (struct oc_endpoint_ip *)OC_MBUF_ENDPOINT(m);
    to.msin6_len = sizeof(to);
    to.msin6_family = MN_AF_INET6;
    to.msin6_port = htons(oe_ip->port);
    to.msin6_flowinfo = 0;
    to.msin6_scope_id = oe_ip->v6.scope;
    memcpy(&to.msin6_addr, oe_ip->v6.address, sizeof(to.msin6_addr));

    STATS_INCN(oc_ip_stats, obytes, OS_MBUF_PKTLEN(m));
    if (is_mcast) {
        memset(&itf, 0, sizeof(itf));
        memset(&itf2, 0, sizeof(itf2));

        while (1) {
            rc = mn_itf_getnext(&itf);
            if (rc) {
                break;
            }

            if ((itf.mif_flags & (MN_ITF_F_UP | MN_ITF_F_MULTICAST)) !=
              (MN_ITF_F_UP | MN_ITF_F_MULTICAST)) {
                continue;
            }

            if (!itf2.mif_idx) {
                memcpy(&itf2, &itf, sizeof(itf));
                continue;
            }

            n = os_mbuf_dup(m);
            if (!n) {
                STATS_INC(oc_ip_stats, oerr);
                break;
            }
            to.msin6_scope_id = itf2.mif_idx;
            rc = mn_sendto(oc_ucast6, n, (struct mn_sockaddr *) &to);
            if (rc != 0) {
                OC_LOG_ERROR("Failed to send buffer %u on itf %d\n",
                             OS_MBUF_PKTHDR(m)->omp_len, to.msin6_scope_id);
                STATS_INC(oc_ip_stats, oerr);
                os_mbuf_free_chain(n);
            }
            memcpy(&itf2, &itf, sizeof(itf));
        }
        if (itf2.mif_idx) {
            to.msin6_scope_id = itf2.mif_idx;
            rc = mn_sendto(oc_ucast6, m, (struct mn_sockaddr *) &to);
            if (rc != 0) {
                OC_LOG_ERROR("Failed sending buffer %u on itf %d\n",
                             OS_MBUF_PKTHDR(m)->omp_len, to.msin6_scope_id);
                STATS_INC(oc_ip_stats, oerr);
                os_mbuf_free_chain(m);
            }
        } else {
            os_mbuf_free_chain(m);
        }
    } else {
        rc = mn_sendto(oc_ucast6, m, (struct mn_sockaddr *) &to);
        if (rc != 0) {
            OC_LOG_ERROR("Failed to send buffer %u on itf %d\n",
                         OS_MBUF_PKTHDR(m)->omp_len, to.msin6_scope_id);
            STATS_INC(oc_ip_stats, oerr);
            os_mbuf_free_chain(m);
        }
    }
}

static void
oc_send_buffer_ip6(struct os_mbuf *m)
{
    STATS_INC(oc_ip_stats, oucast);
    oc_send_buffer_ip6_int(m, 0);
}

static void
oc_send_buffer_ip6_mcast(struct os_mbuf *m)
{
    STATS_INC(oc_ip_stats, omcast);
    oc_send_buffer_ip6_int(m, 1);
}

static struct os_mbuf *
oc_attempt_rx_ip6_sock(struct mn_socket *rxsock)
{
    int rc;
    struct os_mbuf *m;
    struct os_mbuf *n;
    struct oc_endpoint_ip *oe_ip;
    struct mn_sockaddr_in6 from;

    rc = mn_recvfrom(rxsock, &n, (struct mn_sockaddr *) &from);
    if (rc != 0) {
        return NULL;
    }
    assert(OS_MBUF_IS_PKTHDR(n));

    STATS_INC(oc_ip_stats, iframe);
    STATS_INCN(oc_ip_stats, ibytes, OS_MBUF_PKTLEN(n));
    m = os_msys_get_pkthdr(0, sizeof(struct oc_endpoint_ip));
    if (!m) {
        OC_LOG_ERROR("Could not allocate RX buffer\n");
        goto rx_attempt_err;
    }
    OS_MBUF_PKTHDR(m)->omp_len = OS_MBUF_PKTHDR(n)->omp_len;
    SLIST_NEXT(m, om_next) = n;

    oe_ip = (struct oc_endpoint_ip *)OC_MBUF_ENDPOINT(m);

    oe_ip->ep.oe_type = oc_ip6_transport_id;
    oe_ip->ep.oe_flags = 0;
    memcpy(&oe_ip->v6.address, &from.msin6_addr, sizeof(oe_ip->v6.address));
    oe_ip->v6.scope = from.msin6_scope_id;
    oe_ip->port = ntohs(from.msin6_port);

    return m;

    /* add the addr info to the message */
rx_attempt_err:
    STATS_INC(oc_ip_stats, ierr);
    os_mbuf_free_chain(n);
    return NULL;
}

static struct os_mbuf *
oc_attempt_rx_ip6(void)
{
    struct os_mbuf *m;

    m = oc_attempt_rx_ip6_sock(oc_ucast6);
#if (MYNEWT_VAL(OC_SERVER) == 1)
    if (m == NULL) {
        m = oc_attempt_rx_ip6_sock(oc_mcast6);
    }
#endif
    return m;
}

static void oc_socks6_readable(void *cb_arg, int err);

union mn_socket_cb oc_sock6_cbs = {
    .socket.readable = oc_socks6_readable,
    .socket.writable = NULL
};

void
oc_socks6_readable(void *cb_arg, int err)
{
    os_eventq_put(oc_evq_get(), &oc_sock6_read_event);
}

void
oc_connectivity_shutdown_ip6(void)
{
    if (oc_ucast6) {
        mn_close(oc_ucast6);
    }

#if (MYNEWT_VAL(OC_SERVER) == 1)
    if (oc_mcast6) {
        mn_close(oc_mcast6);
    }
#endif

}

static void
oc_event_ip6(struct os_event *ev)
{
    struct os_mbuf *m;

    while ((m = oc_attempt_rx_ip6()) != NULL) {
        oc_recv_message(m);
    }
}

int
oc_connectivity_init_ip6(void)
{
    int rc;
    struct mn_sockaddr_in6 sin;
    struct mn_itf itf;

    memset(&itf, 0, sizeof(itf));

    rc = mn_socket(&oc_ucast6, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    if (rc != 0 || !oc_ucast6) {
        OC_LOG_ERROR("Could not create oc unicast socket\n");
        return rc;
    }
    mn_socket_set_cbs(oc_ucast6, oc_ucast6, &oc_sock6_cbs);

#if (MYNEWT_VAL(OC_SERVER) == 1)
    rc = mn_socket(&oc_mcast6, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    if (rc != 0 || !oc_mcast6) {
        mn_close(oc_ucast6);
        OC_LOG_ERROR("Could not create oc multicast socket\n");
        return rc;
    }
    mn_socket_set_cbs(oc_mcast6, oc_mcast6, &oc_sock6_cbs);
#endif

    sin.msin6_len = sizeof(sin);
    sin.msin6_family = MN_AF_INET6;
    sin.msin6_port = 0;
    sin.msin6_flowinfo = 0;
    sin.msin6_scope_id = 0;
    memcpy(&sin.msin6_addr, nm_in6addr_any, sizeof(sin.msin6_addr));

    rc = mn_bind(oc_ucast6, (struct mn_sockaddr *)&sin);
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

        if ((itf.mif_flags & (MN_ITF_F_UP | MN_ITF_F_MULTICAST)) !=
          (MN_ITF_F_UP | MN_ITF_F_MULTICAST)) {
            continue;
        }

        join.mm_addr.v6 = coap_all_nodes_v6;
        join.mm_idx = itf.mif_idx;
        join.mm_family = MN_AF_INET6;

        rc = mn_setsockopt(oc_mcast6, MN_SO_LEVEL, MN_MCAST_JOIN_GROUP, &join);
        if (rc != 0) {
            continue;
        }

        OC_LOG_DEBUG("Joined Coap mcast group on %s\n", itf.mif_name);
    }

    sin.msin6_port = htons(COAP_PORT_UNSECURED);
    rc = mn_bind(oc_mcast6, (struct mn_sockaddr *)&sin);
    if (rc != 0) {
        OC_LOG_ERROR("Could not bind oc multicast socket\n");
        goto oc_connectivity_init_err;
    }
#endif

    return 0;

oc_connectivity_init_err:
    oc_connectivity_shutdown_ip6();
    return rc;
}

#endif

uint8_t oc_ip6_transport_id = -1;

void
oc_register_ip6(void)
{
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV6) == 1)
    oc_ip6_transport_id = oc_transport_register(&oc_ip6_transport);
#endif
}

