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
#include "testutil/testutil.h"
#include "mn_socket/mn_socket.h"
#include "core_mqtt.h"
#include "coremqtt/coremqtt.h"
#include "coremqtt/mqtt_mn_socket.h"
#include "coremqtt_test.h"

#define TEST_TOPIC      "mynewt/test"
#define TEST_TIMEOUT_MS 2000
#define TEST_LARGE_LEN  3000

static NetworkContext_t test_net;
static MQTTContext_t test_ctx;
static uint8_t test_buf[TEST_LARGE_LEN + 64];
static MQTTPubAckInfo_t test_outgoing[2];
static MQTTPubAckInfo_t test_incoming[2];
static uint8_t test_payload[TEST_LARGE_LEN];

static struct {
    int suback;
    int puback;
    int rx;
    uint8_t payload[TEST_LARGE_LEN];
    size_t len;
} test_ev;

static bool
test_event(MQTTContext_t *ctx, MQTTPacketInfo_t *pkt,
           MQTTDeserializedInfo_t *info, MQTTSuccessFailReasonCode_t *reason,
           MQTTPropBuilder_t *send_props, MQTTPropBuilder_t *get_props)
{
    MQTTPublishInfo_t *pub;

    switch (pkt->type & 0xF0) {
    case MQTT_PACKET_TYPE_SUBACK:
        test_ev.suback++;
        break;
    case MQTT_PACKET_TYPE_PUBACK:
        test_ev.puback++;
        break;
    case MQTT_PACKET_TYPE_PUBLISH:
        pub = info->pPublishInfo;
        TEST_ASSERT(pub->topicNameLength == strlen(TEST_TOPIC));
        TEST_ASSERT(memcmp(pub->pTopicName, TEST_TOPIC, pub->topicNameLength) == 0);
        TEST_ASSERT_FATAL(pub->payloadLength <= sizeof(test_ev.payload));
        memcpy(test_ev.payload, pub->pPayload, pub->payloadLength);
        test_ev.len = pub->payloadLength;
        test_ev.rx++;
        break;
    default:
        break;
    }

    return true;
}

static int
test_net_connect(uint16_t port)
{
    struct mn_sockaddr_in sin;

    memset(&sin, 0, sizeof(sin));
    sin.msin_len = sizeof(sin);
    sin.msin_family = MN_AF_INET;
    sin.msin_port = htons(port);
    mn_inet_pton(MN_AF_INET, "127.0.0.1", &sin.msin_addr);

    return mqtt_mn_socket_connect(&test_net, (struct mn_sockaddr *)&sin,
                                  TEST_TIMEOUT_MS);
}

static MQTTStatus_t
test_mqtt_connect(void)
{
    TransportInterface_t transport;
    MQTTFixedBuffer_t buf;
    MQTTConnectInfo_t conn;
    MQTTStatus_t status;
    bool session_present;
    int rc;

    memset(&test_ev, 0, sizeof(test_ev));

    rc = test_net_connect(TEST_BROKER_PORT);
    TEST_ASSERT_FATAL(rc == 0, "connect rc=%d", rc);

    mqtt_mn_socket_transport(&test_net, &transport);
    buf.pBuffer = test_buf;
    buf.size = sizeof(test_buf);

    status = MQTT_Init(&test_ctx, &transport, coremqtt_time_ms, test_event, &buf);
    TEST_ASSERT_FATAL(status == MQTTSuccess);
    status = MQTT_InitStatefulQoS(&test_ctx, test_outgoing, 2, test_incoming,
                                  2, NULL, 0);
    TEST_ASSERT_FATAL(status == MQTTSuccess);

    memset(&conn, 0, sizeof(conn));
    conn.cleanSession = true;
    conn.keepAliveSeconds = 60;
    conn.pClientIdentifier = "test";
    conn.clientIdentifierLength = 4;

    return MQTT_Connect(&test_ctx, &conn, NULL, TEST_TIMEOUT_MS,
                        &session_present, NULL, NULL);
}

static void
test_mqtt_disconnect(void)
{
    MQTTStatus_t status;

    status = MQTT_Disconnect(&test_ctx, NULL, NULL);
    TEST_ASSERT(status == MQTTSuccess);
    mqtt_mn_socket_disconnect(&test_net);
    test_broker_wait_idle();
    TEST_ASSERT(test_broker.err == 0);
}

static void
test_process_until(int *cnt, int target)
{
    MQTTStatus_t status;
    uint32_t start;

    start = coremqtt_time_ms();
    while (*cnt < target) {
        status = MQTT_ProcessLoop(&test_ctx);
        TEST_ASSERT_FATAL(status == MQTTSuccess || status == MQTTNeedMoreBytes,
                          "status=%d", status);
        TEST_ASSERT_FATAL(coremqtt_time_ms() - start < TEST_TIMEOUT_MS);
    }
}

static void
test_subscribe(void)
{
    MQTTSubscribeInfo_t sub;
    MQTTStatus_t status;

    memset(&sub, 0, sizeof(sub));
    sub.qos = MQTTQoS1;
    sub.pTopicFilter = TEST_TOPIC;
    sub.topicFilterLength = strlen(TEST_TOPIC);

    status = MQTT_Subscribe(&test_ctx, &sub, 1, MQTT_GetPacketId(&test_ctx), NULL);
    TEST_ASSERT_FATAL(status == MQTTSuccess);
    test_process_until(&test_ev.suback, 1);
}

static void
test_publish(const void *payload, size_t len)
{
    MQTTPublishInfo_t pub;
    MQTTStatus_t status;

    memset(&pub, 0, sizeof(pub));
    pub.qos = MQTTQoS1;
    pub.pTopicName = TEST_TOPIC;
    pub.topicNameLength = strlen(TEST_TOPIC);
    pub.pPayload = payload;
    pub.payloadLength = len;

    status = MQTT_Publish(&test_ctx, &pub, MQTT_GetPacketId(&test_ctx), NULL);
    TEST_ASSERT_FATAL(status == MQTTSuccess);
}

static void
coremqtt_test_connect_refused(void)
{
    int rc;

    /* Nothing listens on this port */
    rc = test_net_connect(TEST_BROKER_PORT + 1);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(test_net.sock == NULL);
}

static void
coremqtt_test_pubsub(void)
{
    TEST_ASSERT_FATAL(test_mqtt_connect() == MQTTSuccess);
    TEST_ASSERT(test_broker.connects == 1);

    test_subscribe();

    test_publish("hello", 5);
    test_process_until(&test_ev.rx, 1);
    test_process_until(&test_ev.puback, 1);
    TEST_ASSERT(test_ev.len == 5);
    TEST_ASSERT(memcmp(test_ev.payload, "hello", 5) == 0);
    TEST_ASSERT(test_broker.publishes == 1);

    test_mqtt_disconnect();
    TEST_ASSERT(test_broker.disconnects == 1);
}

static void
coremqtt_test_large_publish(void)
{
    size_t i;

    for (i = 0; i < sizeof(test_payload); i++) {
        test_payload[i] = i * 7;
    }

    TEST_ASSERT_FATAL(test_mqtt_connect() == MQTTSuccess);
    test_subscribe();

    /* Spans several mbufs and several socket reads in both directions */
    test_publish(test_payload, sizeof(test_payload));
    test_process_until(&test_ev.rx, 1);
    test_process_until(&test_ev.puback, 1);
    TEST_ASSERT(test_ev.len == sizeof(test_payload));
    TEST_ASSERT(memcmp(test_ev.payload, test_payload, sizeof(test_payload)) == 0);

    test_mqtt_disconnect();
}

static void
coremqtt_test_ping(void)
{
    MQTTStatus_t status;
    uint32_t start;

    TEST_ASSERT_FATAL(test_mqtt_connect() == MQTTSuccess);

    status = MQTT_Ping(&test_ctx);
    TEST_ASSERT_FATAL(status == MQTTSuccess);
    TEST_ASSERT(test_ctx.waitingForPingResp);

    start = coremqtt_time_ms();
    while (test_ctx.waitingForPingResp) {
        status = MQTT_ProcessLoop(&test_ctx);
        TEST_ASSERT_FATAL(status == MQTTSuccess);
        TEST_ASSERT_FATAL(coremqtt_time_ms() - start < TEST_TIMEOUT_MS);
    }
    TEST_ASSERT(test_broker.pings == 1);

    test_mqtt_disconnect();
}

static void
coremqtt_test_recv_timeout(void)
{
    MQTTStatus_t status;
    uint32_t start;
    uint32_t elapsed;

    TEST_ASSERT_FATAL(test_mqtt_connect() == MQTTSuccess);

    /* With nothing to receive, the process loop waits for the timeout */
    mqtt_mn_socket_set_recv_timeout(&test_net, 200);
    start = coremqtt_time_ms();
    status = MQTT_ProcessLoop(&test_ctx);
    elapsed = coremqtt_time_ms() - start;
    TEST_ASSERT(status == MQTTSuccess);
    TEST_ASSERT(elapsed >= 150 && elapsed < 1000, "elapsed=%u", (unsigned)elapsed);

    /* Without a timeout it returns right away */
    mqtt_mn_socket_set_recv_timeout(&test_net, 0);
    start = coremqtt_time_ms();
    status = MQTT_ProcessLoop(&test_ctx);
    elapsed = coremqtt_time_ms() - start;
    TEST_ASSERT(status == MQTTSuccess);
    TEST_ASSERT(elapsed < 100, "elapsed=%u", (unsigned)elapsed);

    test_mqtt_disconnect();
}

static void
coremqtt_test_broker_close(void)
{
    MQTTStatus_t status;

    /* Broker drops the connection instead of answering CONNECT */
    test_broker.close_on_connect = 1;
    status = test_mqtt_connect();
    test_broker.close_on_connect = 0;
    TEST_ASSERT(status != MQTTSuccess);
    TEST_ASSERT(test_ctx.connectStatus != MQTTConnected);

    /* Further use of the closed socket fails instead of blocking */
    TEST_ASSERT(mqtt_mn_socket_recv(&test_net, test_buf, 1) < 0);
    TEST_ASSERT(mqtt_mn_socket_send(&test_net, "x", 1) < 0);

    mqtt_mn_socket_disconnect(&test_net);
    test_broker_wait_idle();
}

TEST_CASE_TASK(coremqtt_test_transport)
{
    test_broker_start();

    coremqtt_test_connect_refused();
    coremqtt_test_pubsub();
    coremqtt_test_large_publish();
    coremqtt_test_ping();
    coremqtt_test_recv_timeout();
    coremqtt_test_broker_close();
}

TEST_SUITE(coremqtt_test_all)
{
    coremqtt_test_transport();
}

int
main(int argc, char **argv)
{
    coremqtt_test_all();
    return tu_any_failed;
}
