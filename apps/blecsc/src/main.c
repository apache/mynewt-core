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

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "console/console.h"
#include "config/config.h"
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "blecsc_sens.h"

/* Wheel size for simulation calculations */
#define CSC_SIM_WHEEL_CIRCUMFERENCE_MM            2000
/* Simulated cadence lower limit */
#define CSC_SIM_CRANK_RPM_MIN                     20
/* Simulated cadence upper limit */
#define CSC_SIM_CRANK_RPM_MAX                     100
/* Simulated speed lower limit */
#define CSC_SIM_SPEED_KPH_MIN                     0
/* Simulated speed upper limit */
#define CSC_SIM_SPEED_KPH_MAX                     35

/* Log data */
struct log blehr_log;

static bool notify_state = false;

static const char *device_name = "blecsc_sensor";

static int blehr_gap_event(struct ble_gap_event *event, void *arg);

static uint8_t blehr_addr_type;

/* Sending notify data timer */
static struct os_callout blecsc_tx_timer;

/* Variable holds current CSC measurement state */
static struct ble_csc_measurement_state csc_measurement_state;

/* Variable holds simulted speed (kilometers per hour) */
static uint16_t csc_sim_speed_kph = CSC_SIM_SPEED_KPH_MIN;

/* Variable holds simulated cadence (RPM) */
static uint8_t csc_sim_crank_rpm = CSC_SIM_CRANK_RPM_MIN;

static void 
store_le32_as_u8_arr(uint32_t val, uint8_t * arr)
{
    *arr++ = (val & 0x000000FF);
    val >>= 8;
    *arr++ = (val & 0x000000FF);
    val >>= 8;
    *arr++ = (val & 0x000000FF);
    val >>= 8;
    *arr = (val & 0x000000FF);
}

static void 
store_le16_as_u8_arr(uint32_t val, uint8_t * arr)
{
    *arr++ = (val & 0x000000FF);
    val >>= 8;
    *arr = (val & 0x000000FF);
}



/*
 * Enables advertising with parameters:
 *     o General discoverable mode
 *     o Undirected connectable mode
 */
static void
blehr_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    /*
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info)
     *     o Advertising tx power
     *     o Device name
     */
    memset(&fields, 0, sizeof(fields));

    /*
     * Advertise two flags:
     *      o Discoverability in forthcoming advertisement (general)
     *      o BLE-only (BR/EDR unsupported)
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                    BLE_HS_ADV_F_BREDR_UNSUP;

    /*
     * Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        BLEHR_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(blehr_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, blehr_gap_event, NULL);
    if (rc != 0) {
        BLEHR_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/* Reset heartrate measurment */
static void
blehr_tx_hrate_reset(void)
{
    int rc;

    rc = os_callout_reset(&blecsc_tx_timer, OS_TICKS_PER_SEC);
    assert(rc == 0);
}

/* This functions updates simulated CSC measurements */
static void 
blecsc_simulate_speed_and_cadence()
{
    uint16_t wheel_rev_period;
    uint16_t crank_rev_period;

    /* Update simulated crank and wheel rotation speed */
    csc_sim_speed_kph += 1;
    if (csc_sim_speed_kph >= CSC_SIM_SPEED_KPH_MAX) {
         csc_sim_speed_kph = CSC_SIM_SPEED_KPH_MIN;
    }
    
    csc_sim_crank_rpm++;
    if (csc_sim_crank_rpm >= CSC_SIM_CRANK_RPM_MAX) {
         csc_sim_crank_rpm = CSC_SIM_CRANK_RPM_MIN;
    }
    
    /* Calculate simulated measurement values */
    if (csc_sim_speed_kph > 0){
        wheel_rev_period = (6*6*64*CSC_SIM_WHEEL_CIRCUMFERENCE_MM) / 
                           (625*csc_sim_speed_kph);
        csc_measurement_state.cumulative_wheel_rev++;
        csc_measurement_state.last_wheel_evt_time += wheel_rev_period;
    }
    
    if (csc_sim_crank_rpm > 0){
        crank_rev_period = (60*1024) / csc_sim_crank_rpm;
        csc_measurement_state.cumulative_crank_rev++;
        csc_measurement_state.last_crank_evt_time += crank_rev_period; 
    }    
    
    BLEHR_LOG(INFO, "CSC simulated values: speed = %d kph, cadence = %d \n",
                    csc_sim_speed_kph, csc_sim_crank_rpm);  
}

/* This functions simulates CSC measurement and notifies it to the client */
static void
blehr_tx_csc_measurement(struct os_event *ev)
{
    int rc;
    struct os_mbuf *om;
    uint8_t data_buf[11];
    uint8_t data_offset = 1;
    
    blecsc_simulate_speed_and_cadence();
    
    if (!notify_state) {
         blehr_tx_hrate_reset();
         return;
    }    
    
    memset(data_buf, 0, sizeof(data_buf));

#if (CSC_FEATURES & CSC_FEATURE_WHEEL_REV_DATA)
    data_buf[0] |= CSC_MEASUREMENT_WHEEL_REV_PRESENT;
    store_le32_as_u8_arr(csc_measurement_state.cumulative_wheel_rev, 
                         &(data_buf[1]));
    store_le16_as_u8_arr(csc_measurement_state.last_wheel_evt_time, 
                         &(data_buf[5]));
    data_offset += 6;
#endif

#if (CSC_FEATURES & CSC_FEATURE_CRANK_REV_DATA)
    data_buf[0] |= CSC_MEASUREMENT_CRANK_REV_PRESENT;
    store_le16_as_u8_arr(csc_measurement_state.cumulative_crank_rev, 
                         &(data_buf[data_offset]));  
    store_le16_as_u8_arr(csc_measurement_state.last_crank_evt_time, 
                         &(data_buf[data_offset + 2]));
    data_offset += 4;                         
#endif  
    
    om = ble_hs_mbuf_from_flat(data_buf, data_offset);

    rc = ble_gattc_notify_custom(notify_state, csc_measurement_handle, om);

    assert(rc == 0);
    blehr_tx_hrate_reset();
}

static int
blehr_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed */
        BLEHR_LOG(INFO, "connection %s; status=%d\n",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising */
            blehr_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        BLEHR_LOG(INFO, "disconnect; reason=%d\n", event->disconnect.reason);

        /* Connection terminated; resume advertising */
        blehr_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        BLEHR_LOG(INFO, "adv complete\n");
        blehr_advertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        BLEHR_LOG(INFO, "subscribe event; cur_notify=%d\n value handle; "
                  "val_handle=%d\n",
                        event->subscribe.cur_notify,
                        csc_measurement_handle);
        if (event->subscribe.attr_handle == csc_measurement_handle) {
            notify_state = event->subscribe.cur_notify;
            blehr_tx_hrate_reset();
        } else {
            notify_state = event->subscribe.cur_notify;
       //     blehr_tx_hrate_stop();
        }
        break;

    case BLE_GAP_EVENT_MTU:
        BLEHR_LOG(INFO, "mtu update event; conn_handle=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.value);
        break;

    }

    return 0;
}

static void
blehr_on_sync(void)
{
    int rc;

    /* Use privacy */
    rc = ble_hs_id_infer_auto(0, &blehr_addr_type);
    assert(rc == 0);

    /* Begin advertising */
    blehr_advertise();
}

/*
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

    /* Initialize the blecsc log */
    log_register("blecsc_sens_log", &blehr_log, &log_console_handler, NULL,
                    LOG_SYSLEVEL);

    /* Initialize the NimBLE host configuration */
    log_register("blecsc_sens", &ble_hs_log, &log_console_handler, NULL,
                    LOG_SYSLEVEL);
    ble_hs_cfg.sync_cb = blehr_on_sync;

    os_callout_init(&blecsc_tx_timer, os_eventq_dflt_get(),
                    blehr_tx_csc_measurement, NULL);

    rc = gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name */
    rc = ble_svc_gap_device_name_set(device_name);
    assert(rc == 0);

    /* As the last thing, process events from default event queue */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    return 0;
}

