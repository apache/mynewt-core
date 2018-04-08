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
#include <stdio.h>
#include <errno.h>
#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "hal/hal_system.h"
#include "config/config.h"
#include "split/split.h"

/* BLE */
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"

/* Application-specified header. */
#include "bsnprph.h"

#define BSNPRPH_PKT_SZ          80
#define BSNPRPH_TX_TIMER_RATE   2

static const uint8_t bsnprph_prph_public_addr[6] = {
    0x0a, 0x0b, 0x09, 0x09, 0x09, 0x05,
};

static const ble_addr_t bsnprph_central_addr = {
    BLE_ADDR_PUBLIC,
    { 0x0a, 0x0b, 0x09, 0x09, 0x09, 0x00 },
};

struct log bsnprph_log;

/* Sends data to central at 60 Hz. */
static struct os_callout bsnprph_tx_timer;

/* The handle of the current connection. */
static uint16_t bsnprph_conn_handle;

static int bsnprph_gap_event(struct ble_gap_event *event, void *arg);

/**
 * Logs information about a connection to the console.
 */
static void
bsnprph_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    BSNPRPH_LOG(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    BSNPRPH_LOG(INFO, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    BSNPRPH_LOG(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    BSNPRPH_LOG(INFO, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    BSNPRPH_LOG(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                "encrypted=%d authenticated=%d bonded=%d\n",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Directed connectable mode.
 */
static void
bsnprph_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assiging the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]){
        BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        BSNPRPH_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_DIR;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.high_duty_cycle = 1;
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, &bsnprph_central_addr,
                           BLE_HS_FOREVER, &adv_params,
                           bsnprph_gap_event, NULL);
    if (rc != 0) {
        BSNPRPH_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

static void
bsnprph_tx_timer_reset(void)
{
    int rc;

    rc = os_callout_reset(&bsnprph_tx_timer, BSNPRPH_TX_TIMER_RATE);
    assert(rc == 0);
}

/**
 * Transmits dummy data at 60 Hz.
 */
static void
bsnprph_tx_timer_exp(struct os_event *ev)
{
    static uint8_t buf[BSNPRPH_PKT_SZ];
    static uint8_t val;

    struct os_mbuf *om;
    int rc;

    memset(buf, val, sizeof buf);
    val++;

    om = ble_hs_mbuf_from_flat(buf, sizeof buf);
    if (om == NULL) {
        /* XXX: OOM; log this. */
    } else {
        rc = ble_gattc_notify_custom(bsnprph_conn_handle,
                                     gatt_svr_chr_gendata_val_handle, om);
        if (rc != 0) {
            /* XXX: Log this. */
        }
    }

    bsnprph_tx_timer_reset();
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bsnprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  bsnprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bsnprph_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        BSNPRPH_LOG(INFO, "connection %s; status=%d ",
                       event->connect.status == 0 ? "established" : "failed",
                       event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bsnprph_print_conn_desc(&desc);

            bsnprph_conn_handle = event->connect.conn_handle;
        }
        BSNPRPH_LOG(INFO, "\n");

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            bsnprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        os_callout_stop(&bsnprph_tx_timer);

        BSNPRPH_LOG(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        bsnprph_print_conn_desc(&event->disconnect.conn);
        BSNPRPH_LOG(INFO, "\n");

        /* Connection terminated; resume advertising. */
        bsnprph_advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        os_callout_stop(&bsnprph_tx_timer);
        BSNPRPH_LOG(INFO, "adv complete\n");
        bsnprph_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        BSNPRPH_LOG(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        bsnprph_print_conn_desc(&desc);
        BSNPRPH_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        BSNPRPH_LOG(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        bsnprph_print_conn_desc(&desc);
        BSNPRPH_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        BSNPRPH_LOG(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
                          "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
        if (event->subscribe.attr_handle == gatt_svr_chr_gendata_val_handle) {
            /* Start transmitting notifications. */
            bsnprph_tx_timer_reset();
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        BSNPRPH_LOG(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;
    }

    return 0;
}

static void
bsnprph_on_reset(int reason)
{
    BSNPRPH_LOG(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
bsnprph_on_sync(void)
{
    /* Begin advertising. */
    bsnprph_advertise();
}

/**
 * main
 *
 * The main task for the project. This function initializes the packages,
 * then starts serving events from default event queue.
 *
 * @return int NOTE: this function should never return!
 */
int
main(void)
{
    int rc;

    /* Initialize OS */
    sysinit();

    /* Set initial BLE device address. */
    memcpy(g_dev_addr, bsnprph_prph_public_addr, 6);

    /* Initialize the bsnprph log. */
    log_register("bsnprph", &bsnprph_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);

    /* Initialize the NimBLE host configuration. */
    log_register("ble_hs", &ble_hs_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);
    ble_hs_cfg.reset_cb = bsnprph_on_reset;
    ble_hs_cfg.sync_cb = bsnprph_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;

    os_callout_init(&bsnprph_tx_timer, os_eventq_dflt_get(),
                    bsnprph_tx_timer_exp, NULL);

    rc = gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set(MYNEWT_VAL(BSNPRPH_BLE_NAME));
    assert(rc == 0);

    conf_load();

    /* If this app is acting as the loader in a split image setup, jump into
     * the second stage application instead of starting the OS.
     */
#if MYNEWT_VAL(SPLIT_LOADER)
    {
        void *entry;
        rc = split_app_go(&entry, true);
        if (rc == 0) {
            hal_system_start(entry);
        }
    }
#endif

    /*
     * As the last thing, process events from default event queue.
     */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    return 0;
}
