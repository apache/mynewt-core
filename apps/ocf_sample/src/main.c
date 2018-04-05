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
#include "os/mynewt.h"
#include <bsp/bsp.h>
#include <log/log.h>
#include <oic/oc_api.h>
#include <cborattr/cborattr.h>
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
#include <console/console.h>
#include <console/prompt.h>
#include <shell/shell.h>
#endif

#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
#include "host/ble_hs.h"
#include "ocf_sample.h"
#endif

#if (MYNEWT_VAL(OC_CLIENT) == 1)
static void issue_requests(void);
#endif

#if (MYNEWT_VAL(OC_SERVER) == 1)
static bool light_state = false;

static void
get_light(oc_request_t *request, oc_interface_mask_t interface)
{
    printf("GET_light:\n");
    oc_rep_start_root_object();
    switch (interface) {
    case OC_IF_BASELINE:
        oc_process_baseline_interface(request->resource);
    case OC_IF_RW:
        oc_rep_set_boolean(root, state, light_state);
        break;
    default:
        break;
    }
    oc_rep_end_root_object();
    oc_send_response(request, OC_STATUS_OK);
    printf("Light state %d\n", light_state);
}

static void
put_light(oc_request_t *request, oc_interface_mask_t interface)
{
    bool state;
    int len;
    uint16_t data_off;
    struct os_mbuf *m;
    struct cbor_attr_t attrs[] = {
        [0] = {
            .attribute = "state",
            .type = CborAttrBooleanType,
            .addr.boolean = &state,
            .dflt.boolean = false
        },
        [1] = {
        }
    };

    printf("PUT_light:\n");

    len = coap_get_payload(request->packet, &m, &data_off);
    if (cbor_read_mbuf_attrs(m, data_off, len, attrs)) {
        oc_send_response(request, OC_STATUS_BAD_REQUEST);
    } else {
        printf("value: %d\n", state);
        light_state = state;
        oc_send_response(request, OC_STATUS_CHANGED);
    }
}

static void
register_resources(void)
{
    oc_resource_t *res = oc_new_resource("/light/1", 1, 0);
    oc_resource_bind_resource_type(res, "oic.r.light");
    oc_resource_bind_resource_interface(res, OC_IF_RW);
    oc_resource_set_default_interface(res, OC_IF_RW);

    oc_resource_set_discoverable(res);
    oc_resource_set_periodic_observable(res, 1);
    oc_resource_set_request_handler(res, OC_GET, get_light);
    oc_resource_set_request_handler(res, OC_PUT, put_light);
    oc_add_resource(res);
}
#endif

#if (MYNEWT_VAL(OC_CLIENT) == 1)
#define MAX_URI_LENGTH (30)
static char light_1[MAX_URI_LENGTH];
static oc_server_handle_t light_server;
static bool light_state = false;
static struct os_callout callout;

static void
set_device_custom_property(void *data)
{
    oc_set_custom_device_property(purpose, "operate mynewt-light");
}

static void
stop_observe(struct os_event *ev)
{
    printf("Stopping OBSERVE\n");
    oc_stop_observe(light_1, &light_server);
}

static void
put_light(oc_client_response_t *data)
{
    printf("PUT_light:\n");
    if (data->code == OC_STATUS_CHANGED)
        printf("PUT response OK\n");
    else
        printf("PUT response code %d\n", data->code);
}

static void
observe_light(oc_client_response_t *rsp)
{
    bool state;
    int len;
    uint16_t data_off;
    struct os_mbuf *m;
    struct cbor_attr_t attrs[] = {
        [0] = {
            .attribute = "state",
            .type = CborAttrBooleanType,
            .addr.boolean = &state,
            .dflt.boolean = false
        },
        [1] = {
        }
    };

    len = coap_get_payload(rsp->packet, &m, &data_off);
    if (!cbor_read_mbuf_attrs(m, data_off, len, attrs)) {
        printf("OBSERVE_light: %d\n", state);
        light_state = state;
    }

    if (oc_init_put(light_1, &light_server, NULL, &put_light, LOW_QOS)) {
        oc_rep_start_root_object();
        oc_rep_set_boolean(root, state, !light_state);
        oc_rep_end_root_object();
        if (oc_do_put() == true) {
            printf("Sent PUT request\n");
        } else {
            printf("Could not send PUT\n");
        }
    } else {
        printf("Could not init PUT\n");
    }
}

static oc_discovery_flags_t
discovery(const char *di, const char *uri, oc_string_array_t types,
          oc_interface_mask_t interfaces, oc_server_handle_t *server)
{
    int i;
    int uri_len = strlen(uri);
    uri_len = (uri_len >= MAX_URI_LENGTH) ? MAX_URI_LENGTH - 1 : uri_len;

    for (i = 0; i < oc_string_array_get_allocated_size(types); i++) {
        char *t = oc_string_array_get_item(types, i);
        if (strlen(t) == 11 && strncmp(t, "oic.r.light", 11) == 0) {
            memcpy(&light_server, server, sizeof(oc_server_handle_t));

            strncpy(light_1, uri, uri_len);
            light_1[uri_len] = '\0';

            oc_do_observe(light_1, &light_server, NULL, &observe_light,
                          LOW_QOS);
            os_callout_reset(&callout, 30 * OS_TICKS_PER_SEC);
            return OC_STOP_DISCOVERY;
        }
    }
    return OC_CONTINUE_DISCOVERY;
}

static void
issue_requests(void)
{
    oc_do_ip_discovery("oic.r.light", &discovery);
}
#endif

static void
app_init(void)
{
    oc_init_platform("Mynewt", NULL, NULL);
#if (MYNEWT_VAL(OC_CLIENT) == 1)
    oc_add_device("/oic/d", "oic.d.light", "MynewtClient", "1.0", "1.0",
                  set_device_custom_property, NULL);
#endif

#if (MYNEWT_VAL(OC_SERVER) == 1)
    oc_add_device("/oic/d", "oic.d.light", "MynewtServer", "1.0", "1.0", NULL,
                  NULL);
#endif
}

oc_handler_t ocf_handler = {
    .init = app_init,
#if (MYNEWT_VAL(OC_SERVER) == 1)
    .register_resources = register_resources,
#endif
#if (MYNEWT_VAL(OC_CLIENT) == 1)
    .requests_entry = issue_requests,
#endif
 };

static void
ocf_init_tasks(void)
{
#if (MYNEWT_VAL(OC_CLIENT) == 1)
    os_callout_init(&callout, os_eventq_dflt_get(), stop_observe, NULL);
#endif
    oc_main_init(&ocf_handler);
}

int
main(int argc, char **argv)
{
#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    /* Initialize OS */
    sysinit();

#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
    ocf_ble_init();
#endif

    ocf_init_tasks();

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    /* Never returns */

    /* os start should never return. If it does, this should be an error */
    assert(0);
}
