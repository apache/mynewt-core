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

#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "mgmt/mgmt.h"

#include "oic/oc_api.h"
#include "oic/oc_gatt.h"
#include "cborattr/cborattr.h"

#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"

/* Memfault */
#include "memfault/core/platform/device_info.h"
#include "memfault/http/http_client.h"
#include "memfault/metrics/metrics.h"

/* Application-specified header. */
#include "bleprph.h"


void
memfault_platform_get_device_info(sMemfaultDeviceInfo *info)
{
    *info = (sMemfaultDeviceInfo) {
        .device_serial = "DEMOSERIAL",
        .software_type = "app",
        .software_version = "0.0.1",
        .hardware_version = MYNEWT_VAL(BSP_NAME),
    };
}

sMfltHttpClientConfig g_mflt_http_client_config = {
    .api_key = "<YOUR API KEY HERE>",
};

void
memfault_metrics_heartbeat_collect_data(void)
{
    struct os_task *prev_task;
    struct os_task_info oti;
    char *name;
    int found;

    name = "main";
    found = 0;
    prev_task = NULL;

    while (1) {
        prev_task = os_task_info_get_next(prev_task, &oti);
        if (prev_task == NULL) {
            break;
        }

        if (strcmp(name, oti.oti_name) != 0) {
            continue;
        } else {
            found = 1;
            break;
        }
    }

    if (found) {
        memfault_metrics_heartbeat_set_unsigned(
            MEMFAULT_METRICS_KEY(MainTaskStackHwm), oti.oti_stkusage);
    }
}

static void
bleprph_on_reset(int reason)
{
    DFLT_LOG_ERROR("Resetting state; reason=%d\n", reason);
}

static void
bleprph_on_sync(void)
{
    /* Begin advertising. */
    bleprph_advertise();
}

static void
app_get_light(oc_request_t *request, oc_interface_mask_t interface)
{
    bool value;

    if (hal_gpio_read(LED_BLINK_PIN)) {
        value = true;
    } else {
        value = false;
    }
    oc_rep_start_root_object();
    switch (interface) {
    case OC_IF_BASELINE:
        oc_process_baseline_interface(request->resource);
    case OC_IF_A:
        oc_rep_set_boolean(root, value, value);
        break;
    default:
        break;
    }
    oc_rep_end_root_object();
    oc_send_response(request, OC_STATUS_OK);
}

static void
app_set_light(oc_request_t *request, oc_interface_mask_t interface)
{
    bool value;
    int len;
    uint16_t data_off;
    struct os_mbuf *m;
    struct cbor_attr_t attrs[] = {
        [0] = {
            .attribute = "value",
            .type = CborAttrBooleanType,
            .addr.boolean = &value,
            .dflt.boolean = false
        },
        [1] = {
        }
    };

    len = coap_get_payload(request->packet, &m, &data_off);
    if (cbor_read_mbuf_attrs(m, data_off, len, attrs)) {
        oc_send_response(request, OC_STATUS_BAD_REQUEST);
    } else {
        hal_gpio_write(LED_BLINK_PIN, value == true);
        oc_send_response(request, OC_STATUS_CHANGED);
    }
}

static void
omgr_app_init(void)
{
    oc_resource_t *res;

    oc_init_platform("MyNewt", NULL, NULL);
    oc_add_device("/oic/d", "oic.d.light", "MynewtLed", "1.0", "1.0", NULL,
                  NULL);

    res = oc_new_resource("/light/1", 1, 0);
    oc_resource_bind_resource_type(res, "oic.r.switch.binary");
    oc_resource_bind_resource_interface(res, OC_IF_A);
    oc_resource_set_default_interface(res, OC_IF_A);

    oc_resource_set_discoverable(res);
    oc_resource_set_periodic_observable(res, 1);
    oc_resource_set_request_handler(res, OC_GET, app_get_light);
    oc_resource_set_request_handler(res, OC_PUT, app_set_light);
    oc_resource_set_request_handler(res, OC_POST, app_set_light);
    oc_add_resource(res);
}

static const oc_handler_t omgr_oc_handler = {
    .init = omgr_app_init,
};

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

    /* Set initial BLE device address. */
    memcpy(g_dev_addr, (uint8_t[6]) {0x0a, 0xfa, 0xcf, 0xac, 0xfa, 0xc0}, 6);

    /* Initialize OS */
    sysinit();

    /* Initialize the OIC  */
    oc_main_init((oc_handler_t *) &omgr_oc_handler);
    oc_ble_coap_gatt_srv_init();

    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("memfault");
    assert(rc == 0);

    /* Our light resource */
    hal_gpio_init_out(LED_BLINK_PIN, 1);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    /* Never exit */

    return 0;
}
