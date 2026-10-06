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

#include "os/mynewt.h"
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include <log/log.h>
#include <stats/stats.h>
#include <config/config.h>
#if MYNEWT_VAL(SPLIT_LOADER)
#include "split/split.h"
#endif
#include <bootutil/image.h>
#include <img_mgmt/img_mgmt.h>
#include <mgmt/mgmt.h>
#include <assert.h>
#include <string.h>
#include <reboot/log_reboot.h>
#include <oic/oc_api.h>
#include <cborattr/cborattr.h>

#ifndef ARCH_sim
/* BLE */
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

#include <oic/oc_gatt.h>

/* Application-specified header. */
#include "omp_svr.h"

#else
#include <mcu/mcu_sim.h>
#endif /* !ARCH_sim */

/* Task 1 */
#define TASK1_PRIO (8)
#define TASK1_STACK_SIZE    OS_STACK_ALIGN(192)
static struct os_task task1;
static volatile int g_task1_loops;

/* Task 2 */
#define TASK2_PRIO (9)
#define TASK2_STACK_SIZE    OS_STACK_ALIGN(64)
static struct os_task task2;

static volatile int g_task2_loops;

/* Global test semaphore */
static struct os_sem g_test_sem;

/* For LED toggling */
static int g_led_pin;

STATS_SECT_START(gpio_stats)
STATS_SECT_ENTRY(toggles)
STATS_SECT_END

static STATS_SECT_DECL(gpio_stats) g_stats_gpio_toggle;

STATS_NAME_START(gpio_stats)
STATS_NAME(gpio_stats, toggles)
STATS_NAME_END(gpio_stats)

static char *test_conf_get(int argc, char **argv, char *val, int max_len);
static int test_conf_set(int argc, char **argv, char *val);
static int test_conf_commit(void);
static int test_conf_export(void (*export_func)(char *name, char *val),
  enum conf_export_tgt tgt);

static struct conf_handler test_conf_handler = {
    .ch_name = "test",
    .ch_get = test_conf_get,
    .ch_set = test_conf_set,
    .ch_commit = test_conf_commit,
    .ch_export = test_conf_export
};

static uint8_t test8;
static uint8_t test8_shadow;
static char test_str[32];

static char *
test_conf_get(int argc, char **argv, char *buf, int max_len)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "8")) {
            return conf_str_from_value(CONF_INT8, &test8, buf, max_len);
        } else if (!strcmp(argv[0], "str")) {
            return test_str;
        }
    }
    return NULL;
}

static int
test_conf_set(int argc, char **argv, char *val)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "8")) {
            return CONF_VALUE_SET(val, CONF_INT8, test8_shadow);
        } else if (!strcmp(argv[0], "str")) {
            return CONF_VALUE_SET(val, CONF_STRING, test_str);
        }
    }
    return OS_ENOENT;
}

static int
test_conf_commit(void)
{
    test8 = test8_shadow;

    return 0;
}

static int
test_conf_export(void (*func)(char *name, char *val), enum conf_export_tgt tgt)
{
    char buf[4];

    conf_str_from_value(CONF_INT8, &test8, buf, sizeof(buf));
    func("test/8", buf);
    func("test/str", test_str);
    return 0;
}

#ifndef ARCH_sim
static int omp_svr_gap_event(struct ble_gap_event *event, void *arg);

/**
 * Logs information about a connection to the console.
 */
static void
omp_svr_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    MODLOG_DFLT(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    MODLOG_DFLT(INFO, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    MODLOG_DFLT(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    MODLOG_DFLT(INFO, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    MODLOG_DFLT(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
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
omp_svr_advertise(void)
{
    uint8_t own_addr_type;
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

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
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, omp_svr_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * omp_svr uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  omp_svr.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
omp_svr_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        MODLOG_DFLT(INFO, "connection %s; status=%d ",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            omp_svr_print_conn_desc(&desc);

        }
        MODLOG_DFLT(INFO, "\n");

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            omp_svr_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        omp_svr_print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        /* Connection terminated; resume advertising. */
        omp_svr_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        MODLOG_DFLT(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        omp_svr_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "advertise complete; reason=%d",
                    event->adv_complete.reason);
        omp_svr_advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        omp_svr_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
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
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    return 0;
}

static void
omp_svr_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
omp_svr_on_sync(void)
{
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin advertising. */
    omp_svr_advertise();
}
#endif /* ARCH_sim */

void
task1_handler(void *arg)
{
    struct os_task *t;
    int prev_pin_state, curr_pin_state;
    struct image_version ver;
    char ver_str[IMG_MGMT_VER_MAX_STR_LEN];
    int rc;

    /* Set the led pin for the E407 devboard */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    if (img_mgmt_read_info(0, &ver, NULL, NULL) == 0) {
        rc = img_mgmt_ver_str(&ver, ver_str);
        assert(rc == 0);
        console_printf("\nOMP_SVR %s\n", ver_str);
    } else {
        console_printf("\nOMP_SVR\n");
    }

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        ++g_task1_loops;

        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC);

        /* Toggle the LED */
        prev_pin_state = hal_gpio_read(g_led_pin);
        curr_pin_state = hal_gpio_toggle(g_led_pin);
        MODLOG_DFLT(INFO, "GPIO %d: %u to %u\n", g_led_pin,
                    prev_pin_state, curr_pin_state);
        STATS_INC(g_stats_gpio_toggle, toggles);

        /* Release semaphore to task 2 */
        os_sem_release(&g_test_sem);
    }
}

void
task2_handler(void *arg)
{
    struct os_task *t;

    while (1) {
        /* just for debug; task 2 should be the running task */
        t = os_sched_get_current_task();
        assert(t->t_func == task2_handler);

        /* Increment # of times we went through task loop */
        ++g_task2_loops;

        /* Wait for semaphore from ISR */
        os_sem_pend(&g_test_sem, OS_TIMEOUT_NEVER);
    }
}

/*
 * OIC platform/resource registration.
 */
static void
omgr_app_init(void)
{
    oc_init_platform("MyNewt", NULL, NULL);
}

static const oc_handler_t omgr_oc_handler = {
    .init = omgr_app_init,
};

/**
 * init_tasks
 *
 * Called by main.c after sysinit(). This function performs initializations
 * that are required before tasks are running.
 *
 * @return int 0 success; error otherwise.
 */
static void
init_tasks(void)
{
    os_stack_t *pstack;
    /* Initialize global test semaphore */
    os_sem_init(&g_test_sem, 0);

    pstack = malloc(sizeof(os_stack_t)*TASK1_STACK_SIZE);
    assert(pstack);

    os_task_init(&task1, "task1", task1_handler, NULL,
            TASK1_PRIO, OS_WAIT_FOREVER, pstack, TASK1_STACK_SIZE);

    pstack = malloc(sizeof(os_stack_t)*TASK2_STACK_SIZE);
    assert(pstack);

    os_task_init(&task2, "task2", task2_handler, NULL,
            TASK2_PRIO, OS_WAIT_FOREVER, pstack, TASK2_STACK_SIZE);

    oc_main_init((oc_handler_t *)&omgr_oc_handler);
}

/**
 * main
 *
 * The main task for the project. This function initializes the packages, calls
 * init_tasks to initialize additional tasks (and possibly other objects),
 * then starts serving events from default event queue.
 *
 * @return int NOTE: this function should never return!
 */
int
main(int argc, char **argv)
{
    int rc;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif
    sysinit();

#ifndef ARCH_sim
    oc_ble_coap_gatt_srv_init();

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = omp_svr_on_reset;
    ble_hs_cfg.sync_cb = omp_svr_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("mynewt-omp-svr");
    assert(rc == 0);
#endif

    rc = conf_register(&test_conf_handler);
    assert(rc == 0);

    stats_init(STATS_HDR(g_stats_gpio_toggle),
               STATS_SIZE_INIT_PARMS(g_stats_gpio_toggle, STATS_SIZE_32),
               STATS_NAME_INIT_PARMS(gpio_stats));

    stats_register("gpio_toggle", STATS_HDR(g_stats_gpio_toggle));

    reboot_start(hal_reset_cause());

#if MYNEWT_VAL(SPLIT_LOADER)
    {
        void *entry;
        rc = split_app_go(&entry, true);
        if(rc == 0) {
            hal_system_start(entry);
        }
    }
#endif

    init_tasks();

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    /* Never returns */

    return rc;
}
