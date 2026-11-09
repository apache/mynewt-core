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

#include <os/mynewt.h>

#include <console/console.h>
#include <mn_socket/mn_socket.h>
#include "lwip/tcpip.h"

#define TX_BUF_COUNT MYNEWT_VAL(CONSOLE_TCP_TX_BUF_COUNT)
/* Each block holds mbuf header, packet header and data */
#define TX_BLOCK_SIZE                                                         \
    OS_ALIGN(sizeof(struct os_mbuf) + sizeof(struct os_mbuf_pkthdr) +         \
                 MYNEWT_VAL(CONSOLE_TCP_TX_BUF_SIZE),                         \
             OS_ALIGNMENT)
/*
 * Maximum number of buffers handed to socket at once. Network stack may
 * keep some of them when client does not read, the rest is available for
 * new output (and dropping oldest output when needed).
 */
#define TX_CHUNK_MAX ((TX_BUF_COUNT + 1) / 2)

static struct os_event rx_receive_event;

static os_membuf_t tx_mem[OS_MEMPOOL_SIZE(TX_BUF_COUNT, TX_BLOCK_SIZE)];
static struct os_mempool tx_mempool;
static struct os_mbuf_pool tx_mbuf_pool;
static bool tx_pool_ready;

/*
 * Output not yet handed to socket, oldest first. Every buffer in chain is
 * a packet header mbuf from tx_mbuf_pool so any of them can become head
 * of the chain. Modified only inside critical section since output can
 * come from any task (or interrupt) while tcpip thread sends it.
 */
static struct os_mbuf *tx_head;
static struct os_mbuf *tx_tail;
/* Number of bytes dropped since last marker was sent */
static uint32_t tx_dropped;
/* Flush callback is queued in tcpip thread */
static bool tx_flush_pending;
/* Socket did not accept data, wait for writable callback */
static bool tx_blocked;

static struct mn_socket *server_socket;
static struct mn_socket *console_socket;

static void
tcp_console_pool_init(void)
{
    int rc;

    rc = os_mempool_init(&tx_mempool, TX_BUF_COUNT, TX_BLOCK_SIZE, tx_mem,
                         "tcp_console");
    assert(rc == 0);
    rc = os_mbuf_pool_init(&tx_mbuf_pool, &tx_mempool, TX_BLOCK_SIZE, TX_BUF_COUNT);
    assert(rc == 0);
    tx_pool_ready = true;
}

static void
tcp_console_buf_reset(struct os_mbuf *m)
{
    m->om_data = m->om_databuf + m->om_pkthdr_len;
    m->om_len = 0;
    SLIST_NEXT(m, om_next) = NULL;
}

/*
 * Gets empty buffer for new output. When pool is exhausted, oldest queued
 * buffer is reused and its content is counted as dropped.
 * Must be called inside critical section.
 */
static struct os_mbuf *
tcp_console_buf_get(void)
{
    struct os_mbuf *m;

    m = os_mbuf_get_pkthdr(&tx_mbuf_pool, 0);
    if (m == NULL && tx_head != NULL) {
        m = tx_head;
        tx_head = SLIST_NEXT(m, om_next);
        if (tx_head == NULL) {
            tx_tail = NULL;
        }
        tx_dropped += m->om_len;
        tcp_console_buf_reset(m);
    }

    return m;
}

static void
tcp_console_flush_cb(void *ctx)
{
    struct os_mbuf *head;
    struct os_mbuf *tail;
    struct os_mbuf *marker;
    uint32_t dropped;
    uint16_t len;
    int space;
    int n;
    int sr;
    int rc;

    OS_ENTER_CRITICAL(sr);
    tx_flush_pending = false;
    if (console_socket == NULL || tx_head == NULL) {
        OS_EXIT_CRITICAL(sr);
        return;
    }
    /* Detach up to TX_CHUNK_MAX oldest buffers */
    head = tx_head;
    tail = head;
    len = head->om_len;
    for (n = 1; n < TX_CHUNK_MAX && SLIST_NEXT(tail, om_next); ++n) {
        tail = SLIST_NEXT(tail, om_next);
        len += tail->om_len;
    }
    tx_head = SLIST_NEXT(tail, om_next);
    if (tx_head == NULL) {
        tx_tail = NULL;
    }
    SLIST_NEXT(tail, om_next) = NULL;
    dropped = tx_dropped;
    tx_dropped = 0;
    OS_EXIT_CRITICAL(sr);

    marker = NULL;
    if (dropped) {
        /* Tell client where output is missing */
        marker = os_mbuf_get_pkthdr(&tx_mbuf_pool, 0);
        if (marker == NULL) {
            /* No free buffer, oldest output is replaced by marker */
            marker = head;
            head = SLIST_NEXT(head, om_next);
            dropped += marker->om_len;
            len -= marker->om_len;
            tcp_console_buf_reset(marker);
        }
        space = OS_MBUF_TRAILINGSPACE(marker);
        n = snprintf((char *)marker->om_data, space,
                     "\r\n[tcp_console: %lu bytes dropped]\r\n",
                     (unsigned long)dropped);
        if (n < 0) {
            n = 0;
        } else if (n >= space) {
            n = space - 1;
        }
        marker->om_len = n;
        len += n;
        SLIST_NEXT(marker, om_next) = head;
        head = marker;
    }

    OS_MBUF_PKTHDR(head)->omp_len = len;
    rc = mn_sendto(console_socket, head, NULL);
    if (rc == MN_EAGAIN) {
        /* Socket still busy, put data back and wait for writable event */
        if (marker) {
            head = SLIST_NEXT(marker, om_next);
            os_mbuf_free(marker);
        }
        OS_ENTER_CRITICAL(sr);
        tx_blocked = true;
        tx_dropped += dropped;
        if (head) {
            SLIST_NEXT(tail, om_next) = tx_head;
            tx_head = head;
            if (tx_tail == NULL) {
                tx_tail = tail;
            }
        }
        OS_EXIT_CRITICAL(sr);
    } else if (rc) {
        os_mbuf_free_chain(head);
    }
}

static void
tcp_console_schedule_tx_flush(void)
{
    bool schedule;
    int sr;

    OS_ENTER_CRITICAL(sr);
    schedule = console_socket != NULL && !tx_blocked && !tx_flush_pending;
    if (schedule) {
        tx_flush_pending = true;
    }
    OS_EXIT_CRITICAL(sr);

    if (schedule && tcpip_try_callback(tcp_console_flush_cb, NULL)) {
        tx_flush_pending = false;
    }
}

static void
tcp_console_write(int c)
{
    struct os_mbuf *m;
    int sr;

    OS_ENTER_CRITICAL(sr);
    if (!tx_pool_ready) {
        /* Output may come before package init */
        tcp_console_pool_init();
    }
    m = tx_tail;
    if (m == NULL || OS_MBUF_TRAILINGSPACE(m) == 0) {
        m = tcp_console_buf_get();
        if (m == NULL) {
            /* All buffers are owned by socket */
            tx_dropped++;
            OS_EXIT_CRITICAL(sr);
            return;
        }
        if (tx_tail != NULL) {
            SLIST_NEXT(tx_tail, om_next) = m;
        } else {
            tx_head = m;
        }
        tx_tail = m;
    }
    m->om_data[m->om_len++] = (uint8_t)c;
    OS_EXIT_CRITICAL(sr);
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
        tx_blocked = false;
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
    tx_blocked = false;
    /* Send output collected while no client was connected */
    tcp_console_schedule_tx_flush();

    return 0;
}

static const union mn_socket_cb server_listen_cbs = {
    .listen.newconn = tcp_console_newconn,
};

int
tcp_console_pkg_init(void)
{
    struct mn_sockaddr_in sin = {0};
    int sr;
    int rc;

    rx_receive_event.ev_cb = rx_ev_cb;

    OS_ENTER_CRITICAL(sr);
    if (!tx_pool_ready) {
        tcp_console_pool_init();
    }
    OS_EXIT_CRITICAL(sr);

    /* Create normal TCP socket */
    rc = mn_socket(&server_socket, MN_PF_INET, MN_SOCK_STREAM, 0);
    assert(rc == 0);

    sin.msin_len = sizeof(sin);
    sin.msin_family = MN_AF_INET;
    sin.msin_port = htons(MYNEWT_VAL(CONSOLE_TCP_PORT));

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
