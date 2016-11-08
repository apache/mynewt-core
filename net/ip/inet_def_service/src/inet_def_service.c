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

#include <os/os.h>
#include <os/endian.h>
#include <mn_socket/mn_socket.h>
#include <console/console.h>

#include "inet_def_service/inet_def_service.h"

#define ECHO_PORT       7
#define DISCARD_PORT    9
#define CHARGEN_PORT    19

enum inet_def_type {
    INET_DEF_ECHO = 0,
    INET_DEF_DISCARD,
    INET_DEF_CHARGEN,
    INET_DEF_MAXTYPE
};

#define INET_DEF_FROM_EVENT_TYPE(a) ((a) - OS_EVENT_T_PERUSER)
#define INET_EVENT_TYPE_FROM_DEF(a) ((a) + OS_EVENT_T_PERUSER)

#define CHARGEN_WRITE_SZ     512
static const char chargen_pattern[] = "1234567890";
#define CHARGEN_PATTERN_SZ   (sizeof(chargen_pattern) - 1)

static struct os_eventq *inet_def_evq;

static void inet_def_event_echo(struct os_event *ev);
static void inet_def_event_discard(struct os_event *ev);
static void inet_def_event_chargen(struct os_event *ev);
static void inet_def_event_start(struct os_event *ev);

/*
 * UDP service.
 */
struct inet_def_udp {
    struct os_event ev;                 /* Datagram RX reported via event */
    struct mn_socket *socket;
    uint32_t pkt_cnt;
};

/*
 * Connected TCP socket.
 */
struct inet_def_tcp {
    struct os_event ev;                 /* Data RX reported with this event */
    SLIST_ENTRY(inet_def_tcp) list;
    struct mn_socket *socket;
    int closed;
};

/*
 * TCP service.
 */
struct inet_def_listen {
    struct mn_socket *socket;
    uint32_t conn_cnt;
};

static struct inet_def {
    struct inet_def_listen tcp_service[INET_DEF_MAXTYPE];
    struct inet_def_udp udp_service[INET_DEF_MAXTYPE];
    SLIST_HEAD(, inet_def_tcp) tcp_conns; /* List of connected TCP sockets */
} inet_def = {
    .udp_service = {
        [INET_DEF_ECHO] = {
            .ev.ev_cb = inet_def_event_echo,
            .ev.ev_arg = (void *)MN_SOCK_DGRAM,
        },
        [INET_DEF_DISCARD] = {
            .ev.ev_cb = inet_def_event_discard,
            .ev.ev_arg = (void *)MN_SOCK_DGRAM,
        },
        [INET_DEF_CHARGEN] = {
            .ev.ev_cb = inet_def_event_chargen,
            .ev.ev_arg = (void *)MN_SOCK_DGRAM,
        },
    }
};

static struct os_eventq *
inet_def_evq_get(void)
{
    return inet_def_evq;
}

/*
 * UDP socket callbacks. Called in context of IP stack task.
 */
static void
inet_def_udp_readable(void *arg, int err)
{
    struct inet_def_udp *idu = (struct inet_def_udp *)arg;

    os_eventq_put(&inet_def_evq, &idu->ev);
}

static const union mn_socket_cb inet_udp_cbs = {
    .socket.readable = inet_def_udp_readable,
};

/*
 * TCP socket callbacks. Called in context of IP stack task.
 */
static void
inet_def_tcp_readable(void *arg, int err)
{
    struct inet_def_tcp *idt = (struct inet_def_tcp *)arg;

    if (err) {
        idt->closed = 1;
        /*
         * No locking here. Assuming new connection notifications, as well as
         * close notifications arrive in context of a single task.
         */
        SLIST_REMOVE(&inet_def.tcp_conns, idt, inet_def_tcp, list);
    }
    os_eventq_put(&inet_def_evq, &idt->ev);
}

static const union mn_socket_cb inet_tcp_cbs = {
    .socket.readable = inet_def_tcp_readable,
    .socket.writable = inet_def_tcp_readable /* we want the same behavior */
};

/*
 * Callback for new connection for TCP listen socket.
 */
static int inet_def_newconn(void *arg, struct mn_socket *new)
{
    struct inet_def_listen *idl = (struct inet_def_listen *)arg;
    struct inet_def_tcp *idt;
    enum inet_def_type type;

    idt = (struct inet_def_tcp *)os_malloc(sizeof(*idt));
    if (!idt) {
        return -1;
    }

    memset(idt, 0, sizeof(*idt));
    idt->socket = new;

    /*
     * Event type tells what type of service this connection belongs to.
     * Ev_arg says whether it's TCP or UDP.
     */
    type = idl - &inet_def.tcp_service[0];
    idt->ev.ev_type = INET_EVENT_TYPE_FROM_DEF(type);
    idt->ev.ev_arg = (void *)MN_SOCK_STREAM;
    mn_socket_set_cbs(new, idt, &inet_tcp_cbs);
    SLIST_INSERT_HEAD(&inet_def.tcp_conns, idt, list);

    if (type == INET_DEF_CHARGEN) {
        /*
         * Start transmitting right away.
         */
        os_eventq_put(&inet_def_evq, &idt->ev);
    }
    return 0;
}

static const union mn_socket_cb inet_listen_cbs =  {
    .listen.newconn = inet_def_newconn,
};

static int
inet_def_create_srv(enum inet_def_type idx, uint16_t port)
{
    struct mn_socket *ms;
    struct mn_sockaddr_in msin;

    memset(&msin, 0, sizeof(msin));
    msin.msin_len = sizeof(msin);
    msin.msin_family = MN_AF_INET;
    msin.msin_port = htons(port);

    /*
     * Create TCP listen socket for service.
     */
    if (mn_socket(&ms, MN_PF_INET, MN_SOCK_STREAM, 0)) {
        goto err;
    }
    inet_def.tcp_service[idx].socket = ms;
    mn_socket_set_cbs(ms, &inet_def.tcp_service[idx], &inet_listen_cbs);

    if (mn_bind(ms, (struct mn_sockaddr *)&msin)) {
        goto err;
    }
    if (mn_listen(ms, 1)) {
        goto err;
    }

    /*
     * Create UDP socket for service.
     */
    if (mn_socket(&ms, MN_PF_INET, MN_SOCK_DGRAM, 0)) {
        goto err;
    }
    inet_def.udp_service[idx].socket = ms;
    mn_socket_set_cbs(ms, &inet_def.udp_service[idx], &inet_udp_cbs);

    if (mn_bind(ms, (struct mn_sockaddr *)&msin)) {
        goto err;
    }

    return 0;
err:
    if (inet_def.tcp_service[idx].socket) {
        mn_close(inet_def.tcp_service[idx].socket);
        inet_def.tcp_service[idx].socket = NULL;
    }
    if (inet_def.udp_service[idx].socket) {
        mn_close(inet_def.udp_service[idx].socket);
        inet_def.udp_service[idx].socket = NULL;
    }
    return -1;
}

static struct mn_socket *
inet_def_socket_from_event(struct os_event *ev,
                           struct inet_def_udp **out_idu,
                           struct inet_def_tcp **out_idt)
{
    struct inet_def_udp *idu;
    struct inet_def_tcp *idt;
    struct mn_socket *sock;

    if ((int)ev->ev_arg == MN_SOCK_DGRAM) {
        idu = (struct inet_def_udp *)ev;
        idt = NULL;
        sock = idu->socket;
    } else {
        idt = (struct inet_def_tcp *)ev;
        idu = NULL;
        sock = idt->socket;
    }

    if (out_idu != NULL) {
        *out_idu = idu;
    }
    if (out_idt != NULL) {
        *out_idt = idt;
    }

    return sock;
}

static void
inet_def_free_closed_tcp_sock(struct inet_def_tcp *idt)
{
    if (idt != NULL && idt->closed) {
        /*
         * Remote end has closed the connection, as indicated in the
         * callback. Close the socket and free the memory.
         */
        mn_socket_set_cbs(idt->socket, NULL, NULL);
        os_eventq_remove(inet_def_evq_get(), &idt->ev);
        mn_close(idt->socket);
        os_free(idt);
    }
}

static void
inet_def_event_echo(struct os_event *ev)
{
    struct mn_sockaddr_in msin;
    struct inet_def_tcp *idt;
    struct mn_socket *sock;
    struct os_mbuf *m;
    int rc;

    sock = inet_def_socket_from_event(ev, NULL, &idt);
    while (mn_recvfrom(sock, &m, (struct mn_sockaddr *)&msin) == 0) {
        console_printf("echo %d bytes\n", OS_MBUF_PKTLEN(m));
        rc = mn_sendto(sock, m, (struct mn_sockaddr *)&msin);
        if (rc) {
            console_printf("  failed: %d!!!!\n", rc);
            os_mbuf_free_chain(m);
        }
    }

    inet_def_free_closed_tcp_sock(idt);
}

static void
inet_def_event_discard(struct os_event *ev)
{
    struct inet_def_tcp *idt;
    struct mn_socket *sock;
    struct os_mbuf *m;

    sock = inet_def_socket_from_event(ev, NULL, NULL);
    while (mn_recvfrom(sock, &m, NULL) == 0) {
        console_printf("discard %d bytes\n", OS_MBUF_PKTLEN(m));
        os_mbuf_free_chain(m);
    }

    inet_def_free_closed_tcp_sock(idt);
}

static void
inet_def_event_chargen(struct os_event *ev)
{
    struct inet_def_tcp *idt;
    struct mn_socket *sock;
    struct os_mbuf *m;
    int loop_cnt;

    sock = inet_def_socket_from_event(ev, NULL, &idt);
    while (mn_recvfrom(sock, &m, NULL) == 0) {
        os_mbuf_free_chain(m);
    }
    if (idt == NULL || !idt->closed) {
        /*
         * Don't try to send tons of data to a closed socket.
         */
        loop_cnt = 0;
        do {
            m = os_msys_get(CHARGEN_WRITE_SZ, 0);
            if (m) {
                for (off = 0;
                     OS_MBUF_TRAILINGSPACE(m) >= CHARGEN_PATTERN_SZ;
                     off += CHARGEN_PATTERN_SZ) {
                    os_mbuf_copyinto(m, off, chargen_pattern,
                      CHARGEN_PATTERN_SZ);
                }
                console_printf("chargen %d bytes\n", m->om_len);
                rc = mn_sendto(sock, m, NULL); /* assumes TCP for now */
                if (rc) {
                    os_mbuf_free_chain(m);
                    if (rc != MN_ENOBUFS && rc != MN_EAGAIN) {
                        console_write("  sendto fail!!! %d\n", rc);
                    }
                    break;
                }
            } else {
                /*
                 * Mbuf shortage. Wait for them to appear.
                 */
                os_time_delay(1);
            }
            loop_cnt++;
        } while (loop_cnt < 32);
    }

    inet_def_free_closed_tcp_sock(idt);
}

static void
inet_def_event_start(void)
{
    inet_def_create_srv(INET_DEF_ECHO, ECHO_PORT);
    inet_def_create_srv(INET_DEF_DISCARD, DISCARD_PORT);
    inet_def_create_srv(INET_DEF_CHARGEN, CHARGEN_PORT);
}

void
inet_def_service_init(struct os_eventq *evq)
{
    inet_def_evq = evq;
    os_eventq_ensure(&inet_def_evq, NULL);
    SLIST_INIT(&inet_def.tcp_conns);
}

