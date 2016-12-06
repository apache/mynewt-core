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
#include "bsp/bsp.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"

/* BLE */
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"

/* ADC */
#include "nrf.h"
#include "app_util_platform.h"
#include "app_error.h"
#include <adc/adc.h>
#include <adc_nrf52/adc_nrf52.h>
#include <adc_nrf52/adc_nrf52.h>
#include "nrf_drv_saadc.h"

nrf_drv_saadc_config_t adc_config = NRF_DRV_SAADC_DEFAULT_CONFIG;

/* Application-specified header. */
#include "bleadc.h"

/** Log data. */
struct log bleadc_log;

/** bleadc task settings. */
#define BLEADC_TASK_PRIO           1
#define BLEADC_STACK_SIZE          (OS_STACK_ALIGN(336))

/* ADC Task settings */
#define ADC_TASK_PRIO           5
#define ADC_STACK_SIZE          (OS_STACK_ALIGN(336))
struct os_task adc_task;
bssnz_t os_stack_t adc_stack[ADC_STACK_SIZE];

/* SPI Task settings */
#define SPI_TASK_PRIO           5
#define SPI_STACK_SIZE          (OS_STACK_ALIGN(336))
struct os_task spi_task;
bssnz_t os_stack_t spi_stack[SPI_STACK_SIZE];

struct os_eventq bleadc_evq;
struct os_task bleadc_task;
bssnz_t os_stack_t bleadc_stack[BLEADC_STACK_SIZE];

static int bleadc_gap_event(struct ble_gap_event *event, void *arg);

#define ADC_NUMBER_SAMPLES (2)
#define ADC_NUMBER_CHANNELS (1)

uint8_t *sample_buffer1;
uint8_t *sample_buffer2;






/**
 * Logs information about a connection to the console.
 */
static void
bleadc_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    BLEADC_LOG(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr_type);
    print_addr(desc->our_ota_addr);
    BLEADC_LOG(INFO, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr_type);
    print_addr(desc->our_id_addr);
    BLEADC_LOG(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr_type);
    print_addr(desc->peer_ota_addr);
    BLEADC_LOG(INFO, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr_type);
    print_addr(desc->peer_id_addr);
    BLEADC_LOG(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
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
bleadc_advertise(void)
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

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;
    uint16_t ble16 = ble_uuid_128_to_16(gatt_svr_svc_sns_uuid);
    fields.uuids16 = (uint16_t[]){ ble16};
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        BLEADC_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_ADDR_TYPE_PUBLIC, 0, NULL, BLE_HS_FOREVER,
                           &adv_params, bleadc_gap_event, NULL);
    if (rc != 0) {
        BLEADC_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleadc uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  bleadc.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bleadc_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        BLEADC_LOG(INFO, "connection %s; status=%d ",
                       event->connect.status == 0 ? "established" : "failed",
                       event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleadc_print_conn_desc(&desc);
        }
        BLEADC_LOG(INFO, "\n");

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            bleadc_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        BLEADC_LOG(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        bleadc_print_conn_desc(&event->disconnect.conn);
        BLEADC_LOG(INFO, "\n");

        /* Connection terminated; resume advertising. */
        bleadc_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        BLEADC_LOG(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        bleadc_print_conn_desc(&desc);
        BLEADC_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        BLEADC_LOG(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        bleadc_print_conn_desc(&desc);
        BLEADC_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        BLEADC_LOG(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
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
        BLEADC_LOG(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;
    }

    return 0;
}

static void
bleadc_on_reset(int reason)
{
    BLEADC_LOG(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
bleadc_on_sync(void)
{
    /* Begin advertising. */
    bleadc_advertise();
}

int
adc_read_event(struct adc_dev *dev, void *arg, uint8_t etype,
        void *buffer, int buffer_len)
{
    int i;
    int adc_result;
    int my_result_mv;
    uint16_t chr_val_handle;
    int rc;
    for (i = 0; i < ADC_NUMBER_SAMPLES; i++) {
        rc = adc_buf_read(dev, buffer, buffer_len, i, &adc_result);
        if (rc != 0) {
            goto err;
        }
        
        my_result_mv = adc_result_mv(dev, 0, adc_result);
        gatt_adc_val = my_result_mv;
        rc = ble_gatts_find_chr(gatt_svr_svc_sns_uuid, BLE_UUID16(ADC_SNS_VAL), NULL, &chr_val_handle);
        assert(rc == 0);
        ble_gatts_chr_updated(chr_val_handle);
    }

    adc_buf_release(dev, buffer, buffer_len);
    return (0);
err:
    return (rc);
}

/**
 * Event loop for the main bleadc task.
 */
static void
bleadc_task_handler(void *unused)
{
    while (1) {
        os_eventq_run(&bleadc_evq);
    }
}

/**
 * Event loop for the ADC task.
 */
static void
adc_task_handler(void *unused)
{
    
    struct adc_dev *adc;
    nrf_saadc_channel_config_t cc =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN1);
    cc.gain = NRF_SAADC_GAIN1_6;
    cc.reference = NRF_SAADC_REFERENCE_INTERNAL;
    adc = (struct adc_dev *) os_dev_open("adc0", 0, &adc_config);
    assert(adc != NULL);
    
    adc_chan_config(adc, 0, &cc);
    
    sample_buffer1 = malloc(adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    sample_buffer2 = malloc(adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    memset(sample_buffer1, 0, adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    memset(sample_buffer2, 0, adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    
    adc_buf_set(adc, sample_buffer1, sample_buffer2,
            adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    adc_event_handler_set(adc, adc_read_event, (void *) NULL);
    
    while (1) {
        adc_sample(adc);
        /* Wait 2 second */
        os_time_delay(OS_TICKS_PER_SEC * 2);

    }
}

/**
 * Event loop for the SPI task.
 */
static void
spi_task_handler(void *unused)
{
    
    gatt_spi_val = 10;
    int rc = 0;
    
    
    while (1) {
        gatt_spi_val += 4;
        uint16_t chr_val_handle;
        
        rc = ble_gatts_find_chr(gatt_svr_svc_sns_uuid,
                                  BLE_UUID16(SPI_SNS_VAL),
                                  NULL, &chr_val_handle);
        assert(rc == 0);
        ble_gatts_chr_updated(chr_val_handle);
        //ble_gatts_chr_updated(24);
        /* Wait 2 second */
        os_time_delay(OS_TICKS_PER_SEC*3);
        
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
    int rc;

    /* Set initial BLE device address. */
    memcpy(g_dev_addr, (uint8_t[6]){0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a}, 6);

    /* Initialize OS */
    sysinit();

    
    /* Initialize the bleadc log. */
    log_register("bleadc", &bleadc_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);

    /* Initialize eventq */
    os_eventq_init(&bleadc_evq);

    /* Create the bleadc task.  All application logic and NimBLE host
     * operations are performed in this task.
     */
    os_task_init(&bleadc_task, "bleadc", bleadc_task_handler,
                 NULL, BLEADC_TASK_PRIO, OS_WAIT_FOREVER,
                 bleadc_stack, BLEADC_STACK_SIZE);

    os_task_init(&adc_task, "adc", adc_task_handler,
                              NULL, ADC_TASK_PRIO, OS_WAIT_FOREVER,
                              adc_stack, ADC_STACK_SIZE);
    os_task_init(&spi_task, "adc", spi_task_handler,
                            NULL, SPI_TASK_PRIO, OS_WAIT_FOREVER,
                            spi_stack, SPI_STACK_SIZE);
    /* Initialize the NimBLE host configuration. */
    log_register("ble_hs", &ble_hs_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);
    ble_hs_cfg.reset_cb = bleadc_on_reset;
    ble_hs_cfg.sync_cb = bleadc_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;

    rc = gatt_svr_init();
    assert(rc == 0);



    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("nimble-bleadc");
    assert(rc == 0);

    /* Set the default eventq for packages that lack a dedicated task. */
    os_eventq_dflt_set(&bleadc_evq);


   
   // BLEADC_LOG(INFO, "Created the ADC device\n");
    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}
