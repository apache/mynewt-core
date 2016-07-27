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
#include <imgmgr/imgmgr.h>

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
/* Newtmgr include */
#include "newtmgr/newtmgr.h"
#include "nmgrble/newtmgr_ble.h"
#include "bleuart/bleuart.h"

/* RAM persistence layer. */
#include "store/ram/ble_store_ram.h"

/* Mandatory services. */
#include "services/mandatory/ble_svc_gap.h"
#include "services/mandatory/ble_svc_gatt.h"

/** Mbuf settings. */
#define MBUF_NUM_MBUFS      (12)
#define MBUF_BUF_SIZE       OS_ALIGN(BLE_MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

#define MAX_CONSOLE_INPUT 120
static os_membuf_t bleuart_mbuf_mpool_data[MBUF_MEMPOOL_SIZE];
struct os_mbuf_pool bleuart_mbuf_pool;
struct os_mempool bleuart_mbuf_mpool;

/** Priority of the nimble host and controller tasks. */
#define BLE_LL_TASK_PRI             (OS_TASK_PRI_HIGHEST)

/** bleuart task settings. */
#define bleuart_TASK_PRIO           1
#define bleuart_STACK_SIZE          (OS_STACK_ALIGN(336))

#define NEWTMGR_TASK_PRIO (4)
#define NEWTMGR_TASK_STACK_SIZE (OS_STACK_ALIGN(512))
os_stack_t newtmgr_stack[NEWTMGR_TASK_STACK_SIZE];

struct os_eventq bleuart_evq;
struct os_task bleuart_task;
bssnz_t os_stack_t bleuart_stack[bleuart_STACK_SIZE];

/** Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN] = {0xba, 0xaa, 0xad, 0xba, 0xaa, 0xad};

/** Our random address (in case we need it) */
uint8_t g_random_addr[BLE_DEV_ADDR_LEN];

static int bleuart_gap_event(struct ble_gap_event *event, void *arg);

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void
bleuart_advertise(void)
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
     * stack fill this one automatically as well.  This is done by assiging the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.uuids128 = (void *)gatt_svr_svc_uart;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_ADDR_TYPE_PUBLIC, 0, NULL, BLE_HS_FOREVER,
                           &adv_params, bleuart_gap_event, NULL);
    if (rc != 0) {
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleuart uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  bleuart.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bleuart_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleuart_set_conn_handle(event->connect.conn_handle);
        }

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            bleuart_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated; resume advertising. */
        bleuart_advertise();
        return 0;

    }

    return 0;
}

/**
 * Event loop for the main bleuart task.
 */
static void
bleuart_task_handler(void *unused)
{
    struct os_event *ev;
    struct os_callout_func *cf;
    int rc;

    rc = ble_hs_start();
    assert(rc == 0);

    /* Begin advertising. */
    bleuart_advertise();

    while (1) {
        ev = os_eventq_get(&bleuart_evq);

        /* Check if the event is a nmgr ble mqueue event */
        rc = nmgr_ble_proc_mq_evt(ev);
        if (!rc) {
            continue;
        }

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
    rc = os_mempool_init(&bleuart_mbuf_mpool, MBUF_NUM_MBUFS,
                         MBUF_MEMBLOCK_SIZE, bleuart_mbuf_mpool_data,
                         "bleuart_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&bleuart_mbuf_pool, &bleuart_mbuf_mpool,
                           MBUF_MEMBLOCK_SIZE, MBUF_NUM_MBUFS);
    assert(rc == 0);

    rc = os_msys_register(&bleuart_mbuf_pool);
    assert(rc == 0);

    os_task_init(&bleuart_task, "bleuart", bleuart_task_handler,
                 NULL, bleuart_TASK_PRIO, OS_WAIT_FOREVER,
                 bleuart_stack, bleuart_STACK_SIZE);

    /* Initialize the BLE LL */
    rc = ble_ll_init(BLE_LL_TASK_PRI, MBUF_NUM_MBUFS, BLE_MBUF_PAYLOAD_SIZE);
    assert(rc == 0);

    /* Initialize the BLE host. */
    cfg = ble_hs_cfg_dflt;
    cfg.max_hci_bufs = 3;
    cfg.max_connections = 1;
    cfg.max_gattc_procs = 2;
    cfg.max_l2cap_chans = 3;
    cfg.max_l2cap_sig_procs = 1;
    cfg.sm_bonding = 1;
    cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    cfg.store_read_cb = ble_store_ram_read;
    cfg.store_write_cb = ble_store_ram_write;

    /* Populate config with the required GATT server settings. */
    cfg.max_attrs = 0;
    cfg.max_services = 0;
    cfg.max_client_configs = 0;

    rc = ble_svc_gap_init(&cfg);
    assert(rc == 0);

    rc = ble_svc_gatt_init(&cfg);
    assert(rc == 0);

    /* Nmgr ble GATT server initialization */
    rc = nmgr_ble_gatt_svr_init(&bleuart_evq, &cfg);
    assert(rc == 0);

    rc = bleuart_gatt_svr_init(&cfg);
    assert(rc == 0);

    /* Initialize eventq */
    os_eventq_init(&bleuart_evq);

    rc = ble_hs_init(&bleuart_evq, &cfg);
    assert(rc == 0);

    bleuart_init(MAX_CONSOLE_INPUT);

    nmgr_task_init(NEWTMGR_TASK_PRIO, newtmgr_stack, NEWTMGR_TASK_STACK_SIZE);
    imgmgr_module_init();

    /* Register GATT attributes (services, characteristics, and
     * descriptors).
     */

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("Mynewt_BLEuart");
    assert(rc == 0);

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}
