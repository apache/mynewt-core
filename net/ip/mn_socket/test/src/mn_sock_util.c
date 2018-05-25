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

#include <stdio.h>
#include <string.h>

#include "os/mynewt.h"
#include "testutil/testutil.h"

#include "mn_socket/mn_socket.h"

struct os_sem test_sem;

void
sock_open_close(void)
{
    struct mn_socket *sock;
    int rc;

    rc = mn_socket(&sock, MN_PF_INET, MN_SOCK_DGRAM, 0);
    TEST_ASSERT(sock);
    TEST_ASSERT(rc == 0);
    mn_close(sock);

    rc = mn_socket(&sock, MN_PF_INET, MN_SOCK_STREAM, 0);
    TEST_ASSERT(sock);
    TEST_ASSERT(rc == 0);
    mn_close(sock);

    rc = mn_socket(&sock, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    TEST_ASSERT(sock);
    TEST_ASSERT(rc == 0);
    mn_close(sock);

    rc = mn_socket(&sock, MN_PF_INET6, MN_SOCK_STREAM, 0);
    TEST_ASSERT(sock);
    TEST_ASSERT(rc == 0);
    mn_close(sock);
}

void
sock_listen(void)
{
    struct mn_socket *sock;
    struct mn_sockaddr_in msin;
    int rc;

    rc = mn_socket(&sock, MN_PF_INET, MN_SOCK_STREAM, 0);
    TEST_ASSERT(rc == 0);

    msin.msin_family = MN_PF_INET;
    msin.msin_len = sizeof(msin);
    msin.msin_port = htons(12444);

    mn_inet_pton(MN_PF_INET, "127.0.0.1", &msin.msin_addr);

    rc = mn_bind(sock, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    rc = mn_listen(sock, 2);
    TEST_ASSERT(rc == 0);

    mn_close(sock);
}

void
stc_writable(void *cb_arg, int err)
{
    int *i;

    TEST_ASSERT(err == 0);
    i = (int *)cb_arg;
    *i = *i + 1;

    /*
     * The first instance of writability indicates an established connection.
     * Unblock the test case.
     */
    if (*i == 1) {
        os_sem_release(&test_sem);
    }
}

int
stc_newconn(void *cb_arg, struct mn_socket *new)
{
    struct mn_socket **r_sock;

    r_sock = cb_arg;
    *r_sock = new;

    os_sem_release(&test_sem);
    return 0;
}

void
sock_tcp_connect(void)
{
    struct mn_socket *listen_sock;
    struct mn_socket *sock;
    struct mn_sockaddr_in msin;
    struct mn_sockaddr_in msin2;
    int rc;
    union mn_socket_cb listen_cbs = {
        .listen.newconn = stc_newconn,
    };
    union mn_socket_cb sock_cbs = {
        .socket.writable = stc_writable
    };
    int connected = 0;
    struct mn_socket *new_sock = NULL;

    rc = mn_socket(&listen_sock, MN_PF_INET, MN_SOCK_STREAM, 0);
    TEST_ASSERT(rc == 0);

    msin.msin_family = MN_PF_INET;
    msin.msin_len = sizeof(msin);
    msin.msin_port = htons(12445);

    mn_inet_pton(MN_PF_INET, "127.0.0.1", &msin.msin_addr);

    mn_socket_set_cbs(listen_sock, &new_sock, &listen_cbs);
    rc = mn_bind(listen_sock, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    rc = mn_listen(listen_sock, 2);
    TEST_ASSERT(rc == 0);

    rc = mn_socket(&sock, MN_PF_INET, MN_SOCK_STREAM, 0);
    TEST_ASSERT(rc == 0);

    mn_socket_set_cbs(sock, &connected, &sock_cbs);

    rc = mn_connect(sock, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    /*
     * Wait for both connections to be established.
     */
    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == 0);
    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(connected == 1);
    TEST_ASSERT(new_sock != NULL);

    /*
     * Check endpoint data matches
     */
    rc = mn_getsockname(sock, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);
    rc = mn_getpeername(new_sock, (struct mn_sockaddr *)&msin2);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(!memcmp(&msin, &msin2, sizeof(msin)));

    rc = mn_getsockname(new_sock, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);
    rc = mn_getpeername(sock, (struct mn_sockaddr *)&msin2);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(!memcmp(&msin, &msin2, sizeof(msin)));


    if (new_sock) {
        mn_close(new_sock);
    }
    mn_close(sock);
    mn_close(listen_sock);
}

void
sud_readable(void *cb_arg, int err)
{
    os_sem_release(&test_sem);
}

void
sock_udp_data(void)
{
    struct mn_socket *sock1;
    struct mn_socket *sock2;
    struct mn_sockaddr_in msin;
    struct mn_sockaddr_in msin2;
    int rc;
    union mn_socket_cb sock_cbs = {
        .socket.readable = sud_readable
    };
    struct os_mbuf *m;
    char data[] = "1234567890";

    rc = mn_socket(&sock1, MN_PF_INET, MN_SOCK_DGRAM, 0);
    TEST_ASSERT(rc == 0);
    mn_socket_set_cbs(sock1, NULL, &sock_cbs);

    rc = mn_socket(&sock2, MN_PF_INET, MN_SOCK_DGRAM, 0);
    TEST_ASSERT(rc == 0);
    mn_socket_set_cbs(sock2, NULL, &sock_cbs);

    msin.msin_family = MN_PF_INET;
    msin.msin_len = sizeof(msin);
    msin.msin_port = htons(12445);

    mn_inet_pton(MN_PF_INET, "127.0.0.1", &msin.msin_addr);

    rc = mn_bind(sock1, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    msin2.msin_family = MN_PF_INET;
    msin2.msin_len = sizeof(msin2);
    msin2.msin_port = 0;
    msin2.msin_addr.s_addr = 0;
    rc = mn_bind(sock2, (struct mn_sockaddr *)&msin2);
    TEST_ASSERT(rc == 0);

    m = os_msys_get(sizeof(data), 0);
    TEST_ASSERT(m);
    rc = os_mbuf_copyinto(m, 0, data, sizeof(data));
    TEST_ASSERT(rc == 0);
    rc = mn_sendto(sock2, (struct os_mbuf *)m, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    /*
     * Wait for the packet.
     */
    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == 0);

    rc = mn_recvfrom(sock1, &m, (struct mn_sockaddr *)&msin2);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(m != NULL);
    TEST_ASSERT(msin2.msin_family == MN_AF_INET);
    TEST_ASSERT(msin2.msin_len == sizeof(msin2));
    TEST_ASSERT(msin2.msin_port != 0);
    TEST_ASSERT(msin2.msin_addr.s_addr != 0);

    if (m) {
        TEST_ASSERT(OS_MBUF_IS_PKTHDR(m));
        TEST_ASSERT(OS_MBUF_PKTLEN(m) == sizeof(data));
        TEST_ASSERT(m->om_len == sizeof(data));
        TEST_ASSERT(!memcmp(m->om_data, data, sizeof(data)));
    }

    rc = mn_sendto(sock1, m, (struct mn_sockaddr *)&msin2);
    TEST_ASSERT(rc == 0);

    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == 0);

    rc = mn_recvfrom(sock2, &m, (struct mn_sockaddr *)&msin2);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(m != NULL);
    if (m) {
        TEST_ASSERT(OS_MBUF_IS_PKTHDR(m));
        TEST_ASSERT(OS_MBUF_PKTLEN(m) == sizeof(data));
        TEST_ASSERT(m->om_len == sizeof(data));
        TEST_ASSERT(!memcmp(m->om_data, data, sizeof(data)));
        os_mbuf_free_chain(m);
    }

    mn_close(sock1);
    mn_close(sock2);
}

void
std_writable(void *cb_arg, int err)
{
    int *i;

    TEST_ASSERT(err == 0);

    i = (int *)cb_arg;
    TEST_ASSERT_FATAL(i != NULL);

    *i = *i + 1;

    /*
     * The first instance of writability indicates an established connection.
     * Unblock the test case.
     */
    if (*i == 1) {
        os_sem_release(&test_sem);
    }
}

void
std_readable(void *cb_arg, int err)
{
    os_sem_release(&test_sem);
}

static union mn_socket_cb sud_sock_cbs = {
    .socket.writable = std_writable,
    .socket.readable = std_readable
};

int
std_newconn(void *cb_arg, struct mn_socket *new)
{
    struct mn_socket **r_sock;

    r_sock = cb_arg;
    *r_sock = new;

    mn_socket_set_cbs(new, NULL, &sud_sock_cbs);

    os_sem_release(&test_sem);
    return 0;
}

void
sock_tcp_data(void)
{
    struct mn_socket *listen_sock;
    struct mn_socket *sock;
    struct mn_sockaddr_in msin;
    int rc;
    union mn_socket_cb listen_cbs = {
        .listen.newconn = std_newconn,
    };
    int connected = 0;
    struct mn_socket *new_sock = NULL;
    struct os_mbuf *m;
    char data[] = "1234567890";

    rc = mn_socket(&listen_sock, MN_PF_INET, MN_SOCK_STREAM, 0);
    TEST_ASSERT(rc == 0);

    msin.msin_family = MN_PF_INET;
    msin.msin_len = sizeof(msin);
    msin.msin_port = htons(12447);

    mn_inet_pton(MN_PF_INET, "127.0.0.1", &msin.msin_addr);

    mn_socket_set_cbs(listen_sock, &new_sock, &listen_cbs);
    rc = mn_bind(listen_sock, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    rc = mn_listen(listen_sock, 2);
    TEST_ASSERT(rc == 0);

    rc = mn_socket(&sock, MN_PF_INET, MN_SOCK_STREAM, 0);
    TEST_ASSERT(rc == 0);

    mn_socket_set_cbs(sock, &connected, &sud_sock_cbs);

    rc = mn_connect(sock, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    /*
     * Wait for both connections to be established.
     */
    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == 0);
    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(connected == 1);
    TEST_ASSERT(new_sock != NULL);

    m = os_msys_get(sizeof(data), 0);
    TEST_ASSERT(m);
    rc = os_mbuf_copyinto(m, 0, data, sizeof(data));
    TEST_ASSERT(rc == 0);
    rc = mn_sendto(new_sock, (struct os_mbuf *)m, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    /*
     * Wait for the packet.
     */
    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == 0);

    memset(&msin, 0, sizeof(msin));
    rc = mn_recvfrom(sock, &m, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(m != NULL);
    TEST_ASSERT(msin.msin_family == MN_AF_INET);
    TEST_ASSERT(msin.msin_len == sizeof(msin));
    TEST_ASSERT(msin.msin_port != 0);
    TEST_ASSERT(msin.msin_addr.s_addr != 0);
    os_mbuf_free_chain(m);

    mn_close(new_sock);
    mn_close(sock);
    mn_close(listen_sock);
}

void
sock_itf_list(void)
{
    struct mn_itf itf;
    struct mn_itf_addr itf_addr;
    int if_cnt = 0;
    int seen_127 = 0;
    struct mn_in_addr addr127;
    char addr_str[64];
    int rc;

    mn_inet_pton(MN_PF_INET, "127.0.0.1", &addr127);

    memset(&itf, 0, sizeof(itf));

    while (1) {
        rc = mn_itf_getnext(&itf);
        if (rc) {
            break;
        }
        printf("%d: %x %s\n", itf.mif_idx, itf.mif_flags, itf.mif_name);
        memset(&itf_addr, 0, sizeof(itf_addr));
        while (1) {
            rc = mn_itf_addr_getnext(&itf, &itf_addr);
            if (rc) {
                break;
            }
            if (itf_addr.mifa_family == MN_AF_INET &&
              !memcmp(&itf_addr.mifa_addr, &addr127, sizeof(addr127))) {
                seen_127 = 1;
            }
            addr_str[0] = '\0';
            mn_inet_ntop(itf_addr.mifa_family, &itf_addr.mifa_addr,
              addr_str, sizeof(addr_str));
            printf(" %s/%d\n", addr_str, itf_addr.mifa_plen);
        }
        if_cnt++;
    }
    TEST_ASSERT(if_cnt > 0);
    TEST_ASSERT(seen_127);
}

static int
first_ll_addr(struct mn_sockaddr_in6 *ra)
{
    struct mn_itf itf;
    struct mn_itf_addr itf_addr;
    int rc;
    struct mn_in6_addr *addr;

    memset(&itf, 0, sizeof(itf));
    addr = (struct mn_in6_addr *)&itf_addr.mifa_addr;
    while (1) {
        rc = mn_itf_getnext(&itf);
        if (rc) {
            break;
        }
        memset(&itf_addr, 0, sizeof(itf_addr));
        while (1) {
            rc = mn_itf_addr_getnext(&itf, &itf_addr);
            if (rc) {
                break;
            }
            if (itf_addr.mifa_family == MN_AF_INET6 &&
              addr->s_addr[0] == 0xfe && addr->s_addr[1] == 0x80) {
                memset(ra, 0, sizeof(*ra));
                ra->msin6_family = MN_AF_INET6;
                ra->msin6_len = sizeof(*ra);
                ra->msin6_scope_id = itf.mif_idx;
                memcpy(&ra->msin6_addr, addr, sizeof(*addr));
                return 0;
            }
        }
    }
    return -1;
}

void
sul_readable(void *cb_arg, int err)
{
    os_sem_release(&test_sem);
}

void
sock_udp_ll(void)
{
    struct mn_socket *sock1;
    struct mn_socket *sock2;
    struct mn_sockaddr_in6 msin;
    struct mn_sockaddr_in6 msin2;
    int rc;
    union mn_socket_cb sock_cbs = {
        .socket.readable = sul_readable
    };
    struct os_mbuf *m;
    char data[] = "1234567890";

    rc = mn_socket(&sock1, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    TEST_ASSERT(rc == 0);
    mn_socket_set_cbs(sock1, NULL, &sock_cbs);

    rc = mn_socket(&sock2, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    TEST_ASSERT(rc == 0);
    mn_socket_set_cbs(sock2, NULL, &sock_cbs);

    rc = first_ll_addr(&msin);
    if (rc != 0) {
        printf("No ipv6 address present?\n");
        return;
    }
    msin.msin6_port = htons(12445);

    rc = mn_bind(sock1, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    rc = mn_getsockname(sock1, (struct mn_sockaddr *)&msin2);
    TEST_ASSERT(rc == 0);

    m = os_msys_get(sizeof(data), 0);
    TEST_ASSERT(m);
    rc = os_mbuf_copyinto(m, 0, data, sizeof(data));
    TEST_ASSERT(rc == 0);
    rc = mn_sendto(sock2, (struct os_mbuf *)m, (struct mn_sockaddr *)&msin2);
    TEST_ASSERT(rc == 0);

    /*
     * Wait for the packet.
     */
    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == 0);

    rc = mn_recvfrom(sock1, &m, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(m != NULL);

    if (m) {
        TEST_ASSERT(OS_MBUF_IS_PKTHDR(m));
        TEST_ASSERT(OS_MBUF_PKTLEN(m) == sizeof(data));
        TEST_ASSERT(m->om_len == sizeof(data));
        TEST_ASSERT(!memcmp(m->om_data, data, sizeof(data)));
        os_mbuf_free_chain(m);
    }

    mn_close(sock1);
    mn_close(sock2);
}

static int
sock_find_multicast_if(void)
{
    struct mn_itf itf;

    memset(&itf, 0, sizeof(itf));

    while (1) {
        if (mn_itf_getnext(&itf)) {
            break;
        }
        if ((itf.mif_flags & MN_ITF_F_UP) == 0) {
            continue;
        }
        if (itf.mif_flags & MN_ITF_F_MULTICAST) {
            return itf.mif_idx;
        }
    }
    return -1;
}

void
sum4_readable(void *cb_arg, int err)
{
    os_sem_release(&test_sem);
}

void
sock_udp_mcast_v4(void)
{
    int loop_if_idx;
    struct mn_socket *rx_sock;
    struct mn_socket *tx_sock;
    struct mn_sockaddr_in msin;
    union mn_socket_cb sock_cbs = {
        .socket.readable = sum4_readable
    };
    struct os_mbuf *m;
    char data[] = "1234567890";
    int rc;
    struct mn_mreq mreq;
    loop_if_idx = sock_find_multicast_if();
    TEST_ASSERT(loop_if_idx > 0);

    msin.msin_family = MN_AF_INET;
    msin.msin_len = sizeof(msin);
    msin.msin_port = htons(44344);
    memset(&msin.msin_addr, 0, sizeof(msin.msin_addr));

    rc = mn_socket(&rx_sock, MN_PF_INET, MN_SOCK_DGRAM, 0);
    TEST_ASSERT(rc == 0);
    mn_socket_set_cbs(rx_sock, NULL, &sock_cbs);

    rc = mn_bind(rx_sock, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    rc = mn_socket(&tx_sock, MN_PF_INET, MN_SOCK_DGRAM, 0);
    TEST_ASSERT(rc == 0);

    rc = mn_setsockopt(tx_sock, MN_SO_LEVEL, MN_MCAST_IF, &loop_if_idx);
    TEST_ASSERT(rc == 0);

    m = os_msys_get(sizeof(data), 0);
    rc = os_mbuf_copyinto(m, 0, data, sizeof(data));
    TEST_ASSERT(rc == 0);

    /*
     * multicast tgt
     */
    mn_inet_pton(MN_PF_INET, "224.0.2.241", &msin.msin_addr);

    rc = mn_sendto(tx_sock, (struct os_mbuf *)m, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    /*
     * RX socket has not joined group yet.
     */
    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC / 2);
    TEST_ASSERT(rc == OS_TIMEOUT);

    mreq.mm_idx = loop_if_idx;
    mreq.mm_family = MN_AF_INET;
    mreq.mm_addr.v4.s_addr = msin.msin_addr.s_addr;

    /*
     * Now join it.
     */
    rc = mn_setsockopt(rx_sock, MN_SO_LEVEL, MN_MCAST_JOIN_GROUP, &mreq);
    TEST_ASSERT(rc == 0);

    m = os_msys_get(sizeof(data), 0);
    rc = os_mbuf_copyinto(m, 0, data, sizeof(data));
    TEST_ASSERT(rc == 0);

    rc = mn_sendto(tx_sock, (struct os_mbuf *)m, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == 0);

    rc = mn_recvfrom(rx_sock, &m, NULL);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(m != NULL);
    TEST_ASSERT(!memcmp(m->om_data, data, sizeof(data)));
    os_mbuf_free_chain(m);

    /*
     * Then leave
     */
    rc = mn_setsockopt(rx_sock, MN_SO_LEVEL, MN_MCAST_LEAVE_GROUP, &mreq);
    TEST_ASSERT(rc == 0);

    m = os_msys_get(sizeof(data), 0);
    TEST_ASSERT(m);
    rc = os_mbuf_copyinto(m, 0, data, sizeof(data));
    TEST_ASSERT(rc == 0);

    rc = mn_sendto(tx_sock, (struct os_mbuf *)m, (struct mn_sockaddr *)&msin);
    TEST_ASSERT(rc == 0);

    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == OS_TIMEOUT);

    mn_close(rx_sock);
    mn_close(tx_sock);
}

void
sock_udp_mcast_v6(void)
{
    int loop_if_idx;
    struct mn_socket *rx_sock;
    struct mn_socket *tx_sock;
    struct mn_sockaddr_in6 msin6;
    union mn_socket_cb sock_cbs = {
        .socket.readable = sum4_readable
    };
    struct os_mbuf *m;
    char data[] = "1234567890";
    int rc;
    struct mn_mreq mreq;
    uint8_t mcast_addr[16] = {
        0xff, 2, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 2
    };

    loop_if_idx = sock_find_multicast_if();
    TEST_ASSERT(loop_if_idx > 0);

    msin6.msin6_family = MN_AF_INET6;
    msin6.msin6_len = sizeof(msin6);
    msin6.msin6_port = htons(44344);
    msin6.msin6_scope_id = 0;
    memset(&msin6.msin6_addr, 0, sizeof(msin6.msin6_addr));

    rc = mn_socket(&rx_sock, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    TEST_ASSERT(rc == 0);
    mn_socket_set_cbs(rx_sock, NULL, &sock_cbs);

    rc = mn_bind(rx_sock, (struct mn_sockaddr *)&msin6);
    TEST_ASSERT(rc == 0);

    rc = mn_socket(&tx_sock, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    TEST_ASSERT(rc == 0);

    rc = mn_setsockopt(tx_sock, MN_SO_LEVEL, MN_MCAST_IF, &loop_if_idx);
    TEST_ASSERT(rc == 0);

    m = os_msys_get(sizeof(data), 0);
    rc = os_mbuf_copyinto(m, 0, data, sizeof(data));
    TEST_ASSERT(rc == 0);

    /*
     * multicast tgt
     */
    memcpy(&msin6.msin6_addr, mcast_addr, sizeof(mcast_addr));

    rc = mn_sendto(tx_sock, (struct os_mbuf *)m, (struct mn_sockaddr *)&msin6);
    TEST_ASSERT(rc == 0);

    /*
     * RX socket has not joined group yet.
     */
    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC / 2);
    TEST_ASSERT(rc == OS_TIMEOUT);

    mreq.mm_idx = loop_if_idx;
    mreq.mm_family = MN_AF_INET6;
    memcpy(&mreq.mm_addr.v6.s_addr, msin6.msin6_addr.s_addr,
      sizeof(msin6.msin6_addr.s_addr));

    /*
     * Now join it.
     */
    rc = mn_setsockopt(rx_sock, MN_SO_LEVEL, MN_MCAST_JOIN_GROUP, &mreq);
    TEST_ASSERT(rc == 0);

    m = os_msys_get(sizeof(data), 0);
    rc = os_mbuf_copyinto(m, 0, data, sizeof(data));
    TEST_ASSERT(rc == 0);

    rc = mn_sendto(tx_sock, (struct os_mbuf *)m, (struct mn_sockaddr *)&msin6);
    TEST_ASSERT(rc == 0);

    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == 0);

    rc = mn_recvfrom(rx_sock, &m, NULL);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(m != NULL);
    TEST_ASSERT(!memcmp(m->om_data, data, sizeof(data)));
    os_mbuf_free_chain(m);

    /*
     * Then leave
     */
    rc = mn_setsockopt(rx_sock, MN_SO_LEVEL, MN_MCAST_LEAVE_GROUP, &mreq);
    TEST_ASSERT(rc == 0);

    m = os_msys_get(sizeof(data), 0);
    TEST_ASSERT(m);
    rc = os_mbuf_copyinto(m, 0, data, sizeof(data));
    TEST_ASSERT(rc == 0);

    rc = mn_sendto(tx_sock, (struct os_mbuf *)m, (struct mn_sockaddr *)&msin6);
    TEST_ASSERT(rc == 0);

    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == OS_TIMEOUT);

    mn_close(rx_sock);
    mn_close(tx_sock);
}

void
mn_socket_test_handler(void *arg)
{
    sock_open_close();
    sock_listen();
    sock_tcp_connect();
    sock_udp_data();
    sock_tcp_data();
    sock_itf_list();
    sock_udp_ll();
    sock_udp_mcast_v4();
    sock_udp_mcast_v6();
    tu_restart();
}
