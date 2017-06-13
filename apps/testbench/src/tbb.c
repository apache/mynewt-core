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

/** tbb - test bench BLE. */

#include "syscfg/syscfg.h"

#if MYNEWT_VAL(TESTBENCH_BLE)

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "sysinit/sysinit.h"
#include "bsp/bsp.h"
#include "os/os.h"
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

#include "oic/oc_gatt.h"

#include "tbb.h"

static int tbb_gap_event(struct ble_gap_event *event, void *arg);

static struct log tbb_log;

/* tbb uses the first "peruser" log module. */
#define TBB_LOG_MODULE  (LOG_MODULE_PERUSER + 0)

/* Convenience macro for logging to the tbb module. */
#define TBB_LOG(lvl, ...) \
    LOG_ ## lvl(&tbb_log, TBB_LOG_MODULE, __VA_ARGS__)

void
tbb_print_addr(const void *addr)
{
    const uint8_t *u8p;

    u8p = addr;
    TBB_LOG(INFO, "%02x:%02x:%02x:%02x:%02x:%02x",
            u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

/**
 * Logs information about a connection to the console.
 */
static void
tbb_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    TBB_LOG(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    tbb_print_addr(desc->our_ota_addr.val);
    TBB_LOG(INFO, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    tbb_print_addr(desc->our_id_addr.val);
    TBB_LOG(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    tbb_print_addr(desc->peer_ota_addr.val);
    TBB_LOG(INFO, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    tbb_print_addr(desc->peer_id_addr.val);
    TBB_LOG(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
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
 *     o Undirected connectable mode.
 */
static void
tbb_advertise(void)
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

    fields.uuids128 = (ble_uuid128_t[]) {
        BLE_UUID128_INIT(OC_GATT_SERVICE_UUID)
    };
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        TBB_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, tbb_gap_event, NULL);
    if (rc != 0) {
        TBB_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * tbb uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  tbb.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
tbb_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        TBB_LOG(INFO, "connection %s; status=%d ",
                       event->connect.status == 0 ? "established" : "failed",
                       event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            tbb_print_conn_desc(&desc);
        }
        TBB_LOG(INFO, "\n");

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            tbb_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        TBB_LOG(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        tbb_print_conn_desc(&event->disconnect.conn);
        TBB_LOG(INFO, "\n");

        /* Connection terminated; resume advertising. */
        tbb_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        TBB_LOG(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        tbb_print_conn_desc(&desc);
        TBB_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        TBB_LOG(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        tbb_print_conn_desc(&desc);
        TBB_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        TBB_LOG(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
                          "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
        return 0;

    case BLE_GAP_EVENT_MTU:
        TBB_LOG(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;
    }

    return 0;
}

static void
tbb_on_reset(int reason)
{
    TBB_LOG(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
tbb_on_sync(void)
{
    /* Begin advertising. */
    tbb_advertise();
}

void
tbb_init(void)
{
    int rc;

    /* Initialize the tbb log. */
    log_register("tbb", &tbb_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);

    /* Initialize the NimBLE host configuration. */
    log_register("ble_hs", &ble_hs_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);
    ble_hs_cfg.reset_cb = tbb_on_reset;
    ble_hs_cfg.sync_cb = tbb_on_sync;

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set(MYNEWT_VAL(TESTBENCH_BLE_NAME));
    assert(rc == 0);
}

#endif
