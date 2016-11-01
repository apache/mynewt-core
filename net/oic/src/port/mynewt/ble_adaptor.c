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

#include <syscfg/syscfg.h>
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
#include <assert.h>
#include <os/os.h>
#include <string.h>
#include "oc_buffer.h"
#include "../oc_log.h"
#include "adaptor.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"


/* a custom service for COAP over GATT */
/* {e3f9f9c4-8a83-4055-b647-728b769745d6} */
const uint8_t gatt_svr_svc_coap[16] = {
    0xd6, 0x45, 0x97, 0x76, 0x8b, 0x72, 0x47, 0xb6,
    0x55, 0x40, 0x83, 0x8a, 0xc4, 0xf9, 0xf9, 0xe3,
};

/* {e467fee6-d6bb-4956-94df-0090350631f5} */
const uint8_t gatt_svr_chr_coap[16] = {
    0xf5, 0x31, 0x06, 0x35, 0x90, 0x00, 0xdf, 0x94,
    0x56, 0x49, 0xbb, 0xd6, 0xe6, 0xfe, 0x67, 0xe4,
};

/* queue to hold mbufs until we get called from oic */
struct os_mqueue ble_coap_mq;

#if (MYNEWT_VAL(OC_SERVER) == 1)
/* ble nmgr attr handle */
uint16_t g_ble_coap_attr_handle;

static int
gatt_svr_chr_access_coap(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /* Service: newtmgr */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = (void *)gatt_svr_svc_coap,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /* Characteristic: Write No Rsp */
            .uuid128 = (void *)gatt_svr_chr_coap,
            .access_cb = gatt_svr_chr_access_coap,
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &g_ble_coap_attr_handle,
        }, {
            0, /* No more characteristics in this service */
        } },
    },
    {
        0, /* No more services */
    },
};

static int
gatt_svr_chr_access_coap(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    struct os_mbuf *m;
    int rc;
    (void) attr_handle; /* no need to use this since we have onyl one attr
                         * tied to this callback */

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            m = ctxt->om;
            /* stick the conn handle at the end of the frame -- we will
             * pull it out later */
            rc = os_mbuf_append(m, &conn_handle, sizeof(conn_handle));
            if (rc) {
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            rc = os_mqueue_put(&ble_coap_mq, oc_evq_get(), m);
            if (rc) {
                return BLE_ATT_ERR_PREPARE_QUEUE_FULL;
            }

            /* tell nimble we are keeping the mbuf */
            ctxt->om = NULL;

            break;
        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
    }
    return 0;
}

oc_message_t *
oc_attempt_rx_gatt(void)
{
    int rc;
    struct os_mbuf *m = NULL;
    struct os_mbuf_pkthdr *pkt;
    uint16_t conn_handle;
    oc_message_t *message = NULL;

    LOG("oc_transport_gatt attempt rx\n");

    /* get an mbuf from the queue */
    m = os_mqueue_get(&ble_coap_mq);
    if (NULL == m) {
        ERROR("oc_transport_gatt: Woke for for receive but found no mbufs\n");
        goto rx_attempt_err;
    }

    if (!OS_MBUF_IS_PKTHDR(m)) {
        ERROR("oc_transport_gatt: received mbuf that wasn't a packet header\n");
        goto rx_attempt_err;
    }

    pkt = OS_MBUF_PKTHDR(m);

    LOG("oc_transport_gatt rx %p-%u\n", pkt, pkt->omp_len);
    /* get the conn handle from the end of the message */
    rc = os_mbuf_copydata(m, pkt->omp_len - sizeof(conn_handle),
                          sizeof(conn_handle), &conn_handle);
    if (rc != 0) {
        ERROR("Failed to retrieve conn_handle from mbuf \n");
        goto rx_attempt_err;
    }

    /* trim conn_handle from the end */
    os_mbuf_adj(m, - sizeof(conn_handle));

    message = oc_allocate_message();
    if (NULL == message) {
        ERROR("Could not allocate OC message buffer\n");
        goto rx_attempt_err;
    }

    if (pkt->omp_len > MAX_PAYLOAD_SIZE) {
        ERROR("Message to large for OC message buffer\n");
        goto rx_attempt_err;
    }
    /* copy to message from mbuf chain */
    rc = os_mbuf_copydata(m, 0, pkt->omp_len, message->data);
    if (rc != 0) {
        ERROR("Failed to copy message from mbuf to OC message buffer \n");
        goto rx_attempt_err;
    }

    os_mbuf_free_chain(m);
    message->endpoint.flags = GATT;
    message->endpoint.bt_addr.conn_handle = conn_handle;
    message->length = pkt->omp_len;
    LOG("Successfully rx length %lu\n", (unsigned long)message->length);
    return message;

    /* add the addr info to the message */
rx_attempt_err:
    if (m) {
        os_mbuf_free_chain(m);
    }
    if (message) {
        oc_message_unref(message);
    }
    return NULL;
}
#endif

int
ble_coap_gatt_srv_init(void)
{
#if (MYNEWT_VAL(OC_SERVER) == 1)
    int rc;
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }
#endif

    return 0;
}

static void
oc_event_gatt(struct os_event *ev)
{
    oc_message_t *pmsg;

    while ((pmsg = oc_attempt_rx_gatt()) != NULL) {
        oc_network_event(pmsg);
    }
}

int
oc_connectivity_init_gatt(void)
{
    os_mqueue_init(&ble_coap_mq, oc_event_gatt, NULL);
    return 0;
}

void
oc_connectivity_shutdown_gatt(void)
{
    /* there is not unregister for BLE */
}

void
oc_send_buffer_gatt(oc_message_t *message)
{
    struct os_mbuf *m = NULL;
    int rc;

    /* get a packet header */
    m = os_msys_get_pkthdr(0, 0);
    if (m == NULL) {
        ERROR("oc_transport_gatt: No mbuf available\n");
        goto err;
    }

    /* add this data to the mbuf */
    rc = os_mbuf_append(m, message->data, message->length);
    if (rc != 0) {
        ERROR("oc_transport_gatt: could not append data \n");
        goto err;
    }

#if (MYNEWT_VAL(OC_CLIENT) == 1)
    ERROR("send not supported on client");
#endif

#if (MYNEWT_VAL(OC_SERVER) == 1)
    ble_gattc_notify_custom(message->endpoint.bt_addr.conn_handle,
                            g_ble_coap_attr_handle, m);
    m = NULL;
#endif

err:
    if (m) {
        os_mbuf_free_chain(m);
    }
    oc_message_unref(message);
    return;
}

void
oc_send_buffer_gatt_mcast(oc_message_t *message)
{
#if (MYNEWT_VAL(OC_CLIENT) == 1)
    ERROR("send not supported on client");
#elif (MYNEWT_VAL(OC_SERVER) == 1)
    oc_message_unref(message);
    ERROR("oc_transport_gatt: no multicast support for server only system \n");
#endif
}

#endif
