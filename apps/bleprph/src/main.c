/**
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
#include "bsp/bsp.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_cputime.h"
#include "console/console.h"

/* BLE */
#include "nimble/ble.h"
#include "host/host_hci.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_uuid.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_l2cap.h"
#include "host/ble_sm.h"
#include "controller/ble_ll.h"

#include "bleprph.h"

#define BSWAP16(x)  ((uint16_t)(((x) << 8) | (((x) & 0xff00) >> 8)))

/** Mbuf settings. */
#define MBUF_NUM_MBUFS      (12)
#define MBUF_BUF_SIZE       OS_ALIGN(BLE_MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

static os_membuf_t bleprph_mbuf_mpool_data[MBUF_MEMPOOL_SIZE];
struct os_mbuf_pool bleprph_mbuf_pool;
struct os_mempool bleprph_mbuf_mpool;

/** Log data. */
static struct log_handler bleprph_log_console_handler;
struct log bleprph_log;

/** Priority of the nimble host and controller tasks. */
#define BLE_LL_TASK_PRI             (OS_TASK_PRI_HIGHEST)

/** bleprph task settings. */
#define BLEPRPH_TASK_PRIO           1
#define BLEPRPH_STACK_SIZE          (OS_STACK_ALIGN(336))

struct os_eventq bleprph_evq;
struct os_task bleprph_task;
bssnz_t os_stack_t bleprph_stack[BLEPRPH_STACK_SIZE];

/** Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN] = {0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a};

/** Our random address (in case we need it) */
uint8_t g_random_addr[BLE_DEV_ADDR_LEN];

/** Device name - included in advertisements and exposed by GAP service. */
const char *bleprph_device_name = "nimble-bleprph";

/** Device properties - exposed by GAP service. */
const uint16_t bleprph_appearance = BSWAP16(BLE_GAP_APPEARANCE_GEN_COMPUTER);
const uint8_t bleprph_privacy_flag = 0;
uint8_t bleprph_reconnect_addr[6];
uint8_t bleprph_pref_conn_params[8];
uint8_t bleprph_gatt_service_changed[4];

static int bleprph_gap_event(int event, struct ble_gap_conn_ctxt *ctxt,
                             void *arg);

/**
 * Logs information about a connection to the console.
 */
static void
bleprph_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    BLEPRPH_LOG(INFO, "handle=%d peer_addr_type=%d peer_addr=",
                desc->conn_handle,
                desc->peer_addr_type);
    print_bytes(desc->peer_addr, 6);
    BLEPRPH_LOG(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                      "encrypted=%d authenticated=%d",
                desc->conn_itvl,
                desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated);
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void
bleprph_advertise(void)
{
    struct ble_hs_adv_fields fields;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    fields.tx_pwr_lvl_is_present = 1;

    fields.name = (uint8_t *)bleprph_device_name;
    fields.name_len = strlen(bleprph_device_name);
    fields.name_is_complete = 1;

    fields.uuids16 = (uint16_t[]){ GATT_SVR_SVC_ALERT_UUID };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    rc = ble_gap_adv_start(BLE_GAP_DISC_MODE_GEN, BLE_GAP_CONN_MODE_UND,
                           NULL, 0, NULL, bleprph_gap_event, NULL);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bleprph_gap_event(int event, struct ble_gap_conn_ctxt *ctxt, void *arg)
{
    switch (event) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        BLEPRPH_LOG(INFO, "connection %s; status=%d ",
                       ctxt->connect.status == 0 ? "established" : "failed",
                       ctxt->connect.status);
        bleprph_print_conn_desc(ctxt->desc);
        BLEPRPH_LOG(INFO, "\n");

        if (ctxt->connect.status != 0) {
            /* Connection failed; resume advertising. */
            bleprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        BLEPRPH_LOG(INFO, "disconnect; reason=%d ", ctxt->disconnect.reason);
        bleprph_print_conn_desc(ctxt->desc);
        BLEPRPH_LOG(INFO, "\n");

        /* Connection terminated; resume advertising. */
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        BLEPRPH_LOG(INFO, "connection updated; status=%d ",
                    ctxt->conn_update.status);
        bleprph_print_conn_desc(ctxt->desc);
        BLEPRPH_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        BLEPRPH_LOG(INFO, "encryption change event; status=%d ",
                    ctxt->enc_change.status);
        bleprph_print_conn_desc(ctxt->desc);
        BLEPRPH_LOG(INFO, "\n");
        return 0;
    }

    return 0;
}

/**
 * Event loop for the main bleprph task.
 */
static void
bleprph_task_handler(void *unused)
{
    struct os_event *ev;
    struct os_callout_func *cf;
    int rc;

    rc = ble_hs_start();
    assert(rc == 0);

    /* Begin advertising. */
    bleprph_advertise();

    while (1) {
        ev = os_eventq_get(&bleprph_evq);
        switch (ev->ev_type) {
        case OS_EVENT_T_TIMER:
            cf = (struct os_callout_func *)ev;
            assert(cf->cf_func);
            cf->cf_func(CF_ARG(cf));
            break;
        default:
            assert(0);
            break;
        }
    }
}

/**
 * main
 *
 * The main function for the project. This function initializes the os, calls
 * init_tasks to initialize tasks (and possibly other objects), then starts the
 * OS. We should not return from os start.
 *
 * @return int NOTE: this function should never return!
 */
int
main(void)
{
    struct ble_hs_cfg cfg;
    uint32_t seed;
    int rc;
    int i;

    /* Initialize OS */
    os_init();

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(1000000);
    assert(rc == 0);

    /* Seed random number generator with least significant bytes of device
     * address.
     */
    seed = 0;
    for (i = 0; i < 4; ++i) {
        seed |= g_dev_addr[i];
        seed <<= 8;
    }
    srand(seed);

    /* Initialize msys mbufs. */
    rc = os_mempool_init(&bleprph_mbuf_mpool, MBUF_NUM_MBUFS,
                         MBUF_MEMBLOCK_SIZE, bleprph_mbuf_mpool_data,
                         "bleprph_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&bleprph_mbuf_pool, &bleprph_mbuf_mpool,
                           MBUF_MEMBLOCK_SIZE, MBUF_NUM_MBUFS);
    assert(rc == 0);

    rc = os_msys_register(&bleprph_mbuf_pool);
    assert(rc == 0);

    /* Initialize the logging system. */
    log_init();
    log_console_handler_init(&bleprph_log_console_handler);
    log_register("bleprph", &bleprph_log, &bleprph_log_console_handler);

    os_task_init(&bleprph_task, "bleprph", bleprph_task_handler,
                 NULL, BLEPRPH_TASK_PRIO, OS_WAIT_FOREVER,
                 bleprph_stack, BLEPRPH_STACK_SIZE);

    /* Initialize the BLE LL */
    rc = ble_ll_init(BLE_LL_TASK_PRI, MBUF_NUM_MBUFS, BLE_MBUF_PAYLOAD_SIZE);
    assert(rc == 0);

    /* Initialize the BLE host. */
    cfg = ble_hs_cfg_dflt;
    cfg.max_hci_bufs = 3;
    cfg.max_connections = 1;
    cfg.max_attrs = 42;
    cfg.max_services = 5;
    cfg.max_client_configs = 6;
    cfg.max_gattc_procs = 2;
    cfg.max_l2cap_chans = 3;
    cfg.max_l2cap_sig_procs = 1;
    cfg.sm_bonding = 1;
    cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    cfg.store_read_cb = store_read;
    cfg.store_write_cb = store_write;

    /* Initialize eventq */
    os_eventq_init(&bleprph_evq);

    rc = ble_hs_init(&bleprph_evq, &cfg);
    assert(rc == 0);

    /* Initialize the console (for log output). */
    rc = console_init(NULL);
    assert(rc == 0);

    /* Register GATT attributes (services, characteristics, and
     * descriptors).
     */
    gatt_svr_init();

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}
