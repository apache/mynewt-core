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
#include <string.h>

#include "os/mynewt.h"

#include <mn_socket/mn_socket.h>
#include <mn_socket/mn_socket_ops.h>

#include <lwip/tcpip.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>
#include <lwip/igmp.h>
#include <lwip/mld6.h>
#include "ip_priv.h"

static int lwip_sock_create(struct mn_socket **sp, uint8_t domain,
  uint8_t type, uint8_t proto);
static int lwip_close(struct mn_socket *);
static int lwip_connect(struct mn_socket *, struct mn_sockaddr *);
static int lwip_bind(struct mn_socket *, struct mn_sockaddr *);
static int lwip_listen(struct mn_socket *, uint8_t qlen);
static int lwip_sendto(struct mn_socket *, struct os_mbuf *,
  struct mn_sockaddr *);
static int lwip_recvfrom(struct mn_socket *, struct os_mbuf **,
  struct mn_sockaddr *);
static int lwip_getsockopt(struct mn_socket *, uint8_t level,
  uint8_t name, void *val);
static int lwip_setsockopt(struct mn_socket *, uint8_t level,
  uint8_t name, void *val);
static int lwip_getsockname(struct mn_socket *, struct mn_sockaddr *);
static int lwip_getpeername(struct mn_socket *, struct mn_sockaddr *);

static const struct mn_socket_ops lwip_sock_ops = {
    .mso_create = lwip_sock_create,
    .mso_close = lwip_close,

    .mso_bind = lwip_bind,
    .mso_connect = lwip_connect,
    .mso_listen = lwip_listen,

    .mso_sendto = lwip_sendto,
    .mso_recvfrom = lwip_recvfrom,

    .mso_getsockopt = lwip_getsockopt,
    .mso_setsockopt = lwip_setsockopt,

    .mso_getsockname = lwip_getsockname,
    .mso_getpeername = lwip_getpeername,

    .mso_itf_getnext = lwip_itf_getnext,
    .mso_itf_addr_getnext = lwip_itf_addr_getnext,
};

struct lwip_sock {
    struct mn_socket ls_sock;
    uint8_t ls_type;
    union {
        struct ip_pcb *ip;
        struct udp_pcb *udp;
        struct tcp_pcb *tcp;
    } ls_pcb;
    STAILQ_HEAD(, os_mbuf_pkthdr) ls_rx;
    struct os_mbuf *ls_tx;
};

static struct os_mempool lwip_sockets;

static int lwip_stream_tx(struct lwip_sock *s, int notify);

static int
lwip_mn_addr_to_addr(struct mn_sockaddr *ms, ip_addr_t *addr, uint16_t *port)
{
    struct mn_sockaddr_in *msin;
    struct mn_sockaddr_in6 *msin6;

    switch (ms->msa_family) {
#if LWIP_IPV4
    case MN_AF_INET:
        msin = (struct mn_sockaddr_in *)ms;
        IP_SET_TYPE_VAL(*addr, IPADDR_TYPE_V4);
        *port = ntohs(msin->msin_port);
        memcpy(ip_2_ip4(addr), &msin->msin_addr, sizeof(struct mn_in_addr));
        return 0;
#endif
#if LWIP_IPV6
    case MN_AF_INET6:
        msin6 =  (struct mn_sockaddr_in6 *)ms;
        IP_SET_TYPE_VAL(*addr, IPADDR_TYPE_V6);
        *port = ntohs(msin6->msin6_port);
        memcpy(ip_2_ip6(addr), &msin6->msin6_addr, sizeof(struct mn_in6_addr));
        return 0;
#endif
    default:
        return MN_EPROTONOSUPPORT;
    }
}

static void
lwip_addr_to_mn_addr(struct mn_sockaddr *ms, const ip_addr_t *addr,
  uint16_t port)
{
    struct mn_sockaddr_in *msin;
    struct mn_sockaddr_in6 *msin6;

    if (IP_IS_V4_VAL(*addr)) {
        ms->msa_family = MN_AF_INET;
    } else {
        ms->msa_family = MN_AF_INET6;
    }
    port = htons(port);
    switch (ms->msa_family) {
#if LWIP_IPV4
    case MN_AF_INET:
        msin = (struct mn_sockaddr_in *)ms;
        memcpy(&msin->msin_addr, ip_2_ip4(addr), sizeof(struct mn_in_addr));
        msin->msin_port = port;
        break;
#endif
#if LWIP_IPV6
    case MN_AF_INET6:
        msin6 = (struct mn_sockaddr_in6 *)ms;
        memcpy(&msin6->msin6_addr, ip_2_ip6(addr), sizeof(struct mn_in6_addr));
        msin6->msin6_port = port;
        break;
#endif
    default:
        break;
    }
}

int
lwip_err_to_mn_err(int rc)
{
    switch (rc) {
    case ERR_OK:
        return 0;
    case ERR_MEM:
    case ERR_BUF:
        return MN_ENOBUFS;
    case ERR_RTE:
        return MN_ENETUNREACH;
    case ERR_TIMEOUT:
        return MN_ETIMEDOUT;
    case ERR_INPROGRESS:
    case ERR_WOULDBLOCK:
        return MN_EAGAIN;
    case ERR_VAL:
    case ERR_ARG:
        return MN_EINVAL;
    case ERR_USE:
        return MN_EADDRINUSE;
    case ERR_CONN:
    case ERR_CLSD:
        return MN_ENOTCONN;
    case ERR_ABRT:
    case ERR_RST:
        return MN_ECONNABORTED;
    default:
        return MN_EUNKNOWN;
    }
}

#if LWIP_UDP
static void
lwip_sock_udp_rx(void *arg, struct udp_pcb *pcb, struct pbuf *p,
  const ip_addr_t *addr, uint16_t port)
{
    struct lwip_sock *s = (struct lwip_sock *)arg;
    struct os_mbuf *m;
    struct pbuf *q;
    int off;

    m = os_msys_get_pkthdr(p->tot_len, sizeof(struct mn_sockaddr_in6));
    if (!m) {
        pbuf_free(p);
        return;
    }
    lwip_addr_to_mn_addr((struct mn_sockaddr *)OS_MBUF_USRHDR(m),
      addr, port);
    off = 0;
    for (q = p; q; q = q->next) {
        os_mbuf_copyinto(m, off, q->payload, p->len);
    }
    pbuf_free(p);
    STAILQ_INSERT_TAIL(&s->ls_rx, OS_MBUF_PKTHDR(m), omp_next);
    mn_socket_readable(&s->ls_sock, 0);
}
#endif

#if LWIP_TCP
static err_t
lwip_sock_tcp_rx(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    struct lwip_sock *s = (struct lwip_sock *)arg;
    struct os_mbuf *m;
    struct pbuf *q;
    int off;

    if (!p) {
        /*
         * Connection closed.
         */
        mn_socket_readable(&s->ls_sock, MN_ECONNABORTED);
        return ERR_OK;
    }
    m = os_msys_get_pkthdr(p->tot_len, 0);
    assert(m);
    /* XXX fix this */
    off = 0;
    for (q = p; q; q = q->next) {
        os_mbuf_copyinto(m, off, q->payload, p->len);
    }
    pbuf_free(p);
    STAILQ_INSERT_TAIL(&s->ls_rx, OS_MBUF_PKTHDR(m), omp_next);
    mn_socket_readable(&s->ls_sock, 0);

    return ERR_OK;
}

static err_t
lwip_sock_tcp_sent(void *arg, struct tcp_pcb *pcb, uint16_t len)
{
    struct lwip_sock *s = (struct lwip_sock *)arg;

    lwip_stream_tx(s, 1);
    return ERR_OK;
}

static err_t
lwip_sock_tcp_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
    struct lwip_sock *s = (struct lwip_sock *)arg;

    mn_socket_writable(&s->ls_sock, lwip_err_to_mn_err(err));
    return ERR_OK;
}

static void
lwip_sock_tcp_err(void *arg, err_t err)
{
    struct lwip_sock *s = (struct lwip_sock *)arg;

    mn_socket_writable(&s->ls_sock, lwip_err_to_mn_err(err));
}

static err_t
lwip_sock_accept(void *arg, struct tcp_pcb *new, err_t err)
{
    struct lwip_sock *s = (struct lwip_sock *)arg;
    struct lwip_sock *new_s;

    if (err != ERR_OK) {
        return err;
    }
    new_s = os_memblock_get(&lwip_sockets);
    if (!new_s) {
        return ERR_MEM;
    }
    new_s->ls_type = MN_SOCK_STREAM;
    new_s->ls_sock.ms_ops = &lwip_sock_ops;
    new_s->ls_pcb.tcp = new;
    tcp_arg(new, new_s);
    tcp_recv(new, lwip_sock_tcp_rx);
    tcp_sent(new, lwip_sock_tcp_sent);
    tcp_err(new, lwip_sock_tcp_err);
    STAILQ_INIT(&new_s->ls_rx);
    new_s->ls_tx = NULL;
    if (mn_socket_newconn(&s->ls_sock, &new_s->ls_sock)) {
        /* XXX close connection */
    }
    return ERR_OK;
}
#endif

static int
lwip_sock_create(struct mn_socket **sp, uint8_t domain, uint8_t type,
  uint8_t proto)
{
    struct lwip_sock *s;

    s = os_memblock_get(&lwip_sockets);
    if (!s) {
        return MN_ENOBUFS;
    }
    s->ls_type = type;
    s->ls_pcb.ip = NULL;
    STAILQ_INIT(&s->ls_rx);
    s->ls_tx = NULL;

    LOCK_TCPIP_CORE();
    switch (type) {
#if LWIP_UDP
    case MN_SOCK_DGRAM:
        s->ls_pcb.udp = udp_new();
        udp_recv(s->ls_pcb.udp, lwip_sock_udp_rx, s);
        break;
#endif
#if LWIP_TCP
    case MN_SOCK_STREAM:
        s->ls_pcb.tcp = tcp_new();
        tcp_arg(s->ls_pcb.tcp, s);
        tcp_recv(s->ls_pcb.tcp, lwip_sock_tcp_rx);
        tcp_sent(s->ls_pcb.tcp, lwip_sock_tcp_sent);
        tcp_err(s->ls_pcb.tcp, lwip_sock_tcp_err);
        break;
#endif
    default:
        break;
    }
    UNLOCK_TCPIP_CORE();
    if (!s->ls_pcb.ip) {
        os_memblock_put(&lwip_sockets, s);
        return MN_EPROTONOSUPPORT;
    } else {
        *sp = &s->ls_sock;
        return 0;
    }
}

static int
lwip_close(struct mn_socket *ms)
{
    struct lwip_sock *s = (struct lwip_sock *)ms;
    struct os_mbuf_pkthdr *m;

    LOCK_TCPIP_CORE();
    switch (s->ls_type) {
#if LWIP_UDP
    case MN_SOCK_DGRAM:
        udp_remove(s->ls_pcb.udp);
        break;
#endif
#if LWIP_TCP
    case MN_SOCK_STREAM:
        tcp_recv(s->ls_pcb.tcp, NULL);
        tcp_sent(s->ls_pcb.tcp, NULL);
        tcp_err(s->ls_pcb.tcp, NULL);
        tcp_close(s->ls_pcb.tcp);
        break;
    }
#endif
    UNLOCK_TCPIP_CORE();
    while ((m = STAILQ_FIRST(&s->ls_rx))) {
        STAILQ_REMOVE_HEAD(&s->ls_rx, omp_next);
        os_mbuf_free_chain(OS_MBUF_PKTHDR_TO_MBUF(m));
    }
    if (s->ls_tx) {
        os_mbuf_free_chain(s->ls_tx);
        s->ls_tx = NULL;
    }
    os_memblock_put(&lwip_sockets, s);
    return 0;
}

static int
lwip_connect(struct mn_socket *ms, struct mn_sockaddr *addr)
{
    struct lwip_sock *s = (struct lwip_sock *)ms;
    int rc;
    ip_addr_t ip;
    uint16_t port;

    rc = lwip_mn_addr_to_addr(addr, &ip, &port);
    if (rc) {
        return rc;
    }
    rc = MN_EPROTONOSUPPORT;
    LOCK_TCPIP_CORE();
    switch (s->ls_type) {
#if LWIP_UDP
    case MN_SOCK_DGRAM:
        rc = udp_connect(s->ls_pcb.udp, &ip, port);
        rc = lwip_err_to_mn_err(rc);
        break;
#endif
#if LWIP_TCP
    case MN_SOCK_STREAM:
        rc = tcp_connect(s->ls_pcb.tcp, &ip, port, lwip_sock_tcp_connected);
        rc = lwip_err_to_mn_err(rc);
        break;
#endif
    default:
        break;
    }
    UNLOCK_TCPIP_CORE();
    return rc;
}

static int
lwip_bind(struct mn_socket *ms, struct mn_sockaddr *addr)
{
    struct lwip_sock *s = (struct lwip_sock *)ms;
    int rc;
    ip_addr_t ip;
    uint16_t port;

    rc = lwip_mn_addr_to_addr(addr, &ip, &port);
    if (rc) {
        return rc;
    }
    rc = MN_EPROTONOSUPPORT;
    LOCK_TCPIP_CORE();
    switch (s->ls_type) {
#if LWIP_UDP
    case MN_SOCK_DGRAM:
        rc = udp_bind(s->ls_pcb.udp, &ip, port);
        rc = lwip_err_to_mn_err(rc);
        break;
#endif
#if LWIP_TCP
    case MN_SOCK_STREAM:
        rc = tcp_bind(s->ls_pcb.tcp, &ip, port);
        rc = lwip_err_to_mn_err(rc);
        break;
#endif
    default:
        break;
    }
    UNLOCK_TCPIP_CORE();
    return rc;
}

static int
lwip_listen(struct mn_socket *ms, uint8_t qlen)
{
#if LWIP_TCP
    struct lwip_sock *s = (struct lwip_sock *)ms;
    struct tcp_pcb *pcb;

    if (s->ls_type == MN_SOCK_STREAM) {
        LOCK_TCPIP_CORE();
        pcb = tcp_listen_with_backlog(s->ls_pcb.tcp, qlen);
        UNLOCK_TCPIP_CORE();
        if (!pcb) {
            return MN_EADDRINUSE;
        }
        s->ls_pcb.tcp = pcb;
        tcp_accept(pcb, lwip_sock_accept);
        return 0;
    }
#endif
    return MN_EINVAL;
}

static int
lwip_stream_tx(struct lwip_sock *s, int notify)
{
    int rc;
    struct os_mbuf *m;
    struct os_mbuf *n;

    rc = 0;
    while (s->ls_tx && rc == 0) {
        m = s->ls_tx;
        n = SLIST_NEXT(m, om_next);
        rc = tcp_write(s->ls_pcb.tcp, m->om_data, m->om_len, 0);
        if (rc == 0) {
            s->ls_tx = n;
            os_mbuf_free(m);
        }
    }
    if (rc) {
        if (rc == ERR_MEM) {
            rc = 0;
        } else {
            rc = lwip_err_to_mn_err(rc);
        }
    }
    if (notify) {
        if (s->ls_tx == NULL) {
            mn_socket_writable(&s->ls_sock, 0);
        } else if (rc) {
            mn_socket_writable(&s->ls_sock, rc);
        }
    }
    return rc;
}

static int
lwip_sendto(struct mn_socket *ms, struct os_mbuf *m,
  struct mn_sockaddr *addr)
{
    struct lwip_sock *s = (struct lwip_sock *)ms;
    struct pbuf *p;
    struct os_mbuf *n;
    ip_addr_t ip_addr;
    uint16_t port;
    int off;
    int rc;

    switch (s->ls_type) {
#if LWIP_UDP
    case MN_SOCK_DGRAM:
        if (!addr) {
            return MN_EDESTADDRREQ;
        }
        rc = lwip_mn_addr_to_addr(addr, &ip_addr, &port);
        if (rc) {
            return rc;
        }
        off = 0;
        for (n = m; n; n = SLIST_NEXT(n, om_next)) {
            off += n->om_len;
        }
        p = pbuf_alloc(PBUF_TRANSPORT, off, PBUF_RAM);
        if (!p) {
            return MN_ENOBUFS;
        }

        off = 0;
        for (n = m; n; n = SLIST_NEXT(n, om_next)) {
            pbuf_take_at(p, n->om_data, n->om_len, off);
            off += n->om_len;
        }
        LOCK_TCPIP_CORE();
        rc = udp_sendto(s->ls_pcb.udp, p, &ip_addr, port);
        UNLOCK_TCPIP_CORE();
        if (rc) {
            rc = lwip_err_to_mn_err(rc);
            pbuf_free(p);
            return rc;
        }
        os_mbuf_free_chain(m);
        return 0;
#endif
#if LWIP_TCP
    case MN_SOCK_STREAM:
        if (s->ls_tx) {
            return MN_EAGAIN;
        }
        if (addr) {
            return MN_EINVAL;
        }
        LOCK_TCPIP_CORE();
        s->ls_tx = m;
        rc = lwip_stream_tx(s, 0);
        UNLOCK_TCPIP_CORE();
        return rc;
#endif
    default:
        return MN_EPROTONOSUPPORT;
    }
}

static int
lwip_recvfrom(struct mn_socket *ms, struct os_mbuf **mp,
  struct mn_sockaddr *addr)
{
    struct lwip_sock *s = (struct lwip_sock *)ms;
    struct mn_sockaddr *ms_a;
    struct os_mbuf_pkthdr *m;
    int slen;

    LOCK_TCPIP_CORE();
    m = STAILQ_FIRST(&s->ls_rx);
    if (m) {
        STAILQ_REMOVE_HEAD(&s->ls_rx, omp_next);
        STAILQ_NEXT(m, omp_next) = NULL;
    }
    if (m) {
        *mp = OS_MBUF_PKTHDR_TO_MBUF(m);
        if (addr) {
            if (s->ls_type == MN_SOCK_DGRAM) {
                ms_a = (struct mn_sockaddr *)(m + 1);
                if (ms_a->msa_family == MN_AF_INET6) {
                    slen = sizeof(struct mn_sockaddr_in6);
                } else {
                    slen = sizeof(struct mn_sockaddr_in);
                }
                memcpy(addr, ms_a, slen);
            } else {
#if LWIP_TCP
                lwip_addr_to_mn_addr(addr, &s->ls_pcb.ip->local_ip,
                                     s->ls_pcb.tcp->local_port);
#endif
            }
        }
        UNLOCK_TCPIP_CORE();
        return 0;
    } else {
        UNLOCK_TCPIP_CORE();
        *mp = NULL;
        return MN_EAGAIN;
    }
}

static int
lwip_getsockopt(struct mn_socket *s, uint8_t level,
  uint8_t name, void *val)
{
    return MN_EPROTONOSUPPORT;
}

static struct netif *
lwip_nif_from_idx(int idx)
{
    struct netif *nif;

    for (nif = netif_list; nif; nif = nif->next) {
        if (idx == nif->num) {
            return nif;
        }
    }
    return NULL;
}

static int
lwip_setsockopt(struct mn_socket *ms, uint8_t level, uint8_t name, void *val)
{
    struct netif *nif;
    struct mn_mreq *mreq;
    int rc = MN_EPROTONOSUPPORT;

    if (level == MN_SO_LEVEL) {
        switch (name) {
        case MN_MCAST_JOIN_GROUP:
        case MN_MCAST_LEAVE_GROUP:
            mreq = (struct mn_mreq *)val;

            LOCK_TCPIP_CORE();
            nif = lwip_nif_from_idx(mreq->mm_idx);
            if (mreq->mm_family == MN_AF_INET) {
#if LWIP_IGMP
                if (name == MN_MCAST_JOIN_GROUP) {
                    rc = igmp_joingroup_netif(nif,
                                              (ip4_addr_t *)&mreq->mm_addr);
                } else {
                    rc = igmp_leavegroup_netif(nif,
                                              (ip4_addr_t *)&mreq->mm_addr);
                }
#endif
            } else if (mreq->mm_family == MN_AF_INET6) {
#if LWIP_IPV6_MLD && LWIP_IPV6
                if (name == MN_MCAST_JOIN_GROUP) {
                    rc = mld6_joingroup_netif(nif,
                                              (ip6_addr_t *)&mreq->mm_addr);
                } else {
                    rc = mld6_leavegroup_netif(nif,
                                              (ip6_addr_t *)&mreq->mm_addr);
                }
#endif
            }
            UNLOCK_TCPIP_CORE();
            return lwip_err_to_mn_err(rc);
        case MN_MCAST_IF:
        default:
            break;
        }
    }
    return MN_EPROTONOSUPPORT;
}

static int
lwip_getsockname(struct mn_socket *ms, struct mn_sockaddr *addr)
{
    struct lwip_sock *s = (struct lwip_sock *)ms;
    int rc;

    rc = MN_EPROTONOSUPPORT;
    LOCK_TCPIP_CORE();
    switch (s->ls_type) {
#if LWIP_UDP
    case MN_SOCK_DGRAM:
        lwip_addr_to_mn_addr(addr, &s->ls_pcb.ip->local_ip,
                             s->ls_pcb.udp->local_port);
        rc = 0;
#endif
#if LWIP_TCP
    case MN_SOCK_STREAM:
        lwip_addr_to_mn_addr(addr, &s->ls_pcb.ip->local_ip,
                             s->ls_pcb.tcp->local_port);
        rc = 0;
#endif
    default:
        break;
    }
    UNLOCK_TCPIP_CORE();
    return rc;
}

static int
lwip_getpeername(struct mn_socket *ms, struct mn_sockaddr *addr)
{
    struct lwip_sock *s = (struct lwip_sock *)ms;
    int rc;

    rc = MN_EPROTONOSUPPORT;
    LOCK_TCPIP_CORE();
    switch (s->ls_type) {
#if LWIP_UDP
    case MN_SOCK_DGRAM:
        lwip_addr_to_mn_addr(addr, &s->ls_pcb.ip->remote_ip,
                             s->ls_pcb.udp->remote_port);
        rc = 0;
#endif
#if LWIP_TCP
    case MN_SOCK_STREAM:
        lwip_addr_to_mn_addr(addr, &s->ls_pcb.ip->remote_ip,
                             s->ls_pcb.tcp->remote_port);
        rc = 0;
#endif
    default:
        break;
    }
    UNLOCK_TCPIP_CORE();
    return rc;
}

int
lwip_socket_init(void)
{
    int rc;
    int cnt;
    void *mem;

    cnt = MEMP_NUM_TCP_PCB + MEMP_NUM_UDP_PCB + MEMP_NUM_TCP_PCB_LISTEN;
    mem = os_malloc(OS_MEMPOOL_BYTES(cnt, sizeof(struct lwip_sock)));
    if (!mem) {
        return -1;
    }
    os_mempool_init(&lwip_sockets, cnt, sizeof(struct lwip_sock), mem, "sock");

    rc = mn_socket_ops_reg(&lwip_sock_ops);
    if (rc) {
        return -1;
    }
    return 0;
}
