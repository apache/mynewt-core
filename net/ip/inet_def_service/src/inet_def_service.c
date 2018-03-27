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

#include "os/mynewt.h"
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

#define CHARGEN_WRITE_SZ     512
static const char chargen_pattern[] = "1234567890";
#define CHARGEN_PATTERN_SZ   (sizeof(chargen_pattern) - 1)

static struct os_eventq *inet_def_evq;

struct inet_def_event {
    struct os_event ide_ev;
    enum inet_def_type ide_type;
};

/*
 * UDP service.
 */
struct inet_def_udp {
    struct inet_def_event ev;           /* Datagram RX reported via event */
    struct mn_socket *socket;
    uint32_t pkt_cnt;
};

/*
 * Connected TCP socket.
 */
struct inet_def_tcp {
    struct inet_def_event ev;           /* Data RX reported with this event */
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
} inet_def;

static void inet_def_event(struct os_event *ev);

/*
 * UDP socket callbacks. Called in context of IP stack task.
 */
static void
inet_def_udp_readable(void *arg, int err)
{
    struct inet_def_udp *idu = (struct inet_def_udp *)arg;

    os_eventq_put(inet_def_evq, &idu->ev.ide_ev);
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

    if (err && !idt->closed) {
        idt->closed = 1;
        /*
         * No locking here. Assuming new connection notifications, as well as
         * close notifications arrive in context of a single task.
         */
        SLIST_REMOVE(&inet_def.tcp_conns, idt, inet_def_tcp, list);
    }
    os_eventq_put(inet_def_evq, &idt->ev.ide_ev);
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
    idt->ev.ide_type = type;
    idt->ev.ide_ev.ev_cb = inet_def_event;
    idt->ev.ide_ev.ev_arg = (void *)MN_SOCK_STREAM;
    mn_socket_set_cbs(new, idt, &inet_tcp_cbs);
    SLIST_INSERT_HEAD(&inet_def.tcp_conns, idt, list);

    if (type == INET_DEF_CHARGEN) {
        /*
         * Start transmitting right away.
         */
        os_eventq_put(inet_def_evq, &idt->ev.ide_ev);
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

static void
inet_def_event(struct os_event *ev)
{
    struct inet_def_event *ide;
    struct inet_def_udp *idu;
    struct inet_def_tcp *idt;
    struct mn_socket *sock;
    struct mn_sockaddr_in msin;
    struct os_mbuf *m;
    int rc;
    int off;
    int loop_cnt;

    ide = (struct inet_def_event *)ev;
    idt = (struct inet_def_tcp *)ev;
    idu = (struct inet_def_udp *)ev;
    if ((int)ev->ev_arg == MN_SOCK_DGRAM) {
        sock = idu->socket;
    } else {
        sock = idt->socket;
    }
    switch (ide->ide_type) {
    case INET_DEF_ECHO:
        while (mn_recvfrom(sock, &m, (struct mn_sockaddr *)&msin) == 0) {
            console_printf("echo %d bytes\n", OS_MBUF_PKTLEN(m));
            if ((int)ev->ev_arg == MN_SOCK_DGRAM) {
                rc = mn_sendto(sock, m, (struct mn_sockaddr *)&msin);
            } else {
                rc = mn_sendto(sock, m, NULL);
            }
            if (rc) {
                console_printf("  failed: %d!!!!\n", rc);
                os_mbuf_free_chain(m);
            }
        }
        break;
    case INET_DEF_DISCARD:
        while (mn_recvfrom(sock, &m, NULL) == 0) {
            console_printf("discard %d bytes\n", OS_MBUF_PKTLEN(m));
            os_mbuf_free_chain(m);
        }
        break;
    case INET_DEF_CHARGEN:
        while (mn_recvfrom(sock, &m, NULL) == 0) {
            os_mbuf_free_chain(m);
        }
        if ((int)ev->ev_arg == MN_SOCK_STREAM && idt->closed) {
            /*
             * Don't try to send tons of data to a closed socket.
             */
            break;
        }
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
        break;
    default:
        assert(0);
        break;
    }
    if ((int)ev->ev_arg == MN_SOCK_STREAM && idt->closed) {
        /*
         * Remote end has closed the connection, as indicated in the
         * callback. Close the socket and free the memory.
         */
        mn_socket_set_cbs(idt->socket, NULL, NULL);
        os_eventq_remove(inet_def_evq, &idt->ev.ide_ev);
        mn_close(idt->socket);
        os_free(idt);
    }
}

void
inet_def_service_init(struct os_eventq *evq)
{
    int i;

    inet_def_create_srv(INET_DEF_ECHO, ECHO_PORT);
    inet_def_create_srv(INET_DEF_DISCARD, DISCARD_PORT);
    inet_def_create_srv(INET_DEF_CHARGEN, CHARGEN_PORT);

    inet_def_evq = evq;
    SLIST_INIT(&inet_def.tcp_conns);
    for (i = 0; i < 3; i++) {
        inet_def.udp_service[i].ev.ide_type = i;
        inet_def.udp_service[i].ev.ide_ev.ev_cb = inet_def_event;
        inet_def.udp_service[i].ev.ide_ev.ev_arg = (void *)MN_SOCK_DGRAM;
    }
}

