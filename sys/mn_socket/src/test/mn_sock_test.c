/**
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

#include <os/os.h>
#include <os/../../src/test/os_test_priv.h>
#include <testutil/testutil.h>

#include "mn_socket/mn_socket.h"
#include "mn_socket/arch/sim/native_sock.h"

#define TEST_STACK_SIZE 4096
#define TEST_PRIO 22
static os_stack_t test_stack[OS_STACK_ALIGN(TEST_STACK_SIZE)];
static struct os_task test_task;

static struct os_sem test_sem;

#define MB_CNT 10
#define MB_SZ  512
static uint8_t test_mbuf_area[MB_CNT * MB_SZ];
static struct os_mempool test_mbuf_mpool;
static struct os_mbuf_pool test_mbuf_pool;

TEST_CASE(inet_pton_test)
{
    int rc;
    uint8_t addr[8];
    struct test_vec {
        char *str;
        uint8_t cmp[4];
    };
    struct test_vec ok_vec[] = {
        { "1.1.1.1", { 1, 1, 1, 1 } },
        { "1.2.3.4", { 1, 2, 3, 4 } },
        { "010.001.255.255", { 10, 1, 255, 255 } },
        { "001.002.005.006", { 1, 2, 5, 6 } }
    };
    struct test_vec invalid_vec[] = {
        { "a.b.c.d" },
        { "1a.b3.4.2" },
        { "1.3.4.2a" },
        { "1111.3.4.2" },
        { "3.256.1.0" },
    };
    int i;

    for (i = 0; i < sizeof(ok_vec) / sizeof(ok_vec[0]); i++) {
        memset(addr, 0xa5, sizeof(addr));
        rc = mn_inet_pton(MN_PF_INET, ok_vec[i].str, addr);
        TEST_ASSERT(rc == 1);
        TEST_ASSERT(!memcmp(ok_vec[i].cmp, addr, sizeof(uint32_t)));
        TEST_ASSERT(addr[5] == 0xa5);
    }
    for (i = 0; i < sizeof(invalid_vec) / sizeof(invalid_vec[0]); i++) {
        rc = mn_inet_pton(MN_PF_INET, invalid_vec[i].str, addr);
        TEST_ASSERT(rc == 0);
    }
}

TEST_CASE(inet_ntop_test)
{
    const char *rstr;
    char addr[48];
    struct test_vec {
        char *str;
        uint8_t cmp[4];
    };
    struct test_vec ok_vec[] = {
        { "1.1.1.1", { 1, 1, 1, 1 } },
        { "1.2.3.4", { 1, 2, 3, 4 } },
        { "255.1.255.255", { 255, 1, 255, 255 } },
        { "1.2.5.6", { 1, 2, 5, 6 } }
    };
    int i;

    for (i = 0; i < sizeof(ok_vec) / sizeof(ok_vec[0]); i++) {
        memset(addr, 0xa5, sizeof(addr));
        rstr = mn_inet_ntop(MN_PF_INET, ok_vec[i].cmp, addr, sizeof(addr));
        TEST_ASSERT(rstr);
        TEST_ASSERT(!strcmp(ok_vec[i].str, addr));
    }
    rstr = mn_inet_ntop(MN_PF_INET, ok_vec[0].cmp, addr, 1);
    TEST_ASSERT(rstr == NULL);

    /* does not have space to null terminate */
    rstr = mn_inet_ntop(MN_PF_INET, ok_vec[0].cmp, addr, 7);
    TEST_ASSERT(rstr == NULL);
}

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

    rc = os_sem_pend(&test_sem, OS_TICKS_PER_SEC);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(connected == 1);
    TEST_ASSERT(new_sock != NULL);

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
    msin2.msin_addr = 0;
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
    TEST_ASSERT(msin2.msin_addr != 0);

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
    if (i) {
        *i = *i + 1;
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
    TEST_ASSERT(msin.msin_addr != 0);
    os_mbuf_free_chain(m);

    if (new_sock) {
        mn_close(new_sock);
    }
    mn_close(sock);
    mn_close(listen_sock);
}

void
mn_socket_test_handler(void *arg)
{
    sock_open_close();
    sock_listen();
    sock_tcp_connect();
    sock_udp_data();
    sock_tcp_data();
    os_test_restart();
}

TEST_CASE(socket_tests)
{
    os_init();
    native_sock_init();

    os_sem_init(&test_sem, 0);

    os_task_init(&test_task, "mn_socket_test", mn_socket_test_handler, NULL,
      TEST_PRIO, OS_WAIT_FOREVER, test_stack, TEST_STACK_SIZE);
    os_start();
}

TEST_SUITE(mn_socket_test_all)
{
    int rc;

    rc = os_mempool_init(&test_mbuf_mpool, MB_CNT, MB_SZ, test_mbuf_area, "mb");
    TEST_ASSERT(rc == 0);
    rc = os_mbuf_pool_init(&test_mbuf_pool, &test_mbuf_mpool, MB_CNT, MB_CNT);
    TEST_ASSERT(rc == 0);
    rc = os_msys_register(&test_mbuf_pool);
    TEST_ASSERT(rc == 0);

    inet_pton_test();
    inet_ntop_test();

    socket_tests();
}

#ifdef MYNEWT_SELFTEST

int
main(int argc, char **argv)
{
    tu_config.tc_print_results = 1;
    tu_init();

    mn_socket_test_all();

    return tu_any_failed;
}
#endif

