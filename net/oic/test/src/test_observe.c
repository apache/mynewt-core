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
#include "test_oic.h"

#include <os/os.h>
#include <oic/oc_api.h>

#include <cborattr/cborattr.h>

static int test_observe_state;
static volatile int test_observe_done;
static struct oc_resource *test_res_observe;

static void test_observe_next_step(struct os_event *);
static struct os_event test_observe_next_ev = {
    .ev_cb = test_observe_next_step
};

static void
test_observe_get(struct oc_request *request, oc_interface_mask_t interface)
{
    oc_rep_start_root_object();
    switch (interface) {
    case OC_IF_BASELINE:
        oc_process_baseline_interface(request->resource);
    case OC_IF_R:
        switch (test_observe_state) {
        case 1:
        case 2:
            /* initial get request */
        case 3:
        case 4:
        case 5:
        case 6:
            oc_rep_set_int(root, value, test_observe_state);
            break;
        default:
            break;
        }
    default:
        break;
    }
    oc_rep_end_root_object();
    oc_send_response(request, OC_STATUS_OK);
}

static void
test_observe_rsp(struct oc_client_response *rsp)
{
    long long rsp_value;
    struct cbor_attr_t attrs[] = {
        [0] = {
            .attribute = "state",
            .type = CborAttrIntegerType,
            .addr.integer = &rsp_value,
            .dflt.integer = 0
        },
        [1] = {
        }
    };
    struct os_mbuf *m;
    uint16_t data_off;
    int len;

    switch (test_observe_state) {
    case 1:
        TEST_ASSERT(rsp->code == OC_STATUS_NOT_FOUND);
        break;
    case 2:
    case 3:
    case 4:
        TEST_ASSERT(rsp->code == OC_STATUS_OK);
        len = coap_get_payload(rsp->packet, &m, &data_off);
        if (cbor_read_mbuf_attrs(m, data_off, len, attrs)) {
            TEST_ASSERT(rsp_value == test_observe_state);
        }
        break;
    default:
        break;
    }
    os_eventq_put(&oic_tapp_evq, &test_observe_next_ev);
}

static void
test_observe_next_step(struct os_event *ev)
{
    bool b_rc;
    int rc;
    struct oc_server_handle server;

    test_observe_state++;
    switch (test_observe_state) {
    case 1:
        test_res_observe = oc_new_resource("/observe", 1, 0);
        TEST_ASSERT_FATAL(test_res_observe);

        oc_resource_bind_resource_interface(test_res_observe, OC_IF_R);
        oc_resource_set_default_interface(test_res_observe, OC_IF_R);
        oc_resource_set_observable(test_res_observe);

        oc_resource_set_request_handler(test_res_observe, OC_GET,
                                        test_observe_get);
        b_rc = oc_add_resource(test_res_observe);
        TEST_ASSERT(b_rc == true);

        /*
         * Observe nonexistent URI
         */
        oic_test_get_endpoint(&server);
        b_rc = oc_do_observe("/observe_wrong", &server, NULL, test_observe_rsp,
          LOW_QOS);
        TEST_ASSERT_FATAL(b_rc == true);

        oic_test_reset_tmo("observe1");
        break;
    case 2:
        oic_test_get_endpoint(&server);
        b_rc = oc_do_observe("/observe", &server, NULL, test_observe_rsp,
                             LOW_QOS);
        TEST_ASSERT_FATAL(b_rc == true);

        oic_test_reset_tmo("observe2");
        break;
    case 3:
    case 4:
        /*
         * Valid notifications
         */
        rc = oc_notify_observers(test_res_observe);
        TEST_ASSERT(rc == 1); /* one observer */
        oic_test_reset_tmo("observe3-4");
        break;
    case 5:
        test_observe_done = 1;
        break;
    default:
        TEST_ASSERT_FATAL(0);
        break;
    }
}

void
test_observe(void)
{
    os_eventq_put(&oic_tapp_evq, &test_observe_next_ev);

    while (!test_observe_done) {
        os_eventq_run(&oic_tapp_evq);
    }
    oc_delete_resource(test_res_observe);
}


