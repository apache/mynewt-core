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

/*
 * SMP transport over UDP.
 *
 * Each datagram carries exactly one SMP packet, without any additional
 * framing, the same as the UDP transport of mcumgr clients and Zephyr.
 * Address of the requester is kept in the mbuf user header, SMP copies
 * it to the response so it can be sent back to the same peer.
 */

#include <assert.h>
#include <string.h>

#include "os/mynewt.h"
#include <mgmt/mgmt.h>
#include <mynewt_smp/smp.h>
#include <mn_socket/mn_socket.h>

#if !MYNEWT_VAL(SMP_UDP_IPV4) && !MYNEWT_VAL(SMP_UDP_IPV6)
#error "SMP_UDP_IPV4 or SMP_UDP_IPV6 must be enabled"
#endif

/* Big enough for both IPv4 and IPv6 peer address. */
#define SMP_UDP_USRHDR_LEN sizeof(struct mn_sockaddr_in6)

struct smp_udp_sock {
    struct mn_socket *sock;
    struct os_event rx_ev;
};

struct smp_udp_state {
    struct smp_transport transport;
#if MYNEWT_VAL(SMP_UDP_IPV4)
    struct smp_udp_sock v4;
#endif
#if MYNEWT_VAL(SMP_UDP_IPV6)
    struct smp_udp_sock v6;
#endif
};

static struct smp_udp_state smp_udp_state;

static uint16_t
smp_udp_mtu(struct os_mbuf *m)
{
    (void)m;

    return MYNEWT_VAL(SMP_UDP_MTU);
}

static struct mn_socket *
smp_udp_sock_for_family(uint8_t family)
{
#if MYNEWT_VAL(SMP_UDP_IPV4)
    if (family == MN_AF_INET) {
        return smp_udp_state.v4.sock;
    }
#endif
#if MYNEWT_VAL(SMP_UDP_IPV6)
    if (family == MN_AF_INET6) {
        return smp_udp_state.v6.sock;
    }
#endif
    return NULL;
}

/**
 * Called by SMP to send response back to the peer stored in the user
 * header.  The mbuf is always consumed.
 */
static int
smp_udp_out(struct os_mbuf *m)
{
    struct mn_sockaddr_in6 to;
    struct mn_socket *sock;
    int rc;

    if (OS_MBUF_USRHDR_LEN(m) < SMP_UDP_USRHDR_LEN) {
        goto err;
    }
    /* User header may not be aligned for struct access, copy it. */
    memcpy(&to, OS_MBUF_USRHDR(m), sizeof(to));

    sock = smp_udp_sock_for_family(to.msin6_family);
    if (sock == NULL) {
        goto err;
    }

    rc = mn_sendto(sock, m, (struct mn_sockaddr *)&to);
    if (rc != 0) {
        goto err;
    }

    return 0;
err:
    os_mbuf_free_chain(m);
    return -1;
}

/**
 * Store peer address in the user header of the request.  If the mbuf
 * from the socket has no room for it, the data is copied to a new one.
 *
 * @return  mbuf to pass to SMP, NULL if memory was exhausted; the
 *          original mbuf is consumed in that case.
 */
static struct os_mbuf *
smp_udp_set_peer(struct os_mbuf *m, const struct mn_sockaddr_in6 *from)
{
    struct os_mbuf *n;
    int rc;

    if (OS_MBUF_USRHDR_LEN(m) < SMP_UDP_USRHDR_LEN) {
        n = os_msys_get_pkthdr(OS_MBUF_PKTLEN(m), SMP_UDP_USRHDR_LEN);
        if (n == NULL) {
            os_mbuf_free_chain(m);
            return NULL;
        }
        rc = os_mbuf_appendfrom(n, m, 0, OS_MBUF_PKTLEN(m));
        os_mbuf_free_chain(m);
        if (rc != 0) {
            os_mbuf_free_chain(n);
            return NULL;
        }
        m = n;
    }
    memcpy(OS_MBUF_USRHDR(m), from, sizeof(*from));

    return m;
}

/**
 * Drains the socket, called in SMP task context.
 */
static void
smp_udp_rx_event(struct os_event *ev)
{
    struct smp_udp_sock *us = ev->ev_arg;
    struct mn_sockaddr_in6 from;
    struct os_mbuf *m;

    while (1) {
        memset(&from, 0, sizeof(from));
        if (mn_recvfrom(us->sock, &m, (struct mn_sockaddr *)&from) != 0) {
            break;
        }
        if (m == NULL) {
            continue;
        }
        if (OS_MBUF_PKTLEN(m) < sizeof(struct mgmt_hdr)) {
            os_mbuf_free_chain(m);
            continue;
        }
        m = smp_udp_set_peer(m, &from);
        if (m == NULL) {
            continue;
        }
        /* smp_rx_req consumes the mbuf even on failure. */
        (void)smp_rx_req(&smp_udp_state.transport, m);
    }
}

/**
 * Socket callback, may be called from IP stack context.
 */
static void
smp_udp_readable(void *arg, int err)
{
    struct smp_udp_sock *us = arg;

    (void)err;

    os_eventq_put(mgmt_evq_get(), &us->rx_ev);
}

static const union mn_socket_cb smp_udp_sock_cbs = {
    .socket.readable = smp_udp_readable,
    .socket.writable = NULL,
};

static int
smp_udp_sock_open(struct smp_udp_sock *us, uint8_t domain)
{
    struct mn_sockaddr_in6 sin6;
    struct mn_sockaddr_in sin;
    struct mn_sockaddr *addr;
    int rc;

    rc = mn_socket(&us->sock, domain, MN_SOCK_DGRAM, 0);
    if (rc != 0) {
        return rc;
    }

    us->rx_ev.ev_cb = smp_udp_rx_event;
    us->rx_ev.ev_arg = us;
    mn_socket_set_cbs(us->sock, us, &smp_udp_sock_cbs);

    if (domain == MN_PF_INET6) {
        memset(&sin6, 0, sizeof(sin6));
        sin6.msin6_len = sizeof(sin6);
        sin6.msin6_family = MN_AF_INET6;
        sin6.msin6_port = htons(MYNEWT_VAL(SMP_UDP_PORT));
        memcpy(&sin6.msin6_addr, nm_in6addr_any, sizeof(sin6.msin6_addr));
        addr = (struct mn_sockaddr *)&sin6;
    } else {
        memset(&sin, 0, sizeof(sin));
        sin.msin_len = sizeof(sin);
        sin.msin_family = MN_AF_INET;
        sin.msin_port = htons(MYNEWT_VAL(SMP_UDP_PORT));
        sin.msin_addr.s_addr = 0;
        addr = (struct mn_sockaddr *)&sin;
    }

    rc = mn_bind(us->sock, addr);
    if (rc != 0) {
        mn_close(us->sock);
        us->sock = NULL;
    }

    return rc;
}

void
smp_udp_pkg_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = smp_transport_init(&smp_udp_state.transport, smp_udp_out, smp_udp_mtu);
    SYSINIT_PANIC_ASSERT(rc == 0);

#if MYNEWT_VAL(SMP_UDP_IPV4)
    rc = smp_udp_sock_open(&smp_udp_state.v4, MN_PF_INET);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
#if MYNEWT_VAL(SMP_UDP_IPV6)
    rc = smp_udp_sock_open(&smp_udp_state.v6, MN_PF_INET6);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}
