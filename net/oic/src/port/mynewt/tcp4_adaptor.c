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

uint8_t oc_tcp4_transport_id = UINT8_MAX;

#if MYNEWT_VAL(OC_TRANSPORT_TCP4)

#include <log/log.h>
#include <mn_socket/mn_socket.h>
#include <stats/stats.h>

#include "oic/oc_log.h"
#include "oic/port/oc_connectivity.h"
#include "oic/port/mynewt/adaptor.h"
#include "oic/port/mynewt/transport.h"
#include "oic/port/mynewt/ip.h"
#include "oic/port/mynewt/stream.h"
#include "oic/port/mynewt/tcp4.h"

#ifdef OC_SECURITY
#error This implementation does not yet support security
#endif

static void oc_send_buffer_tcp4(struct os_mbuf *m);
static uint8_t oc_ep_tcp4_size(const struct oc_endpoint *oe);
static char *oc_log_ep_tcp4(char *ptr, int maxlen, const struct oc_endpoint *);
static int oc_connectivity_init_tcp4(void);
static void oc_connectivity_shutdown_tcp4(void);
static void oc_event_tcp4(struct os_event *ev);
static void oc_tcp4_readable(void *cb_arg, int err);
static void oc_tcp4_writable(void *cb_arg, int err);
static struct oc_tcp4_conn *oc_tcp4_remove_conn(struct mn_socket *sock);
static bool oc_tcp4_ep_match(const void *ep, const void *ep_desc);
static void oc_tcp4_ep_fill(void *ep, const void *ep_desc);

static const struct oc_transport oc_tcp4_transport = {
    .ot_flags = OC_TRANSPORT_USE_TCP,
    .ot_ep_size = oc_ep_tcp4_size,
    .ot_tx_ucast = oc_send_buffer_tcp4,
    .ot_tx_mcast = NULL,
    .ot_get_trans_security = NULL,
    .ot_ep_str = oc_log_ep_tcp4,
    .ot_init = oc_connectivity_init_tcp4,
    .ot_shutdown = oc_connectivity_shutdown_tcp4
};

static struct os_event oc_tcp4_read_event = {
    .ev_cb = oc_event_tcp4,
};

#define COAP_PORT_UNSECURED (5683)

STATS_SECT_START(oc_tcp4_stats)
    STATS_SECT_ENTRY(iframe)
    STATS_SECT_ENTRY(ibytes)
    STATS_SECT_ENTRY(ierr)
    STATS_SECT_ENTRY(oucast)
    STATS_SECT_ENTRY(obytes)
    STATS_SECT_ENTRY(oerr)
STATS_SECT_END
STATS_SECT_DECL(oc_tcp4_stats) oc_tcp4_stats;
STATS_NAME_START(oc_tcp4_stats)
    STATS_NAME(oc_tcp4_stats, iframe)
    STATS_NAME(oc_tcp4_stats, ibytes)
    STATS_NAME(oc_tcp4_stats, ierr)
    STATS_NAME(oc_tcp4_stats, oucast)
    STATS_NAME(oc_tcp4_stats, obytes)
    STATS_NAME(oc_tcp4_stats, oerr)
STATS_NAME_END(oc_tcp4_stats)

/**
 * Describes a tcp endpoint.  EP descriptors get passed to callbacks that need
 * to operate on a specific managed connection.
 */
struct oc_tcp4_ep_desc {
    struct mn_socket *sock;
    oc_ipv4_addr_t addr;
    uint16_t port;
};

static struct oc_stream_reassembler oc_tcp4_r = {
    .pkt_q = STAILQ_HEAD_INITIALIZER(oc_tcp4_r.pkt_q),
    .ep_match = oc_tcp4_ep_match,
    .ep_fill = oc_tcp4_ep_fill,
    .endpoint_size = sizeof(struct oc_endpoint_tcp),
};

struct oc_tcp4_conn {
    struct mn_socket *sock;
    oc_tcp4_err_fn *err_cb;
    void *err_arg;
    SLIST_ENTRY(oc_tcp4_conn) next;
};

struct os_mempool oc_tcp4_conn_pool;
static os_membuf_t oc_tcp4_conn_buf[
    OS_MEMPOOL_SIZE(MYNEWT_VAL(OC_TCP4_MAX_CONNS),
                    sizeof(struct oc_tcp4_conn))
];

union mn_socket_cb oc_tcp4_cbs = {
    .socket.readable = oc_tcp4_readable,
    .socket.writable = oc_tcp4_writable,
};

static SLIST_HEAD(, oc_tcp4_conn) oc_tcp4_conn_list;

/* sockets to use for coap unicast and multicast */
static struct mn_socket *oc_ucast4;

static char *
oc_log_ep_tcp4(char *ptr, int maxlen, const struct oc_endpoint *oe)
{
    const struct oc_endpoint_tcp *ep_tcp = (const struct oc_endpoint_tcp *)oe;
    int len;

    mn_inet_ntop(MN_PF_INET, ep_tcp->ep_ip.v4.address, ptr, maxlen);
    len = strlen(ptr);
    snprintf(ptr + len, maxlen - len, "-%u", ep_tcp->ep_ip.port);

    return ptr;
}

static uint8_t
oc_ep_tcp4_size(const struct oc_endpoint *oe)
{
    return sizeof(struct oc_endpoint_tcp);
}

static void
oc_send_buffer_tcp4(struct os_mbuf *m)
{
    struct oc_endpoint_tcp *ep_tcp;
    int rc;

    STATS_INC(oc_tcp4_stats, oucast);

    assert(OS_MBUF_USRHDR_LEN(m) >= sizeof(struct oc_endpoint_tcp));
    ep_tcp = OS_MBUF_USRHDR(m);

    assert(ep_tcp->sock != NULL);

    STATS_INCN(oc_tcp4_stats, obytes, OS_MBUF_PKTLEN(m));

    rc = mn_sendto(ep_tcp->sock, m, NULL);
    if (rc != 0) {
        OC_LOG_ERROR("Failed to send buffer %u ucast\n",
                     OS_MBUF_PKTHDR(m)->omp_len);
        STATS_INC(oc_tcp4_stats, oerr);
    }
}

static bool
oc_tcp4_ep_match(const void *ep, const void *ep_desc)
{
    const struct oc_tcp4_ep_desc *tcp4_desc;
    const struct oc_endpoint_tcp *ep_tcp;

    ep_tcp = ep;
    tcp4_desc = ep_desc;

    return tcp4_desc->sock == ep_tcp->sock;
}

static void
oc_tcp4_ep_fill(void *ep, const void *ep_desc)
{
    const struct oc_tcp4_ep_desc *tcp4_desc;
    struct oc_endpoint_tcp *ep_tcp;

    ep_tcp = ep;
    tcp4_desc = ep_desc;

    ep_tcp->ep_ip.ep.oe_type = oc_tcp4_transport_id;
    ep_tcp->ep_ip.ep.oe_flags = 0;
    ep_tcp->ep_ip.v4 = tcp4_desc->addr;
    ep_tcp->ep_ip.port = tcp4_desc->port;
    ep_tcp->sock = tcp4_desc->sock;
}

int
oc_tcp4_ep_create(struct oc_endpoint_tcp *ep, struct mn_socket *sock)
{
    struct oc_tcp4_ep_desc desc;
    struct mn_sockaddr_in msin;
    int rc;

    rc = mn_getpeername(sock, (struct mn_sockaddr *)&msin);
    if (rc != 0) {
        return rc;
    }

    desc.sock = sock;
    memcpy(&desc.addr, &msin.msin_addr, sizeof desc.addr);
    desc.port = ntohs(msin.msin_port);

    oc_tcp4_ep_fill(ep, &desc);

    return 0;
}

static int
oc_tcp_rx_frag(struct mn_socket *sock, struct os_mbuf *frag,
               const struct mn_sockaddr_in *from)
{
    struct oc_tcp4_ep_desc ep_desc;
    struct os_mbuf *pkt;
    int rc;

    STATS_INC(oc_tcp4_stats, iframe);
    STATS_INCN(oc_tcp4_stats, ibytes, OS_MBUF_PKTLEN(frag));

    ep_desc.sock = sock;
    memcpy(&ep_desc.addr, &from->msin_addr, sizeof ep_desc.addr);
    ep_desc.port = ntohs(from->msin_port);

    rc = oc_stream_reass(&oc_tcp4_r, frag, &ep_desc, &pkt);
    if (rc != 0) {
        if (rc == SYS_ENOMEM) {
            OC_LOG_ERROR("oc_tcp4_rx: Could not allocate mbuf\n");
        }
        STATS_INC(oc_tcp4_stats, ierr);
        return rc;
    }

    if (pkt != NULL) {
        STATS_INC(oc_tcp4_stats, iframe);
        oc_recv_message(pkt);
    }

    return 0;
}

static void
oc_tcp4_err(struct mn_socket *sock, int err)
{
    struct oc_tcp4_conn *conn;

    conn = oc_tcp4_remove_conn(sock);
    assert(conn != NULL);

    if (conn->err_cb != NULL) {
        conn->err_cb(conn->sock, err, conn->err_arg);
    }

    os_memblock_put(&oc_tcp4_conn_pool, conn);
}

static void
oc_tcp4_readable(void *cb_arg, int err)
{
    if (err != 0) {
        oc_tcp4_err(cb_arg, err);
    } else {
        os_eventq_put(oc_evq_get(), &oc_tcp4_read_event);
    }
}

static void
oc_tcp4_writable(void *cb_arg, int err)
{
    if (err != 0) {
        oc_tcp4_err(cb_arg, err);
    }
}

static void
oc_connectivity_shutdown_tcp4(void)
{
    if (oc_ucast4) {
        mn_close(oc_ucast4);
    }
}

/**
 * Receives all buffered packets sent over the given connection.
 */
static int
oc_tcp4_recv_conn(struct oc_tcp4_conn *conn)
{
    struct mn_sockaddr_in from;
    struct os_mbuf *frag;
    int rc;

    /* Keep receiving until there are no more pending packets. */
    while (1) {
        rc = mn_recvfrom(conn->sock, &frag, (struct mn_sockaddr *) &from);
        switch (rc) {
        case 0:
            assert(OS_MBUF_IS_PKTHDR(frag));
            oc_tcp_rx_frag(conn->sock, frag, &from);
            break;

        case MN_EAGAIN:
            // No more packets to receive.
            return 0;

        default:
            return rc;
        }
    }
}

static void
oc_event_tcp4(struct os_event *ev)
{
    struct oc_tcp4_conn *conn;
    int rc;

    SLIST_FOREACH(conn, &oc_tcp4_conn_list, next) {
        rc = oc_tcp4_recv_conn(conn);
        if (rc != 0) {
            /* The connection is in a bad state and needs to be removed from
             * the list.  We can't continue looping after modifying the list,
             * so enqueue "receive" event and return.
             */
            oc_tcp4_err(conn->sock, rc);
            os_eventq_put(oc_evq_get(), &oc_tcp4_read_event);
            return;
        }
    }
}

static int
oc_connectivity_init_tcp4(void)
{
    return 0;
}

static struct oc_tcp4_conn *
oc_tcp4_find_conn(const struct mn_socket *sock, struct oc_tcp4_conn **out_prev)
{
    struct oc_tcp4_conn *prev;
    struct oc_tcp4_conn *conn;

    prev = NULL;

    SLIST_FOREACH(conn, &oc_tcp4_conn_list, next) {
        if (conn->sock == sock) {
            break;
        }

        prev = conn;
    }

    if (out_prev != NULL) {
        *out_prev = prev;
    }

    return conn;
}

int
oc_tcp4_add_conn(struct mn_socket *sock, oc_tcp4_err_fn *on_err, void *arg)
{
    struct oc_tcp4_conn *conn;

    conn = os_memblock_get(&oc_tcp4_conn_pool);
    if (conn == NULL) {
        return SYS_ENOMEM;
    }

    *conn = (struct oc_tcp4_conn) {
        .sock = sock,
        .err_cb = on_err,
        .err_arg = arg,
    };
    mn_socket_set_cbs(conn->sock, conn->sock, &oc_tcp4_cbs);

    SLIST_INSERT_HEAD(&oc_tcp4_conn_list, conn, next);

    return 0;
}

static struct oc_tcp4_conn *
oc_tcp4_remove_conn(struct mn_socket *sock)
{
    struct oc_tcp4_conn *conn;
    struct oc_tcp4_conn *prev;

    conn = oc_tcp4_find_conn(sock, &prev);
    if (conn == NULL) {
        return NULL;
    }

    if (prev == NULL) {
        SLIST_REMOVE_HEAD(&oc_tcp4_conn_list, next);
    } else {
        SLIST_NEXT(prev, next) = SLIST_NEXT(conn, next);
    }

    return conn;
}

int
oc_tcp4_del_conn(struct mn_socket *sock)
{
    struct oc_tcp4_conn *conn;

    conn = oc_tcp4_remove_conn(sock);
    if (conn == NULL) {
        return SYS_ENOENT;
    }

    os_memblock_put(&oc_tcp4_conn_pool, conn);

    return 0;
}

#endif /* MYNEWT_VAL(OC_TRANSPORT_TCP4) */

void
oc_register_tcp4(void)
{
#if MYNEWT_VAL(OC_TRANSPORT_TCP4)
    int rc;

    SLIST_INIT(&oc_tcp4_conn_list);

    rc = os_mempool_init(&oc_tcp4_conn_pool, MYNEWT_VAL(OC_TCP4_MAX_CONNS),
                         sizeof (struct oc_tcp4_conn), oc_tcp4_conn_buf,
                         "oc_tcp4_conn_pool");
    SYSINIT_PANIC_ASSERT(rc == 0);

    oc_tcp4_transport_id = oc_transport_register(&oc_tcp4_transport);
#endif
}
