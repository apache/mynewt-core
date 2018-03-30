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
#include "bsp/bsp.h"

/* BLE */
#include "nimble/ble.h"
#include "controller/ble_ll.h"
#include "host/ble_hs.h"

/* RAM HCI transport. */
#include "transport/ram/ble_hci_ram.h"

/* Mandatory services. */
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* Application-specified header. */
#include "bsncent.h"

#define BSNCENT_PRINT_RATE      (OS_TICKS_PER_SEC * 10)

struct log bsncent_log;

static uint32_t num_notify_pkts_rx;
static uint32_t num_notify_bytes_rx;

/* Prints statistics every 10 seconds. */
static struct os_callout bsncent_print_timer;

/* c66f3301-33b3-4687-850a-d52b0d5d1e3c */
static const ble_uuid128_t bsncent_svc_gendata_uuid =
    BLE_UUID128_INIT(0x3c, 0x1e, 0x5d, 0x0d, 0x2b, 0xd5, 0x0a, 0x85,
                     0x87, 0x46, 0xb3, 0x33, 0x01, 0x33, 0x6f, 0xc6);

/* c66f3301-33b3-4687-850a-d52b0d5d1e3d */
static const ble_uuid128_t bsncent_chr_gendata_uuid =
    BLE_UUID128_INIT(0x3d, 0x1e, 0x5d, 0x0d, 0x2b, 0xd5, 0x0a, 0x85,
                     0x87, 0x46, 0xb3, 0x33, 0x01, 0x33, 0x6f, 0xc6);

static const uint8_t bsncent_cent_public_addr[6] = {
    0x0a, 0x0b, 0x09, 0x09, 0x09, 0x00,
};

static const ble_addr_t bsncent_peer_addrs[] = {
    { BLE_ADDR_PUBLIC, { 0x0a, 0x0b, 0x09, 0x09, 0x09, 0x01 } },
    { BLE_ADDR_PUBLIC, { 0x0a, 0x0b, 0x09, 0x09, 0x09, 0x02 } },
    { BLE_ADDR_PUBLIC, { 0x0a, 0x0b, 0x09, 0x09, 0x09, 0x03 } },
    { BLE_ADDR_PUBLIC, { 0x0a, 0x0b, 0x09, 0x09, 0x09, 0x04 } },
    { BLE_ADDR_PUBLIC, { 0x0a, 0x0b, 0x09, 0x09, 0x09, 0x05 } },
};
static const int bsncent_num_peer_addrs =
    sizeof bsncent_peer_addrs / sizeof bsncent_peer_addrs[0];

static int bsncent_gap_event(struct ble_gap_event *event, void *arg);

static const struct ble_gap_conn_params ble_gap_conn_params_bsn = {
   .scan_itvl = 0x0010,
   .scan_window = 0x0010,
   .itvl_min = 13,
   .itvl_max = 13,
   .latency = BLE_GAP_INITIAL_CONN_LATENCY,
   .supervision_timeout = BLE_GAP_INITIAL_SUPERVISION_TIMEOUT,
   .min_ce_len = BLE_GAP_INITIAL_CONN_MIN_CE_LEN,
   .max_ce_len = BLE_GAP_INITIAL_CONN_MAX_CE_LEN,
};

/**
 * Application callback.  Called when the attempt to subscribe to notifications
 * for the ANS Unread Alert Status characteristic has completed.
 */
static int
bsncent_on_subscribe(uint16_t conn_handle,
                     const struct ble_gatt_error *error,
                     struct ble_gatt_attr *attr,
                     void *arg)
{
    BSNCENT_LOG(INFO, "Subscribe complete; status=%d conn_handle=%d "
                      "attr_handle=%d\n",
                error->status, conn_handle, attr->handle);

    return 0;
}

/**
 * Performs three concurrent GATT operations against the specified peer:
 * 1. Reads the ANS Supported New Alert Category characteristic.
 * 2. Writes the ANS Alert Notification Control Point characteristic.
 * 3. Subscribes to notifications for the ANS Unread Alert Status
 *    characteristic.
 *
 * If the peer does not support a required service, characteristic, or
 * descriptor, then the peer lied when it claimed support for the alert
 * notification service!  When this happens, or if a GATT procedure fails,
 * this function immediately terminates the connection.
 */
static void
bsncent_subscribe(const struct peer *peer)
{
    const struct peer_dsc *dsc;
    uint8_t value[2];
    int rc;

    /* Subscribe to notifications for the gendata characteristic.
     * A central enables notifications by writing two bytes (1, 0) to the
     * characteristic's client-characteristic-configuration-descriptor (CCCD).
     */
    dsc = peer_dsc_find_uuid(
        peer,
        &bsncent_svc_gendata_uuid.u,
        &bsncent_chr_gendata_uuid.u,
        BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL) {
        BSNCENT_LOG(ERROR, "Error: Peer lacks a CCCD for the generic data "
                           "characteristic\n");
        goto err;
    }

    value[0] = 1;
    value[1] = 0;
    rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle,
                              value, sizeof value, bsncent_on_subscribe, NULL);
    if (rc != 0) {
        BSNCENT_LOG(ERROR, "Error: Failed to subscribe to characteristic; "
                           "rc=%d\n", rc);
        goto err;
    }

    return;

err:
    /* Terminate the connection. */
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Called when service discovery of the specified peer has completed.
 */
static void
bsncent_on_disc_complete(const struct peer *peer, int status, void *arg)
{

    if (status != 0) {
        /* Service discovery failed.  Terminate the connection. */
        BSNCENT_LOG(ERROR, "Error: Service discovery failed; status=%d "
                           "conn_handle=%d\n", status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    /* Service discovery has completed successfully.  Now we have a complete
     * list of services, characteristics, and descriptors that the peer
     * supports.
     */
    BSNCENT_LOG(ERROR, "Service discovery complete; status=%d "
                       "conn_handle=%d\n", status, peer->conn_handle);

    /* Now subscribe to the gendata characterustic. */
    bsncent_subscribe(peer);
}


static int
bsncent_on_mtu_exchanged(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         uint16_t mtu, void *arg)
{
    int rc;

    if (error->status != 0) {
        BSNCENT_LOG(ERROR, "MTU exchange failed; rc=%d\n", error->status);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }

    /* Perform service discovery. */
    rc = peer_disc_all(conn_handle, bsncent_on_disc_complete, NULL);
    if (rc != 0) {
        BSNCENT_LOG(ERROR, "Failed to discover services; rc=%d\n", rc);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }

    return 0;
}

static void
bsncent_connect(void)
{
    int rc;

    rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                         &ble_gap_conn_params_bsn, bsncent_gap_event, NULL);
    if (rc != 0) {
        BSNCENT_LOG(ERROR, "Error connecting; rc=%d\n", rc);
        if (!((rc == BLE_HS_EALREADY) || (rc == BLE_HS_EBUSY))) {
            /* Only assert if we are not already trying */
            assert(0);
        }
    }
}

static void
bsncent_fill_wl(void)
{
    int rc;

    rc = ble_gap_wl_set(bsncent_peer_addrs, bsncent_num_peer_addrs);
    if (rc != 0) {
        BSNCENT_LOG(ERROR, "Error setting white list; rc=%d\n", rc);
        assert(0);
    }
}

static void
bsncent_print_timer_reset(void)
{
    int rc;

    rc = os_callout_reset(&bsncent_print_timer, BSNCENT_PRINT_RATE);
    assert(rc == 0);
}

/**
 * Prints statistics every 10 seconds.
 */
static void
bsncent_print_timer_exp(struct os_event *ev)
{
    static uint32_t prev_bytes;
    static uint32_t prev_pkts;
    uint32_t diff_bytes;
    uint32_t diff_pkts;

    console_printf("--\n");
    console_printf("%8d connections\n", peer_count());
    console_printf("%8lu packets received\n",
                   (unsigned long)num_notify_pkts_rx);
    console_printf("%8lu bytes received\n",
                   (unsigned long)num_notify_bytes_rx);

    if (prev_pkts != 0) {
        diff_pkts = num_notify_pkts_rx - prev_pkts;
        diff_bytes = num_notify_bytes_rx - prev_bytes;

        console_printf("%8lu packets per second\n",
                       (unsigned long)(diff_pkts /
                                       (BSNCENT_PRINT_RATE /
                                        OS_TICKS_PER_SEC)));
        console_printf("%8lu bytes per second\n",
                       (unsigned long)(diff_bytes /
                                       (BSNCENT_PRINT_RATE /
                                        OS_TICKS_PER_SEC)));
    }

    prev_pkts = num_notify_pkts_rx;
    prev_bytes = num_notify_bytes_rx;

    bsncent_print_timer_reset();
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  bsncent uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  bsncent.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bsncent_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) {
            /* Connection successfully established. */
            BSNCENT_LOG(INFO, "Connection established ");

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            print_conn_desc(&desc);
            BSNCENT_LOG(INFO, "\n");

            /* Remember peer. */
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                BSNCENT_LOG(ERROR, "Failed to add peer; rc=%d\n", rc);
                assert(0);
            }

            /* Try to connect to next peer if any remain unconnected. */
            if (peer_count() < bsncent_num_peer_addrs) {
                bsncent_connect();
            }

            /* Negotiate ATT MTU. */
            rc = ble_gattc_exchange_mtu(event->connect.conn_handle,
                                        bsncent_on_mtu_exchanged, NULL);
            if (rc != 0) {
                BSNCENT_LOG(ERROR, "Failed to exchange MTU; rc=%d\n", rc);
                return 0;
            }
        } else {
            /* Connection attempt failed; resume connecting. */
            BSNCENT_LOG(ERROR, "Error: Connection failed; status=%d\n",
                        event->connect.status);
            bsncent_connect();
        }

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated. */
        BSNCENT_LOG(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);
        BSNCENT_LOG(INFO, "\n");

        /* Forget about peer. */
        peer_delete(event->disconnect.conn.conn_handle);

        /* Resume scanning. */
        bsncent_connect();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        BSNCENT_LOG(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        print_conn_desc(&desc);
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        /* Peer sent us a notification or indication. */
        BSNCENT_LOG(DEBUG, "received %s; conn_handle=%d attr_handle=%d "
                           "attr_len=%d\n",
                    event->notify_rx.indication ?
                        "indication" :
                        "notification",
                    event->notify_rx.conn_handle,
                    event->notify_rx.attr_handle,
                    OS_MBUF_PKTLEN(event->notify_rx.om));

        num_notify_pkts_rx++;
        num_notify_bytes_rx += OS_MBUF_PKTLEN(event->notify_rx.om);

        /* Attribute data is contained in event->notify_rx.attr_data. */
        return 0;

    case BLE_GAP_EVENT_MTU:
        BSNCENT_LOG(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    default:
        return 0;
    }
}

static void
bsncent_on_reset(int reason)
{
    BSNCENT_LOG(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
bsncent_on_sync(void)
{
    /* Start printing statistics. */
    bsncent_print_timer_reset();

    /* Add the five known peripherals to the white list. */
    bsncent_fill_wl();

    /* Attempt to connect to the first peripheral. */
    bsncent_connect();
}

/**
 * main
 *
 * All application logic and NimBLE host work is performed in default task.
 *
 * @return int NOTE: this function should never return!
 */
int
main(void)
{
    int rc;

    /* Set initial BLE device address. */
    memcpy(g_dev_addr, bsncent_cent_public_addr, 6);

    /* Initialize OS */
    sysinit();

    /* Initialize the bsncent log. */
    log_register("bsncent", &bsncent_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);

    /* Configure the host. */
    log_register("ble_hs", &ble_hs_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);
    ble_hs_cfg.reset_cb = bsncent_on_reset;
    ble_hs_cfg.sync_cb = bsncent_on_sync;

    os_callout_init(&bsncent_print_timer, os_eventq_dflt_get(),
                    bsncent_print_timer_exp, NULL);

    /* XXX: I think some of these need to be based on # of connections */
    /* Initialize data structures to track connected peers. */
    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 96, 64);
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set(MYNEWT_VAL(BSNCENT_BLE_NAME));
    assert(rc == 0);

    /* os start should never return. If it does, this should be an error */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return 0;
}
