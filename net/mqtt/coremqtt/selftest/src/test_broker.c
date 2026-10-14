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
#include "coremqtt_test.h"

struct test_broker test_broker;

static void
test_broker_close(void)
{
    if (test_broker.conn) {
        mn_close(test_broker.conn);
        test_broker.conn = NULL;
    }
    test_broker.rx_len = 0;
}

static void
test_broker_send(const uint8_t *data, size_t len)
{
    struct os_mbuf *om;
    int rc;

    om = os_msys_get_pkthdr(len, 0);
    if (!om) {
        test_broker.err = MN_ENOBUFS;
        return;
    }
    if (os_mbuf_append(om, data, len)) {
        os_mbuf_free_chain(om);
        test_broker.err = MN_ENOBUFS;
        return;
    }

    rc = mn_sendto(test_broker.conn, om, NULL);
    if (rc) {
        if (rc == MN_EAGAIN) {
            os_mbuf_free_chain(om);
        }
        test_broker.err = rc;
    }
}

static void
test_broker_handle(const uint8_t *pkt, size_t hdr_len, size_t len)
{
    const uint8_t *var = pkt + hdr_len;
    uint8_t rsp[6];
    size_t topic_len;
    int qos;

    switch (pkt[0] & 0xF0) {
    case 0x10:
        /* CONNECT -> CONNACK, no session, success, no properties */
        test_broker.connects++;
        if (test_broker.close_on_connect) {
            test_broker_close();
            return;
        }
        rsp[0] = 0x20;
        rsp[1] = 3;
        rsp[2] = 0;
        rsp[3] = 0;
        rsp[4] = 0;
        test_broker_send(rsp, 5);
        break;
    case 0x80:
        /* SUBSCRIBE -> SUBACK granting QoS 1 */
        rsp[0] = 0x90;
        rsp[1] = 4;
        rsp[2] = var[0];
        rsp[3] = var[1];
        rsp[4] = 0;
        rsp[5] = 1;
        test_broker_send(rsp, 6);
        break;
    case 0x30:
        /* PUBLISH -> PUBACK (QoS 1), then deliver it back to the client */
        test_broker.publishes++;
        qos = (pkt[0] >> 1) & 0x03;
        if (qos) {
            topic_len = (var[0] << 8) | var[1];
            rsp[0] = 0x40;
            rsp[1] = 2;
            rsp[2] = var[2 + topic_len];
            rsp[3] = var[3 + topic_len];
            test_broker_send(rsp, 4);
        }
        test_broker_send(pkt, len);
        break;
    case 0xC0:
        /* PINGREQ -> PINGRESP */
        test_broker.pings++;
        rsp[0] = 0xD0;
        rsp[1] = 0;
        test_broker_send(rsp, 2);
        break;
    case 0xE0:
        test_broker.disconnects++;
        test_broker_close();
        break;
    case 0x40:
        /* PUBACK for a delivered message */
        break;
    default:
        test_broker.err = -1;
        test_broker_close();
        break;
    }
}

static void
test_broker_process(void)
{
    size_t rem_len;
    size_t total;
    size_t i;
    int shift;

    while (test_broker.conn && test_broker.rx_len >= 2) {
        rem_len = 0;
        shift = 0;
        i = 1;
        do {
            if (i >= test_broker.rx_len) {
                return;
            }
            rem_len |= (test_broker.rx[i] & 0x7F) << shift;
            shift += 7;
        } while (test_broker.rx[i++] & 0x80 && i < 5);

        total = i + rem_len;
        if (total > test_broker.rx_len) {
            return;
        }

        test_broker_handle(test_broker.rx, i, total);
        if (!test_broker.conn) {
            return;
        }

        test_broker.rx_len -= total;
        memmove(test_broker.rx, test_broker.rx + total, test_broker.rx_len);
    }
}

static void
test_broker_readable(void *arg, int err)
{
    struct os_mbuf *om;
    uint16_t len;
    int rc;

    while (test_broker.conn) {
        rc = mn_recvfrom(test_broker.conn, &om, NULL);
        if (rc == MN_EAGAIN) {
            break;
        }
        if (rc) {
            /* Client closed the connection */
            test_broker_close();
            break;
        }

        len = OS_MBUF_PKTLEN(om);
        if (test_broker.rx_len + len > sizeof(test_broker.rx)) {
            os_mbuf_free_chain(om);
            test_broker.err = MN_ENOBUFS;
            test_broker_close();
            break;
        }
        os_mbuf_copydata(om, 0, len, test_broker.rx + test_broker.rx_len);
        test_broker.rx_len += len;
        os_mbuf_free_chain(om);

        test_broker_process();
    }
}

static void
test_broker_writable(void *arg, int err)
{
}

static const union mn_socket_cb test_broker_conn_cbs = {
    .socket = {
        .readable = test_broker_readable,
        .writable = test_broker_writable,
    },
};

static int
test_broker_newconn(void *arg, struct mn_socket *new)
{
    if (test_broker.conn) {
        return -1;
    }

    test_broker.conn = new;
    test_broker.rx_len = 0;
    mn_socket_set_cbs(new, NULL, &test_broker_conn_cbs);

    return 0;
}

static const union mn_socket_cb test_broker_listen_cbs = {
    .listen = {
        .newconn = test_broker_newconn,
    },
};

void
test_broker_start(void)
{
    struct mn_sockaddr_in sin;
    int rc;

    memset(&test_broker, 0, sizeof(test_broker));

    rc = mn_socket(&test_broker.listen, MN_PF_INET, MN_SOCK_STREAM, 0);
    TEST_ASSERT_FATAL(rc == 0);
    mn_socket_set_cbs(test_broker.listen, NULL, &test_broker_listen_cbs);

    memset(&sin, 0, sizeof(sin));
    sin.msin_len = sizeof(sin);
    sin.msin_family = MN_AF_INET;
    sin.msin_port = htons(TEST_BROKER_PORT);
    mn_inet_pton(MN_AF_INET, "127.0.0.1", &sin.msin_addr);

    rc = mn_bind(test_broker.listen, (struct mn_sockaddr *)&sin);
    TEST_ASSERT_FATAL(rc == 0, "bind rc=%d", rc);

    rc = mn_listen(test_broker.listen, 2);
    TEST_ASSERT_FATAL(rc == 0, "listen rc=%d", rc);
}

void
test_broker_wait_idle(void)
{
    int i;

    for (i = 0; i < 100 && test_broker.conn; i++) {
        os_time_delay(os_time_ms_to_ticks32(20));
    }
    TEST_ASSERT_FATAL(test_broker.conn == NULL);
}
