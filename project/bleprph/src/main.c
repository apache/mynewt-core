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
#include "controller/ble_ll.h"

#include "bleprph.h"

#define BSWAP16(x)  ((uint16_t)(((x) << 8) | (((x) & 0xff00) >> 8)))

/** Mbuf settings. */
#define MBUF_NUM_MBUFS      (8)
#define MBUF_BUF_SIZE       OS_ALIGN(BLE_MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

static os_membuf_t bleprph_mbuf_mpool_data[MBUF_MEMPOOL_SIZE];
struct os_mbuf_pool bleprph_mbuf_pool;
struct os_mempool bleprph_mbuf_mpool;

/** Log data. */
static struct log_handler bleprph_log_console_handler;
struct log bleprph_log;

/** Priority of the nimble host task. */
#define BLEPRPH_BLE_HS_PRIO          (1)

/** bleprph task settings. */
#define BLEPRPH_STACK_SIZE             (OS_STACK_ALIGN(200))
#define BLEPRPH_TASK_PRIO              (BLEPRPH_BLE_HS_PRIO + 1)

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

static int bleprph_on_connect(int event, int status,
                              struct ble_gap_conn_ctxt *ctxt, void *arg);

/**
 * Utility function to log an array of bytes.
 */
static void
bleprph_print_bytes(uint8_t *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        BLEPRPH_LOG(INFO, "%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

/**
 * Logs information about a connection to the console.
 */
static void
bleprph_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    BLEPRPH_LOG(INFO, "handle=%d peer_addr_type=%d peer_addr=",
                desc->conn_handle, desc->peer_addr_type);
    bleprph_print_bytes(desc->peer_addr, 6);
    BLEPRPH_LOG(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout);
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

    /* Set the advertisement data included in our advertisements. */
    memset(&fields, 0, sizeof fields);
    fields.name = (uint8_t *)bleprph_device_name;
    fields.name_len = strlen(bleprph_device_name);
    fields.name_is_complete = 1;
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    rc = ble_gap_adv_start(BLE_GAP_DISC_MODE_GEN, BLE_GAP_CONN_MODE_UND,
                           NULL, 0, NULL, bleprph_on_connect, NULL);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

static int
bleprph_on_connect(int event, int status, struct ble_gap_conn_ctxt *ctxt,
                   void *arg)
{
    switch (event) {
    case BLE_GAP_EVENT_CONN:
        BLEPRPH_LOG(INFO, "connection %s; status=%d ",
                    status == 0 ? "up" : "down", status);
        bleprph_print_conn_desc(ctxt->desc);
        BLEPRPH_LOG(INFO, "\n");

        if (status != 0) {
            /* Connection terminated; resume advertising. */
            bleprph_advertise();
        }
        break;

    case BLE_GAP_EVENT_CONN_UPDATED:
        BLEPRPH_LOG(INFO, "connection updated; status=%d ", status);
        bleprph_print_conn_desc(ctxt->desc);
        BLEPRPH_LOG(INFO, "\n");
        break;

    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
        BLEPRPH_LOG(INFO, "connection update request; status=%d ", status);
        *ctxt->self_params = *ctxt->peer_params;
        break;
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

    /* Register GATT attributes (services, characteristics, and
     * descriptors).
     */
    gatt_svr_init();

    /* Initialize eventq */
    os_eventq_init(&bleprph_evq);

    /* Begin advertising. */
    bleprph_advertise();

    while (1) {
        ev = os_eventq_get(&bleprph_evq);
        switch (ev->ev_type) {
        case OS_EVENT_T_TIMER:
            cf = (struct os_callout_func *)ev;
            assert(cf->cf_func);
            cf->cf_func(cf->cf_arg);
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
    rc = ble_ll_init();
    assert(rc == 0);

    /* Initialize the BLE host. */
    cfg = ble_hs_cfg_dflt;
    cfg.max_hci_bufs = 3;
    cfg.max_connections = 1;
    cfg.max_attrs = 32;
    cfg.max_services = 4;
    cfg.max_client_configs = 6;
    cfg.max_gattc_procs = 2;
    cfg.max_l2cap_chans = 3;
    cfg.max_l2cap_sig_procs = 2;

    rc = ble_hs_init(BLEPRPH_BLE_HS_PRIO, &cfg);
    assert(rc == 0);

    /* Initialize the console (for log output). */
    rc = console_init(NULL);
    assert(rc == 0);

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}
