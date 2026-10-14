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
#include "mn_socket/mn_socket.h"
#include "coremqtt/mqtt_mn_socket.h"

/*
 * Socket callbacks run in the socket provider's context; they only record
 * errors and wake the task that uses the connection. A semaphore is
 * released only if it has no token, so the count stays bounded.
 */
static void
mqtt_mn_socket_wake(struct os_sem *sem)
{
    if (os_sem_get_count(sem) == 0) {
        os_sem_release(sem);
    }
}

static void
mqtt_mn_socket_readable(void *arg, int err)
{
    NetworkContext_t *ctx = arg;

    if (err) {
        ctx->err = err;
    }
    mqtt_mn_socket_wake(&ctx->rx_sem);
}

static void
mqtt_mn_socket_writable(void *arg, int err)
{
    NetworkContext_t *ctx = arg;

    if (err) {
        ctx->err = err;
    }
    mqtt_mn_socket_wake(&ctx->tx_sem);
}

static const union mn_socket_cb mqtt_mn_socket_cbs = {
    .socket = {
        .readable = mqtt_mn_socket_readable,
        .writable = mqtt_mn_socket_writable,
    },
};

int
mqtt_mn_socket_connect(NetworkContext_t *ctx, const struct mn_sockaddr *addr,
                       uint32_t timeout_ms)
{
    uint8_t domain;
    int rc;

    memset(ctx, 0, sizeof(*ctx));
    os_sem_init(&ctx->rx_sem, 0);
    os_sem_init(&ctx->tx_sem, 0);
    ctx->recv_timeout_ms = MYNEWT_VAL(COREMQTT_MN_SOCKET_RECV_TIMEOUT_MS);

    switch (addr->msa_family) {
    case MN_AF_INET:
        domain = MN_PF_INET;
        break;
    case MN_AF_INET6:
        domain = MN_PF_INET6;
        break;
    default:
        return MN_EAFNOSUPPORT;
    }

    rc = mn_socket(&ctx->sock, domain, MN_SOCK_STREAM, 0);
    if (rc) {
        ctx->sock = NULL;
        return rc;
    }
    mn_socket_set_cbs(ctx->sock, ctx, &mqtt_mn_socket_cbs);

    /* Connection result is reported through the writable callback */
    rc = mn_connect(ctx->sock, (struct mn_sockaddr *)addr);
    if (rc == 0) {
        rc = os_sem_pend(&ctx->tx_sem, os_time_ms_to_ticks32(timeout_ms));
        if (rc == OS_TIMEOUT) {
            rc = MN_ETIMEDOUT;
        } else {
            rc = ctx->err;
        }
    }

    if (rc) {
        mqtt_mn_socket_disconnect(ctx);
    }

    return rc;
}

void
mqtt_mn_socket_disconnect(NetworkContext_t *ctx)
{
    if (ctx->sock) {
        mn_close(ctx->sock);
        ctx->sock = NULL;
    }
    if (ctx->rx_om) {
        os_mbuf_free_chain(ctx->rx_om);
        ctx->rx_om = NULL;
    }
}

void
mqtt_mn_socket_set_recv_timeout(NetworkContext_t *ctx, uint32_t timeout_ms)
{
    ctx->recv_timeout_ms = timeout_ms;
}

void
mqtt_mn_socket_transport(NetworkContext_t *ctx, TransportInterface_t *ti)
{
    memset(ti, 0, sizeof(*ti));
    ti->recv = mqtt_mn_socket_recv;
    ti->send = mqtt_mn_socket_send;
    ti->writev = mqtt_mn_socket_writev;
    ti->pNetworkContext = ctx;
}

int32_t
mqtt_mn_socket_recv(NetworkContext_t *ctx, void *buf, size_t len)
{
    struct os_mbuf *om;
    uint16_t pktlen;
    int rc;

    if (!ctx->sock) {
        return -1;
    }

    if (!ctx->rx_om) {
        rc = mn_recvfrom(ctx->sock, &om, NULL);
        if (rc == MN_EAGAIN && ctx->recv_timeout_ms && !ctx->err) {
            os_sem_pend(&ctx->rx_sem, os_time_ms_to_ticks32(ctx->recv_timeout_ms));
            rc = mn_recvfrom(ctx->sock, &om, NULL);
        }
        if (rc == MN_EAGAIN) {
            /* Nothing received yet; not an error unless the socket failed */
            return ctx->err ? -1 : 0;
        }
        if (rc) {
            /* Connection closed or failed, fail any further use */
            ctx->err = rc;
            return -1;
        }
        ctx->rx_om = om;
    }

    pktlen = OS_MBUF_PKTLEN(ctx->rx_om);
    if (len > pktlen) {
        len = pktlen;
    }

    os_mbuf_copydata(ctx->rx_om, 0, len, buf);
    os_mbuf_adj(ctx->rx_om, len);

    if (OS_MBUF_PKTLEN(ctx->rx_om) == 0) {
        os_mbuf_free_chain(ctx->rx_om);
        ctx->rx_om = NULL;
    }

    return len;
}

int32_t
mqtt_mn_socket_send(NetworkContext_t *ctx, const void *buf, size_t len)
{
    TransportOutVector_t iov = {
        .iov_base = buf,
        .iov_len = len,
    };

    return mqtt_mn_socket_writev(ctx, &iov, 1);
}

static int
mqtt_mn_socket_append(struct os_mbuf *om, const uint8_t *data, size_t len)
{
    uint16_t chunk;
    int rc;

    while (len > 0) {
        chunk = len > UINT16_MAX ? UINT16_MAX : len;
        rc = os_mbuf_append(om, data, chunk);
        if (rc) {
            return rc;
        }
        data += chunk;
        len -= chunk;
    }

    return 0;
}

int32_t
mqtt_mn_socket_writev(NetworkContext_t *ctx, TransportOutVector_t *iov, size_t iov_cnt)
{
    struct os_mbuf *om;
    size_t total;
    size_t i;
    int rc;

    if (!ctx->sock || ctx->err) {
        return -1;
    }

    total = 0;
    for (i = 0; i < iov_cnt; i++) {
        total += iov[i].iov_len;
    }
    if (total == 0) {
        return 0;
    }
    if (total > INT32_MAX) {
        return -1;
    }

    /* coreMQTT retries sends returning 0 until MQTT_SEND_TIMEOUT_MS */
    om = os_msys_get_pkthdr(total > UINT16_MAX ? UINT16_MAX : total, 0);
    if (!om) {
        os_time_delay(1);
        return 0;
    }

    for (i = 0; i < iov_cnt; i++) {
        rc = mqtt_mn_socket_append(om, iov[i].iov_base, iov[i].iov_len);
        if (rc) {
            os_mbuf_free_chain(om);
            os_time_delay(1);
            return 0;
        }
    }

    rc = mn_sendto(ctx->sock, om, NULL);
    if (rc == 0) {
        return total;
    }

    if (rc == MN_EAGAIN) {
        /* Previous data still queued in the socket, wait until it drains */
        os_mbuf_free_chain(om);
        os_sem_pend(&ctx->tx_sem,
                    os_time_ms_to_ticks32(MYNEWT_VAL(COREMQTT_SEND_TIMEOUT_MS)));
        return 0;
    }

    /*
     * Socket providers differ on mbuf ownership after a failed stream
     * send, so leave it to the provider (it is released on close).
     */
    return -1;
}
