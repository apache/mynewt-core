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
#include <oic/oc_api.h>
#include "test_oic.h"

static int test_getset_state;
static volatile int test_getset_done;
static struct oc_resource *test_res_getset;

static void test_getset_next_step(struct os_event *);
static struct os_event test_getset_next_ev = {
    .ev_cb = test_getset_next_step
};

static void
test_getset_get(struct oc_request *request, oc_interface_mask_t interface)
{
    switch (test_getset_state) {
    case 1:
        oc_rep_start_root_object();
        oc_rep_end_root_object();
        oc_send_response(request, OC_STATUS_OK);
        break;
    case 2:
        oc_send_response(request, OC_STATUS_OK);
        break;
    case 3:
        oc_send_response(request, OC_STATUS_BAD_REQUEST);
        break;
    }
}

static void
test_getset_put(struct oc_request *request, oc_interface_mask_t interface)
{

}

static void
test_getset_rsp1(struct oc_client_response *rsp)
{
    switch (test_getset_state) {
    case 1:
        TEST_ASSERT(rsp->code == OC_STATUS_OK);
        break;
    case 2:
        TEST_ASSERT(rsp->code == OC_STATUS_OK);
        break;
    case 3:
        TEST_ASSERT(rsp->code == OC_STATUS_BAD_REQUEST);
        break;
    default:
        break;
    }
    os_eventq_put(os_eventq_dflt_get(), &test_getset_next_ev);
}

static void
test_getset_next_step(struct os_event *ev)
{
    bool b_rc;
    struct oc_server_handle server;

    test_getset_state++;
    switch (test_getset_state) {
    case 1:
        test_res_getset = oc_new_resource("/getset", 1, 0);
        TEST_ASSERT_FATAL(test_res_getset);

        oc_resource_bind_resource_interface(test_res_getset, OC_IF_RW);
        oc_resource_set_default_interface(test_res_getset, OC_IF_RW);

        oc_resource_set_request_handler(test_res_getset, OC_GET,
                                        test_getset_get);
        oc_resource_set_request_handler(test_res_getset, OC_PUT,
                                        test_getset_put);
        b_rc = oc_add_resource(test_res_getset);
        TEST_ASSERT(b_rc == true);
        /* fall-through */
    case 2:
    case 3:
        oic_test_get_endpoint(&server);

        b_rc = oc_do_get("/getset", &server, NULL, test_getset_rsp1, LOW_QOS);
        TEST_ASSERT_FATAL(b_rc == true);

        oic_test_reset_tmo("getset");
        break;
    case 4:
        test_getset_done = 1;
        break;
    default:
        TEST_ASSERT_FATAL(0);
        break;
    }
}

void
test_getset(void)
{
    os_eventq_put(os_eventq_dflt_get(), &test_getset_next_ev);
    while (!test_getset_done)
        ;

    oc_delete_resource(test_res_getset);
}


