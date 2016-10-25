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

static int
blecoap_gap_event(struct ble_gap_event *event, void *arg);

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
            rc = os_mqueue_put(&ble_coap_mq, &oc_event_q, m);
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

static int
oc_gatt_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    /*
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o 128 bit UUID
     */

    memset(&fields, 0, sizeof fields);

    /* Indicate that the flags field should be included; specify a value of 0
     * to instruct the stack to fill the value in for us.
     */
    fields.flags_is_present = 1;
    fields.flags = 0;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this one automatically as well.  This is done by assigning
     * the special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.uuids128 = (void *)gatt_svr_svc_coap;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        return rc;
    }

    memset(&fields, 0, sizeof fields);
    fields.name = (uint8_t *)ble_svc_gap_device_name();
    fields.name_len = strlen((char *)fields.name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&fields);
    if (rc != 0) {
        return rc;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_ADDR_TYPE_PUBLIC, 0, NULL, BLE_HS_FOREVER,
                           &adv_params, blecoap_gap_event, NULL);
    return rc;
}
#endif

#if (MYNEWT_VAL(OC_CLIENT) == 1)
static char *
addr_str(const void *addr)
{
    static char buf[6 * 2 + 5 + 1];
    const uint8_t *u8p;

    u8p = addr;
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
            u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);

    return buf;
}


/**
 * Indicates whether we should tre to connect to the sender of the specified
 * advertisement.  The function returns a positive result if the device
 * advertises connectability and support for the Alert Notification service.
 */
static int
oc_gatt_should_connect(const struct ble_gap_disc_desc *disc)
{
    int i;

    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
        disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {

        return 0;
    }

    /* The device has to advertise support for the COAP service
     */
    for (i = 0; i < disc->fields->num_uuids128; i++) {
        char *ptr = ((char*) disc->fields->uuids128) + 16 * i;
        if (memcmp(ptr, gatt_svr_svc_coap, sizeof(gatt_svr_svc_coap)) == 0) {
            return 1;
        }
    }

    return 0;
}

/**
 * Connects to the sender of the specified advertisement of it looks
 * interesting.  A device is "interesting" if it advertises connectability and
 * support for the Alert Notification service.
 */
static void
oc_gatt_connect_if_interesting(const struct ble_gap_disc_desc *disc)
{
    int rc;

    /* Don't do anything if we don't care about this advertiser. */
    if (!oc_gatt_should_connect(disc)) {
        return;
    }

    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        ERROR("Failed to cancel scan; rc=%d\n", rc);
        return;
    }

    /* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for
     * timeout.
     */
    rc = ble_gap_connect(BLE_ADDR_TYPE_PUBLIC, disc->addr_type, disc->addr,
                         30000, NULL, blecoap_gap_event, NULL);
    if (rc != 0) {
        ERROR("Error: Failed to connect to device; addr_type=%d "
                           "addr=%s\n", disc->addr_type, addr_str(disc->addr));
        return;
    }
}


/**
 * Initiates the GAP general discovery procedure.
 */
static int
oc_gatt_scan(void)
{
    struct ble_gap_disc_params disc_params;
    int rc;

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 1;

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    disc_params.passive = 1;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(BLE_ADDR_TYPE_PUBLIC, BLE_HS_FOREVER, &disc_params,
                      blecoap_gap_event, NULL);
    if (rc != 0) {
        ERROR("Error initiating GAP discovery procedure; rc=%d\n",
                    rc);
    }
    return rc;
}

#endif

oc_message_t *
oc_attempt_rx_gatt(void) {
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
    LOG("Successfully rx length %lu\n", message->length);
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

static int
blecoap_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
#if (MYNEWT_VAL(OC_CLIENT) == 1)
    case BLE_GAP_EVENT_DISC:
        /* Try to connect to the advertiser if it looks interesting. */
        oc_gatt_connect_if_interesting(&event->disc);
        return 0;
#endif
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) {
            /* nothing to do here on the server  */
        }
        if (event->connect.status != 0) {
            /* nothing to do here on the client */
        }

#if (MYNEWT_VAL(OC_SERVER) == 1)
    /* keep advertising for multiple connections */
    oc_gatt_advertise();
#endif
#if (MYNEWT_VAL(OC_CLIENT) == 1)
        /* keep scanning for new connections */
        oc_gatt_scan();
#endif
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated; resume advertising. */
#if (MYNEWT_VAL(OC_CLIENT) == 1)
        /* keep scanning for new connections */
        oc_gatt_scan();
#endif
#if (MYNEWT_VAL(OC_SERVER) == 1)
        /* resume advertising  */
        oc_gatt_advertise();
#endif
        return 0;
#if (MYNEWT_VAL(OC_CLIENT) == 1)
    case BLE_GAP_EVENT_NOTIFY_RX:
        /* TODO queue the packet */
        return 0;
#endif
    }
    return 0;
}

int
ble_coap_gatt_srv_init(struct os_eventq **out)
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

    *out = &oc_event_q;
    return 0;
}

int oc_connectivity_init_gatt(void) {
    os_mqueue_init(&ble_coap_mq, NULL);
    ble_coap_mq.mq_ev.ev_type = OC_ADATOR_EVENT_GATT;
    return 0;
}

void oc_connectivity_start_gatt(void) {
    int rc;
    rc = ble_hs_start();
    if (rc != 0) {
        goto err;
    }
#if (MYNEWT_VAL(OC_SERVER) == 1)
    rc = oc_gatt_advertise();
    if (rc != 0) {
        goto err;
    }
#endif
#if (MYNEWT_VAL(OC_CLIENT) == 1)
    rc = oc_gatt_scan();
    if (rc != 0) {
        goto err;
    }
#endif
err:
    ERROR("error enabling advertisement; rc=%d\n", rc);
}

void oc_connectivity_shutdown_gatt(void)
{
    /* there is not unregister for BLE */
}

void oc_send_buffer_gatt(oc_message_t *message)
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