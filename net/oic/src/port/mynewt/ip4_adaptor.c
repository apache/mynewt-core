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
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV4) == 1)
#include <assert.h>
#include <string.h>

#include <os/os.h>
#include <os/endian.h>

#include <log/log.h>
#include <mn_socket/mn_socket.h>
#include <stats/stats.h>

#include "port/oc_connectivity.h"
#include "oic/oc_log.h"
#include "api/oc_buffer.h"
#include "adaptor.h"

static void oc_event_ip4(struct os_event *ev);

static struct os_event oc_sock4_read_event = {
    .ev_cb = oc_event_ip4,
};

#ifdef OC_SECURITY
#error This implementation does not yet support security
#endif

#define COAP_PORT_UNSECURED (5683)

/* 224.0.1.187 */
static const struct mn_in_addr coap_all_nodes_v4 = {
    .s_addr = htonl(0xe00001bb)
};

STATS_SECT_START(oc_ip4_stats)
    STATS_SECT_ENTRY(iframe)
    STATS_SECT_ENTRY(ibytes)
    STATS_SECT_ENTRY(ierr)
    STATS_SECT_ENTRY(oucast)
    STATS_SECT_ENTRY(omcast)
    STATS_SECT_ENTRY(obytes)
    STATS_SECT_ENTRY(oerr)
STATS_SECT_END
STATS_SECT_DECL(oc_ip4_stats) oc_ip4_stats;
STATS_NAME_START(oc_ip4_stats)
    STATS_NAME(oc_ip4_stats, iframe)
    STATS_NAME(oc_ip4_stats, ibytes)
    STATS_NAME(oc_ip4_stats, ierr)
    STATS_NAME(oc_ip4_stats, oucast)
    STATS_NAME(oc_ip4_stats, omcast)
    STATS_NAME(oc_ip4_stats, obytes)
    STATS_NAME(oc_ip4_stats, oerr)
STATS_NAME_END(oc_ip4_stats)

/* sockets to use for coap unicast and multicast */
static struct mn_socket *oc_ucast4;

#if (MYNEWT_VAL(OC_SERVER) == 1)
static struct mn_socket *oc_mcast4;
#endif

static void
oc_send_buffer_ip4_int(struct os_mbuf *m, int is_mcast)
{
    struct mn_sockaddr_in to;
    struct oc_endpoint *oe;
    struct mn_itf itf;
    uint32_t if2_idx;
    struct os_mbuf *n;
    int rc;

    assert(OS_MBUF_USRHDR_LEN(m) >= sizeof(struct oc_endpoint_ip));
    oe = OC_MBUF_ENDPOINT(m);
    if ((oe->oe_ip.flags & IP4) == 0) {
        os_mbuf_free_chain(m);
        return;
    }
    to.msin_len = sizeof(to);
    to.msin_family = MN_AF_INET;
    to.msin_port = htons(oe->oe_ip.v4.port);
    memcpy(&to.msin_addr, oe->oe_ip.v4.address, sizeof(to.msin_addr));

    STATS_INCN(oc_ip4_stats, obytes, OS_MBUF_PKTLEN(m));
    if (is_mcast) {
        memset(&itf, 0, sizeof(itf));
        if2_idx = 0;

        while (1) {
            rc = mn_itf_getnext(&itf);
            if (rc) {
                break;
            }

            if ((itf.mif_flags & (MN_ITF_F_UP | MN_ITF_F_MULTICAST)) !=
              (MN_ITF_F_UP | MN_ITF_F_MULTICAST)) {
                continue;
            }

            if (!if2_idx) {
                if2_idx = itf.mif_idx;
                continue;
            }

            rc = mn_setsockopt(oc_ucast4, MN_SO_LEVEL, MN_MCAST_IF, &if2_idx);
            if (rc) {
                STATS_INC(oc_ip4_stats, oerr);
                continue;
            }
            n = os_mbuf_dup(m);
            if (!n) {
                STATS_INC(oc_ip4_stats, oerr);
                break;
            }
            rc = mn_sendto(oc_ucast4, n, (struct mn_sockaddr *)&to);
            if (rc != 0) {
                OC_LOG_ERROR("Failed to send buffer %u on %x\n",
                             OS_MBUF_PKTHDR(m)->omp_len, if2_idx);
                STATS_INC(oc_ip4_stats, oerr);
                os_mbuf_free_chain(n);
            }
            if2_idx = itf.mif_idx;
        }
        if (if2_idx) {
            rc = mn_setsockopt(oc_ucast4, MN_SO_LEVEL, MN_MCAST_IF, &if2_idx);
            if (rc) {
                STATS_INC(oc_ip4_stats, oerr);
                os_mbuf_free_chain(m);
            } else {
                rc = mn_sendto(oc_ucast4, m, (struct mn_sockaddr *) &to);
                if (rc != 0) {
                    OC_LOG_ERROR("Failed sending buffer %u on itf %x\n",
                                 OS_MBUF_PKTHDR(m)->omp_len, if2_idx);
                    STATS_INC(oc_ip4_stats, oerr);
                    os_mbuf_free_chain(m);
                }
            }
        } else {
            os_mbuf_free_chain(m);
        }
    } else {
        rc = mn_sendto(oc_ucast4, m, (struct mn_sockaddr *) &to);
        if (rc != 0) {
            OC_LOG_ERROR("Failed to send buffer %u ucast\n",
                         OS_MBUF_PKTHDR(m)->omp_len);
            STATS_INC(oc_ip4_stats, oerr);
            os_mbuf_free_chain(m);
        }
    }
}

void
oc_send_buffer_ip4(struct os_mbuf *m)
{
    STATS_INC(oc_ip4_stats, oucast);
    oc_send_buffer_ip4_int(m, 0);
}
void
oc_send_buffer_ip4_mcast(struct os_mbuf *m)
{
    STATS_INC(oc_ip4_stats, omcast);
    oc_send_buffer_ip4_int(m, 1);
}

static struct os_mbuf *
oc_attempt_rx_ip4_sock(struct mn_socket *rxsock)
{
    int rc;
    struct os_mbuf *m;
    struct os_mbuf *n;
    struct oc_endpoint *oe;
    struct mn_sockaddr_in from;

    rc = mn_recvfrom(rxsock, &n, (struct mn_sockaddr *) &from);
    if (rc != 0) {
        return NULL;
    }
    assert(OS_MBUF_IS_PKTHDR(n));

    STATS_INC(oc_ip4_stats, iframe);
    STATS_INCN(oc_ip4_stats, ibytes, OS_MBUF_PKTLEN(n));
    m = os_msys_get_pkthdr(0, sizeof(struct oc_endpoint_ip));
    if (!m) {
        OC_LOG_ERROR("Could not allocate RX buffer\n");
        goto rx_attempt_err;
    }
    OS_MBUF_PKTHDR(m)->omp_len = OS_MBUF_PKTHDR(n)->omp_len;
    SLIST_NEXT(m, om_next) = n;

    oe = OC_MBUF_ENDPOINT(m);

    oe->oe_ip.flags = IP4;
    memcpy(&oe->oe_ip.v4.address, &from.msin_addr,
           sizeof(oe->oe_ip.v4.address));
    oe->oe_ip.v4.port = ntohs(from.msin_port);

    return m;

    /* add the addr info to the message */
rx_attempt_err:
    STATS_INC(oc_ip4_stats, ierr);
    os_mbuf_free_chain(n);
    return NULL;
}

static struct os_mbuf *
oc_attempt_rx_ip4(void)
{
    struct os_mbuf *m;

    m = oc_attempt_rx_ip4_sock(oc_ucast4);
#if (MYNEWT_VAL(OC_SERVER) == 1)
    if (m == NULL) {
        m = oc_attempt_rx_ip4_sock(oc_mcast4);
    }
#endif
    return m;
}

static void oc_socks4_readable(void *cb_arg, int err);

union mn_socket_cb oc_sock4_cbs = {
    .socket.readable = oc_socks4_readable,
    .socket.writable = NULL
};

void
oc_socks4_readable(void *cb_arg, int err)
{
    os_eventq_put(oc_evq_get(), &oc_sock4_read_event);
}

void
oc_connectivity_shutdown_ip4(void)
{
    if (oc_ucast4) {
        mn_close(oc_ucast4);
    }

#if (MYNEWT_VAL(OC_SERVER) == 1)
    if (oc_mcast4) {
        mn_close(oc_mcast4);
    }
#endif
}

static void
oc_event_ip4(struct os_event *ev)
{
    struct os_mbuf *m;

    while ((m = oc_attempt_rx_ip4()) != NULL) {
        oc_recv_message(m);
    }
}

int
oc_connectivity_init_ip4(void)
{
    int rc;
    struct mn_sockaddr_in sin;
    struct mn_itf itf;

    memset(&itf, 0, sizeof(itf));

    rc = mn_socket(&oc_ucast4, MN_PF_INET, MN_SOCK_DGRAM, 0);
    if (rc != 0 || !oc_ucast4) {
        OC_LOG_ERROR("Could not create oc unicast v4 socket\n");
        return rc;
    }
    mn_socket_set_cbs(oc_ucast4, oc_ucast4, &oc_sock4_cbs);

#if (MYNEWT_VAL(OC_SERVER) == 1)
    rc = mn_socket(&oc_mcast4, MN_PF_INET, MN_SOCK_DGRAM, 0);
    if (rc != 0 || !oc_mcast4) {
        mn_close(oc_ucast4);
        OC_LOG_ERROR("Could not create oc multicast v4 socket\n");
        return rc;
    }
    mn_socket_set_cbs(oc_mcast4, oc_mcast4, &oc_sock4_cbs);
#endif

    sin.msin_len = sizeof(sin);
    sin.msin_family = MN_AF_INET;
    sin.msin_port = 11111;
    memset(&sin.msin_addr, 0, sizeof(sin.msin_addr));

    rc = mn_bind(oc_ucast4, (struct mn_sockaddr *)&sin);
    if (rc != 0) {
        OC_LOG_ERROR("Could not bind oc unicast v4 socket\n");
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

        memcpy(&join.mm_addr.v4, &coap_all_nodes_v4, sizeof(coap_all_nodes_v4));
        join.mm_idx = itf.mif_idx;
        join.mm_family = MN_AF_INET;

        rc = mn_setsockopt(oc_mcast4, MN_SO_LEVEL, MN_MCAST_JOIN_GROUP, &join);
        if (rc != 0) {
            continue;
        }

        OC_LOG_DEBUG("Joined Coap v4 mcast group on %s\n", itf.mif_name);
    }

    sin.msin_port = htons(COAP_PORT_UNSECURED);
    rc = mn_bind(oc_mcast4, (struct mn_sockaddr *)&sin);
    if (rc != 0) {
        OC_LOG_ERROR("Could not bind oc v4 multicast socket\n");
        goto oc_connectivity_init_err;
    }
#endif

    return 0;

oc_connectivity_init_err:
    oc_connectivity_shutdown_ip4();
    return rc;
}

#endif
