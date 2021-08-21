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

#include "os/mynewt.h"
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include <config/config.h>
#include <log/log.h>
#include <sensor/sensor.h>
#include "flash_map/flash_map.h"
#include <hal/hal_system.h>
#include <assert.h>
#include <string.h>
#include <reboot/log_reboot.h>
#include <id/id.h>

#if MYNEWT_VAL(BNO055_CLI)
#include <bno055/bno055.h>
#endif
#if MYNEWT_VAL(TCS34725_CLI)
#include <tcs34725/tcs34725.h>
#endif
#if MYNEWT_VAL(TSL2561_CLI)
#include <tsl2561/tsl2561.h>
#endif
#if MYNEWT_VAL(BMA253_CLI)
#include <bma253/bma253.h>
#endif
#if MYNEWT_VAL(BMA2XX_CLI)
#include <bma2xx/bma2xx.h>
#endif
#if MYNEWT_VAL(BME280_CLI)
#include <bme280/bme280.h>
#endif
#if MYNEWT_VAL(BMP280_CLI)
#include <bmp280/bmp280.h>
#endif
#if MYNEWT_VAL(DRV2605_CLI)
#include <drv2605/drv2605.h>
#endif
#if MYNEWT_VAL(LIS2DS12_CLI)
#include <lis2ds12/lis2ds12.h>
#endif
#if MYNEWT_VAL(LIS2DW12_CLI)
#include <lis2dw12/lis2dw12.h>
#endif
#if MYNEWT_VAL(KXTJ3_CLI)
#include <kxtj3/kxtj3.h>
#endif
#if MYNEWT_VAL(ICP101XX_CLI)
#include <icp101xx/icp101xx.h>
#endif

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

#if MYNEWT_VAL(SENSOR_OIC)
static int sensor_oic_gap_event(struct ble_gap_event *event, void *arg);
#endif

#endif

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

#if MYNEWT_VAL(SENSOR_OIC) && MYNEWT_VAL(SENSOR_BLE)

/**
 * Logs information about a connection to the console.
 */
static void
sensor_oic_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    DFLT_LOG_INFO("handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    DFLT_LOG_INFO(" our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    DFLT_LOG_INFO(" peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    DFLT_LOG_INFO(" peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    DFLT_LOG_INFO(" conn_itvl=%d conn_latency=%d supervision_timeout=%d "
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
        BLE_UUID128_INIT(OC_GATT_UNSEC_SVC_UUID)
    };
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        DFLT_LOG_ERROR("error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, sensor_oic_gap_event, NULL);
    if (rc != 0) {
        DFLT_LOG_ERROR("error enabling advertisement; rc=%d\n", rc);
        return;
    }
}


static void
sensor_oic_on_reset(int reason)
{
    DFLT_LOG_ERROR("Resetting state; reason=%d\n", reason);
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
        DFLT_LOG_INFO("connection %s; status=%d ",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            sensor_oic_print_conn_desc(&desc);
        }
        DFLT_LOG_INFO("\n");

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            sensor_oic_advertise();
        } else {
            oc_ble_coap_conn_new(event->connect.conn_handle);
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        DFLT_LOG_INFO("disconnect; reason=%d ", event->disconnect.reason);
        sensor_oic_print_conn_desc(&event->disconnect.conn);
        DFLT_LOG_INFO("\n");

        oc_ble_coap_conn_del(event->disconnect.conn.conn_handle);

        /* Connection terminated; resume advertising. */
        sensor_oic_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        DFLT_LOG_INFO("connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        sensor_oic_print_conn_desc(&desc);
        DFLT_LOG_INFO("\n");
        return 0;


    case BLE_GAP_EVENT_DISC_COMPLETE:
        DFLT_LOG_INFO("discovery complete; reason=%d\n",
                    event->disc_complete.reason);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        DFLT_LOG_INFO("advertise complete; reason=%d\n",
                    event->adv_complete.reason);
        sensor_oic_advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        DFLT_LOG_INFO("encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        sensor_oic_print_conn_desc(&desc);
        DFLT_LOG_INFO("\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        DFLT_LOG_INFO("subscribe event; conn_handle=%d attr_handle=%d "
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
        DFLT_LOG_INFO("mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;
    }

    return 0;
}
#endif

#ifdef LED_BLINK_PIN

/* The timer callout */
static struct os_callout blink_callout;

/* For LED toggling */
static int g_led_pin;

/*
 * Event callback function for timer events. It toggles the led pin.
 */
static void
blink_ev_cb(struct os_event *ev)
{
    assert(ev != NULL);

    hal_gpio_toggle(g_led_pin);

    os_callout_reset(&blink_callout, OS_TICKS_PER_SEC);
}

static void
init_blink(void)
{
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);
    /*
     * Initialize the callout for a timer event.
     */
    os_callout_init(&blink_callout, os_eventq_dflt_get(),
                    blink_ev_cb, NULL);

    os_callout_reset(&blink_callout, OS_TICKS_PER_SEC);
}

#else
#define init_blink()
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

#if MYNEWT_VAL(BMA253_CLI)
    bma253_shell_init();
#endif
#if MYNEWT_VAL(BMA2XX_CLI)
    bma2xx_shell_init();
#endif

#if MYNEWT_VAL(BME280_CLI)
    bme280_shell_init();
#endif

#if MYNEWT_VAL(BMP280_CLI)
    bmp280_shell_init();
#endif

#if MYNEWT_VAL(DRV2605_CLI)
    drv2605_shell_init();
#endif

#if MYNEWT_VAL(LIS2DS12_CLI)
    lis2ds12_shell_init();
#endif

#if MYNEWT_VAL(LIS2DW12_CLI)
    lis2dw12_shell_init();
#endif

#if MYNEWT_VAL(KXTJ3_CLI)
    kxtj3_shell_init();
#endif

#if MYNEWT_VAL(ICP101XX_CLI)
    icp101xx_shell_init();
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
    rc = ble_svc_gap_device_name_set("sn");
    assert(rc == 0);

    rc = oc_main_init((oc_handler_t *)&sensor_oic_handler);
    assert(!rc);

    oc_init_platform("MyNewt", NULL, NULL);
    oc_add_device("/oic/d", "oic.d.sensy", "sensy", "1.0", "1.0", NULL,
                  NULL);
    assert(!oc_stack_errno);
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

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif
    /* Initialize OS */
    sysinit();

    /* Initialize blinking led */
    init_blink();

    /* Sensor device shell init */
    sensors_dev_shell_init();

    /* Initialize BLE OIC GATT Server */
    sensor_ble_oic_server_init();

    /* log reboot */
    reboot_start(hal_reset_cause());

    console_printf("\nSensors Test App\n");

    /*
     * As the last thing, process events from default event queue.
     */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return (0);
}
