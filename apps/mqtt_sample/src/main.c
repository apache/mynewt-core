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
#include "console/console.h"
#include "mn_socket/mn_socket.h"
#include "core_mqtt.h"
#include "coremqtt/coremqtt.h"
#include "coremqtt/mqtt_mn_socket.h"

#define MQTT_SAMPLE_TOPIC              MYNEWT_VAL(MQTT_SAMPLE_TOPIC)
#define MQTT_SAMPLE_CLIENT_ID          MYNEWT_VAL(MQTT_SAMPLE_CLIENT_ID)
#define MQTT_SAMPLE_CONNECT_TIMEOUT_MS 5000
#define MQTT_SAMPLE_RETRY_DELAY_MS     5000
/* Number of QoS 1 publishes that can be in flight at a time */
#define MQTT_SAMPLE_QOS_RECORDS        4

static struct os_task mqtt_task;
static os_stack_t mqtt_task_stack[MYNEWT_VAL(MQTT_SAMPLE_STACK_SIZE)];

static NetworkContext_t mqtt_net;
static MQTTContext_t mqtt_ctx;
static uint8_t mqtt_buf[MYNEWT_VAL(MQTT_SAMPLE_BUF_SIZE)];
static MQTTPubAckInfo_t mqtt_outgoing[MQTT_SAMPLE_QOS_RECORDS];
static MQTTPubAckInfo_t mqtt_incoming[MQTT_SAMPLE_QOS_RECORDS];

static bool
mqtt_sample_event(MQTTContext_t *ctx, MQTTPacketInfo_t *pkt,
                  MQTTDeserializedInfo_t *info, MQTTSuccessFailReasonCode_t *reason,
                  MQTTPropBuilder_t *send_props, MQTTPropBuilder_t *get_props)
{
    MQTTPublishInfo_t *pub;

    switch (pkt->type & 0xF0) {
    case MQTT_PACKET_TYPE_PUBLISH:
        pub = info->pPublishInfo;
        console_printf("received on '%.*s': %.*s\n", (int)pub->topicNameLength,
                       pub->pTopicName, (int)pub->payloadLength,
                       (const char *)pub->pPayload);
        break;
    case MQTT_PACKET_TYPE_SUBACK:
        console_printf("subscribed to '%s'\n", MQTT_SAMPLE_TOPIC);
        break;
    case MQTT_PACKET_TYPE_PUBACK:
        console_printf("publish %u acknowledged\n", info->packetIdentifier);
        break;
    default:
        break;
    }

    return true;
}

static int
mqtt_sample_net_connect(void)
{
    struct mn_sockaddr_in sin;
    int rc;

    memset(&sin, 0, sizeof(sin));
    sin.msin_len = sizeof(sin);
    sin.msin_family = MN_AF_INET;
    sin.msin_port = htons(MYNEWT_VAL(MQTT_SAMPLE_BROKER_PORT));
    if (mn_inet_pton(MN_AF_INET, MYNEWT_VAL(MQTT_SAMPLE_BROKER_ADDR),
                     &sin.msin_addr) != 1) {
        console_printf("invalid broker address\n");
        return -1;
    }

    rc = mqtt_mn_socket_connect(&mqtt_net, (struct mn_sockaddr *)&sin,
                                MQTT_SAMPLE_CONNECT_TIMEOUT_MS);
    if (rc) {
        console_printf("TCP connect to %s:%d failed: %d\n",
                       MYNEWT_VAL(MQTT_SAMPLE_BROKER_ADDR),
                       MYNEWT_VAL(MQTT_SAMPLE_BROKER_PORT), rc);
    }

    return rc;
}

static MQTTStatus_t
mqtt_sample_mqtt_connect(void)
{
    TransportInterface_t transport;
    MQTTFixedBuffer_t buf;
    MQTTConnectInfo_t conn;
    MQTTSubscribeInfo_t sub;
    MQTTStatus_t status;
    bool session_present;

    mqtt_mn_socket_transport(&mqtt_net, &transport);
    buf.pBuffer = mqtt_buf;
    buf.size = sizeof(mqtt_buf);

    status = MQTT_Init(&mqtt_ctx, &transport, coremqtt_time_ms,
                       mqtt_sample_event, &buf);
    if (status != MQTTSuccess) {
        return status;
    }

    status = MQTT_InitStatefulQoS(&mqtt_ctx, mqtt_outgoing, MQTT_SAMPLE_QOS_RECORDS,
                                  mqtt_incoming, MQTT_SAMPLE_QOS_RECORDS, NULL, 0);
    if (status != MQTTSuccess) {
        return status;
    }

    memset(&conn, 0, sizeof(conn));
    conn.cleanSession = true;
    conn.keepAliveSeconds = MYNEWT_VAL(MQTT_SAMPLE_KEEP_ALIVE_S);
    conn.pClientIdentifier = MQTT_SAMPLE_CLIENT_ID;
    conn.clientIdentifierLength = strlen(MQTT_SAMPLE_CLIENT_ID);

    status = MQTT_Connect(&mqtt_ctx, &conn, NULL, MQTT_SAMPLE_CONNECT_TIMEOUT_MS,
                          &session_present, NULL, NULL);
    if (status != MQTTSuccess) {
        return status;
    }
    console_printf("connected to %s:%d\n", MYNEWT_VAL(MQTT_SAMPLE_BROKER_ADDR),
                   MYNEWT_VAL(MQTT_SAMPLE_BROKER_PORT));

    memset(&sub, 0, sizeof(sub));
    sub.qos = MQTTQoS1;
    sub.pTopicFilter = MQTT_SAMPLE_TOPIC;
    sub.topicFilterLength = strlen(MQTT_SAMPLE_TOPIC);
    sub.retainHandlingOption = retainSendOnSub;

    return MQTT_Subscribe(&mqtt_ctx, &sub, 1, MQTT_GetPacketId(&mqtt_ctx), NULL);
}

static MQTTStatus_t
mqtt_sample_publish(unsigned int count)
{
    MQTTPublishInfo_t pub;
    char payload[32];
    int len;

    len = snprintf(payload, sizeof(payload), "hello %u", count);

    memset(&pub, 0, sizeof(pub));
    pub.qos = MQTTQoS1;
    pub.pTopicName = MQTT_SAMPLE_TOPIC;
    pub.topicNameLength = strlen(MQTT_SAMPLE_TOPIC);
    pub.pPayload = payload;
    pub.payloadLength = len;

    return MQTT_Publish(&mqtt_ctx, &pub, MQTT_GetPacketId(&mqtt_ctx), NULL);
}

static void
mqtt_sample_session(void)
{
    MQTTStatus_t status;
    unsigned int count;
    uint32_t next_pub;

    if (mqtt_sample_net_connect()) {
        return;
    }

    status = mqtt_sample_mqtt_connect();
    count = 0;
    next_pub = coremqtt_time_ms();

    while (status == MQTTSuccess || status == MQTTNeedMoreBytes) {
        if ((int32_t)(coremqtt_time_ms() - next_pub) >= 0) {
            next_pub += MYNEWT_VAL(MQTT_SAMPLE_PUBLISH_INTERVAL_MS);
            status = mqtt_sample_publish(count++);
            if (status != MQTTSuccess) {
                break;
            }
        }

        /* Waits for incoming data up to the transport receive timeout */
        status = MQTT_ProcessLoop(&mqtt_ctx);
    }

    console_printf("MQTT session ended: %s\n", MQTT_Status_strerror(status));

    if (mqtt_ctx.connectStatus == MQTTConnected) {
        MQTT_Disconnect(&mqtt_ctx, NULL, NULL);
    }
    mqtt_mn_socket_disconnect(&mqtt_net);
}

static void
mqtt_task_handler(void *arg)
{
    while (1) {
        mqtt_sample_session();
        os_time_delay(os_time_ms_to_ticks32(MQTT_SAMPLE_RETRY_DELAY_MS));
    }
}

int
mynewt_main(int argc, char **argv)
{
    sysinit();

    os_task_init(&mqtt_task, "mqtt", mqtt_task_handler, NULL,
                 MYNEWT_VAL(MQTT_SAMPLE_PRIO), OS_WAIT_FOREVER,
                 mqtt_task_stack, MYNEWT_VAL(MQTT_SAMPLE_STACK_SIZE));

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return 0;
}
