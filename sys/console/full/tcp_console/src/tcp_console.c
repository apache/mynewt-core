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

#include <os/mynewt.h>

#include <console/console.h>
#include <mn_socket/mn_socket.h>
#include "lwip/tcpip.h"

static struct os_event rx_receive_event;

static struct os_mbuf *tcp_console_out_buf;
static struct os_mbuf *flushing_buf;

static struct mn_socket *server_socket;
static struct mn_socket *console_socket;

static void
tcp_console_flush_cb(void *ctx)
{
    int sr;
    int rc;

    if (flushing_buf == NULL && tcp_console_out_buf != NULL) {
        OS_ENTER_CRITICAL(sr);
        flushing_buf = tcp_console_out_buf;
        tcp_console_out_buf = NULL;
        OS_EXIT_CRITICAL(sr);
    }
    if (flushing_buf != NULL && console_socket != NULL) {
        rc = mn_sendto(console_socket, flushing_buf, NULL);
        if (rc) {
            os_mbuf_free_chain(flushing_buf);
        }
        flushing_buf = NULL;
    }
}

static void
tcp_console_schedule_tx_flush(void)
{
    tcpip_try_callback(tcp_console_flush_cb, NULL);
}

static void
tcp_console_write(int c)
{
    uint8_t buf[1] = { (uint8_t)c };
    struct os_mbuf *mbuf;
    int sr;
    bool flush;

    OS_ENTER_CRITICAL(sr);
    mbuf = tcp_console_out_buf;
    tcp_console_out_buf = NULL;
    OS_EXIT_CRITICAL(sr);
    if (NULL == mbuf) {
        mbuf = os_msys_get_pkthdr(0, 0);
        if (NULL == mbuf) {
            return;
        }
    }
    /* If current mbuf was full, try to send it to client */
    flush = OS_MBUF_TRAILINGSPACE(mbuf) < 2;
    os_mbuf_append(mbuf, buf, 1);
    tcp_console_out_buf = mbuf;
    if (flush) {
        tcp_console_schedule_tx_flush();
    }
}

int
console_out_nolock(int c)
{
    tcp_console_write(c);

    if ('\n' == c) {
        tcp_console_write('\r');
    }

    /*
     * Scheduling flush means that event will be process just after everything
     * else is done. Usually it means that data will be flushed after full
     * console_printf().
     */
    tcp_console_schedule_tx_flush();

    return c;
}

void
console_rx_restart(void)
{
    os_eventq_put(os_eventq_dflt_get(), &rx_receive_event);
}

static void
rx_ev_cb(struct os_event *ev)
{
    /* TODO: Check if this really needed */
}

static void
tcp_console_readable(void *arg, int err)
{
    struct os_mbuf *m;
    int rc;
    (void)rc;

    if (err == MN_ECONNABORTED) {
        if (console_socket) {
            mn_close(console_socket);
            console_socket = NULL;
        }
    } else if (err == 0) {
        assert(console_socket != NULL);
        rc = mn_recvfrom(console_socket, &m, NULL);
        assert(rc == 0);
        for (int i = 0; i < m->om_len; ++i) {
            console_handle_char(m->om_data[i]);
        }
        os_mbuf_free_chain(m);
    }
}

static void
tcp_console_writable(void *arg, int err)
{
    if (err == 0) {
        tcp_console_flush_cb(NULL);
    }
}

static const union mn_socket_cb tcp_console_cbs = {
    .socket.readable = tcp_console_readable,
    .socket.writable = tcp_console_writable,
};

static int
tcp_console_newconn(void *arg, struct mn_socket *new)
{
    mn_socket_set_cbs(new, NULL, &tcp_console_cbs);
    if (console_socket != NULL) {
        mn_close(console_socket);
    }
    console_socket = new;
    return 0;
}

static const union mn_socket_cb server_listen_cbs = {
    .listen.newconn = tcp_console_newconn,
};

int
tcp_console_pkg_init(void)
{
    struct mn_sockaddr_in sin = {0};
    int rc;

    rx_receive_event.ev_cb = rx_ev_cb;

    /* Create normal TCP socket */
    rc = mn_socket(&server_socket, MN_PF_INET, MN_SOCK_STREAM, 0);
    assert(rc == 0);

    sin.msin_len = sizeof(sin);
    sin.msin_family = MN_AF_INET;
    sin.msin_port = htons(MYNEWT_VAL(TCP_CONSOLE_PORT));

    /* Bind it to all interfaces */
    rc = mn_bind(server_socket, (struct mn_sockaddr *)&sin);
    assert(rc == 0);

    mn_socket_set_cbs(server_socket, NULL, &server_listen_cbs);

    /* Get ready for incoming connections */
    rc = mn_listen(server_socket, 2);
    assert(rc == 0);

    return 0;
}

int
tcp_console_is_init(void)
{
    /* TODO: Console check what should be returned */
    return 1;
}
