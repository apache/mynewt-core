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

#include "syscfg/syscfg.h"
#include "sysinit/sysinit.h"
#include "sysflash/sysflash.h"
#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include <shell/shell.h>
#include <log/log.h>
#include <stats/stats.h>
#include <config/config.h>
#include <sensor/sensor.h>
#include <sim/sim_accel.h>
#include <lsm303dlhc/lsm303dlhc.h>
#include <tsl2561/tsl2561.h>
#include <tcs34725/tcs34725.h>
#include <bno055/bno055.h>
#include "flash_map/flash_map.h"
#include <hal/hal_system.h>
#if MYNEWT_VAL(SPLIT_LOADER)
#include "split/split.h"
#endif
#include <bootutil/image.h>
#include <bootutil/bootutil.h>
#include <assert.h>
#include <string.h>
#include <flash_test/flash_test.h>
#include <reboot/log_reboot.h>
#include <id/id.h>
#include <os/os_time.h>

#if MYNEWT_VAL(SENSOR_OIC)
#include <oic/oc_api.h>
#include <oic/oc_gatt.h>

extern int oc_stack_errno;

static const oc_handler_t sensor_oic_handler = {
    .init = sensor_oic_init,
};

#endif

#if MYNEWT_VAL(SENSOR_BLE)
/* BLE */
#include <nimble/ble.h>
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>
/* Application-specified header. */
#include "bleprph.h"

static int sensor_oic_gap_event(struct ble_gap_event *event, void *arg);

/** Log data. */
struct log bleprph_log;

#endif

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

/* Task 1 */
#define TASK1_PRIO (8)
#define TASK1_STACK_SIZE    OS_STACK_ALIGN(192)
#define MAX_CBMEM_BUF 600
static struct os_task task1;
static volatile int g_task1_loops;

/* Task 2 */
#define TASK2_PRIO (9)
#define TASK2_STACK_SIZE    OS_STACK_ALIGN(64)
static struct os_task task2;
static volatile int g_task2_loops;

static struct log my_log;

/* Global test semaphore */
static struct os_sem g_test_sem;

/* For LED toggling */
static int g_led_pin;

STATS_SECT_START(gpio_stats)
STATS_SECT_ENTRY(toggles)
STATS_SECT_END

static STATS_SECT_DECL(gpio_stats) g_stats_gpio_toggle;

static STATS_NAME_START(gpio_stats)
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
static uint32_t cbmem_buf[MAX_CBMEM_BUF];
static struct cbmem cbmem;

#if MYNEWT_VAL(SENSOR_OIC) && MYNEWT_VAL(SENSOR_BLE)

/**
 * Logs information about a connection to the console.
 */
static void
sensor_oic_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    BLEPRPH_LOG(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    BLEPRPH_LOG(INFO, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    BLEPRPH_LOG(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    BLEPRPH_LOG(INFO, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    BLEPRPH_LOG(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
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
sensor_oic_advertise(void)
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

    fields.uuids128 = (ble_uuid128_t []) {
        BLE_UUID128_INIT(OC_GATT_SERVICE_UUID)
    };
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, sensor_oic_gap_event, NULL);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}


static void
sensor_oic_on_reset(int reason)
{
    BLEPRPH_LOG(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
sensor_oic_on_sync(void)
{
    /* Begin advertising. */
    sensor_oic_advertise();
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * sensor_oic uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  sensor_oic.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
sensor_oic_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        BLEPRPH_LOG(INFO, "connection %s; status=%d ",
                       event->connect.status == 0 ? "established" : "failed",
                       event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            sensor_oic_print_conn_desc(&desc);
        }
        BLEPRPH_LOG(INFO, "\n");

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            sensor_oic_advertise();
        } else {
            oc_ble_coap_conn_new(event->connect.conn_handle);
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        BLEPRPH_LOG(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        sensor_oic_print_conn_desc(&event->disconnect.conn);
        BLEPRPH_LOG(INFO, "\n");

        oc_ble_coap_conn_del(event->disconnect.conn.conn_handle);

        /* Connection terminated; resume advertising. */
        sensor_oic_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        BLEPRPH_LOG(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        sensor_oic_print_conn_desc(&desc);
        BLEPRPH_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        BLEPRPH_LOG(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        sensor_oic_print_conn_desc(&desc);
        BLEPRPH_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        BLEPRPH_LOG(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
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
        BLEPRPH_LOG(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;
    }

    return 0;
}
#endif

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

static void
task1_handler(void *arg)
{
    struct os_task *t;
    int prev_pin_state, curr_pin_state;

    /* Set the led pin for the E407 devboard */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    console_printf("\nSensors Test App\n");

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        ++g_task1_loops;

        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC);

        /* Toggle the LED */
        prev_pin_state = hal_gpio_read(g_led_pin);
        curr_pin_state = hal_gpio_toggle(g_led_pin);
        LOG_INFO(&my_log, LOG_MODULE_DEFAULT, "GPIO toggle from %u to %u",
            prev_pin_state, curr_pin_state);
        STATS_INC(g_stats_gpio_toggle, toggles);

        /* Release semaphore to task 2 */
        os_sem_release(&g_test_sem);
    }
}

static void
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
}

#if !ARCH_sim
static int
config_sensor(void)
{
    struct os_dev *dev;
    int rc;

#if MYNEWT_VAL(TCS34725_PRESENT)
    struct tcs34725_cfg tcscfg;

    dev = (struct os_dev *) os_dev_open("color0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);
    rc = tcs34725_init(dev, NULL);
    if (rc) {
        os_dev_close(dev);
        goto err;
    }

    /* Gain set to 16X and Inetgration time set to 24ms */
    tcscfg.gain = TCS34725_GAIN_16X;;
    tcscfg.integration_time = TCS34725_INTEGRATIONTIME_24MS;

    rc = tcs34725_config((struct tcs34725 *)dev, &tcscfg);
    if (rc) {
        os_dev_close(dev);
        goto err;
    }
    os_dev_close(dev);
#endif

#if MYNEWT_VAL(TSL2561_PRESENT)
    struct tsl2561_cfg tslcfg;

    dev = (struct os_dev *) os_dev_open("light0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);
    rc = tsl2561_init(dev, NULL);
    if (rc) {
        os_dev_close(dev);
        goto err;
    }

    /* Gain set to 1X and Inetgration time set to 13ms */
    tslcfg.gain = TSL2561_LIGHT_GAIN_1X;
    tslcfg.integration_time = TSL2561_LIGHT_ITIME_13MS;

    rc = tsl2561_config((struct tsl2561 *)dev, &tslcfg);
    if (rc) {
        os_dev_close(dev);
        goto err;
    }
    os_dev_close(dev);
#endif

#if MYNEWT_VAL(LSM303DLHC_PRESENT)
    struct lsm303dlhc_cfg lsmcfg;

    dev = (struct os_dev *) os_dev_open("accel0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    rc = lsm303dlhc_init(dev, NULL);
    if (rc) {
        os_dev_close(dev);
        goto err;
    }

    /* read once per sec.  API should take this value in ms. */
    lsmcfg.accel_rate = LSM303DLHC_ACCEL_RATE_1;
    lsmcfg.accel_range = LSM303DLHC_ACCEL_RANGE_2;

    rc = lsm303dlhc_config((struct lsm303dlhc *) dev, &lsmcfg);
    if (rc) {
        os_dev_close(dev);
        goto err;
    }
    os_dev_close(dev);
#endif

#if MYNEWT_VAL(BNO055_PRESENT)
    struct bno055_cfg bcfg;

    dev = (struct os_dev *) os_dev_open("accel1", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    rc = bno055_init(dev, NULL);
    if (rc) {
        os_dev_close(dev);
        assert(0);
        goto err;
    }

    bcfg.bc_units = BNO055_ACC_UNIT_MS2   | BNO055_ANGRATE_UNIT_DPS |
                    BNO055_EULER_UNIT_DEG | BNO055_TEMP_UNIT_DEGC   |
                    BNO055_DO_FORMAT_ANDROID;

    bcfg.bc_opr_mode = BNO055_OPR_MODE_ACCONLY;
    bcfg.bc_pwr_mode = BNO055_PWR_MODE_NORMAL;
    bcfg.bc_acc_bw = BNO055_ACC_CFG_BW_125HZ;
    bcfg.bc_acc_range =  BNO055_ACC_CFG_RNG_16G;
    bcfg.bc_use_ext_xtal = 1;

    rc = bno055_config((struct bno055 *) dev, &bcfg);
    if (rc) {
        os_dev_close(dev);
        goto err;
    }
    os_dev_close(dev);
#endif

    return (0);
err:
    return rc;
}

#else

static int
config_sensor(void)
{
    struct os_dev *dev;
    struct sim_accel_cfg cfg;
    int rc;

    dev = (struct os_dev *) os_dev_open("simaccel0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    rc = sim_accel_init(dev, NULL);
    if (rc != 0) {
        os_dev_close(dev);
        goto err;
    }

    cfg.sac_nr_samples = 10;
    cfg.sac_nr_axises = 1;
    /* read once per sec.  API should take this value in ms. */
    cfg.sac_sample_itvl = OS_TICKS_PER_SEC;

    rc = sim_accel_config((struct sim_accel *) dev, &cfg);
    if (rc != 0) {
        os_dev_close(dev);
        goto err;
    }

    os_dev_close(dev);

    return (0);
err:
    return (rc);
}
#endif

static void
sensors_dev_shell_init(void)
{

#if MYNEWT_VAL(TCS34725_CLI)
    tcs34725_shell_init();
#endif

#if MYNEWT_VAL(TSL2561_CLI)
    tsl2561_shell_init();
#endif

#if MYNEWT_VAL(BNO055_CLI)
    bno055_shell_init();
#endif

}

static void
sensor_ble_oic_server_init(void)
{
#if MYNEWT_VAL(SENSOR_BLE) && MYNEWT_VAL(SENSOR_OIC)
    int rc;

    /* Set initial BLE device address. */
    memcpy(g_dev_addr, (uint8_t[6]){0x0a, 0xfa, 0xcf, 0xac, 0xfa, 0xc0}, 6);

    oc_ble_coap_gatt_srv_init();

    ble_hs_cfg.reset_cb = sensor_oic_on_reset;
    ble_hs_cfg.sync_cb = sensor_oic_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("pi");
    assert(rc == 0);

    rc = oc_main_init((oc_handler_t *)&sensor_oic_handler);
    assert(!rc);

    oc_init_platform("MyNewt", NULL, NULL);
    oc_add_device("/oic/d", "oic.d.pi", "pi", "1.0", "1.0", NULL,
                  NULL);
    assert(!oc_stack_errno);
#endif
}

static void
ble_oic_log_init(void)
{
#if MYNEWT_VAL(SENSOR_BLE)
    /* Initialize the bleprph log. */
    log_register("bleprph", &bleprph_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);

    /* Initialize the NimBLE host configuration. */
    log_register("ble_hs", &ble_hs_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);
#endif

#if MYNEWT_VAL(SENSOR_OIC)
    /* Initialize the OIC  */
    log_register("oic", &oc_log, &log_console_handler, NULL, LOG_SYSLEVEL);
#endif
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
    /* Initialize OS */
    sysinit();

    rc = conf_register(&test_conf_handler);
    assert(rc == 0);

    cbmem_init(&cbmem, cbmem_buf, MAX_CBMEM_BUF);
    log_register("log", &my_log, &log_cbmem_handler, &cbmem, LOG_SYSLEVEL);

    stats_init(STATS_HDR(g_stats_gpio_toggle),
               STATS_SIZE_INIT_PARMS(g_stats_gpio_toggle, STATS_SIZE_32),
               STATS_NAME_INIT_PARMS(gpio_stats));

    stats_register("gpio_toggle", STATS_HDR(g_stats_gpio_toggle));

    ble_oic_log_init();

    flash_test_init();

    conf_load();

    reboot_start(hal_reset_cause());

    init_tasks();

    /* If this app is acting as the loader in a split image setup, jump into
     * the second stage application instead of starting the OS.
     */
#if MYNEWT_VAL(SPLIT_LOADER)
    {
        void *entry;
        rc = split_app_go(&entry, true);
        if(rc == 0) {
            hal_system_restart(entry);
        }
    }
#endif

    sensors_dev_shell_init();

    config_sensor();

    sensor_ble_oic_server_init();

    /*
     * As the last thing, process events from default event queue.
     */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return (0);
}
