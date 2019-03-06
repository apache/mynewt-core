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
#include <oic/port/mynewt/ip.h>
#include "test_oic.h"

static int test_discovery_state;
static volatile int test_discovery_done;
static struct oc_resource *test_res_light;

static void test_discovery_next_step(struct os_event *);
static struct os_event test_discovery_next_ev = {
    .ev_cb = test_discovery_next_step
};

/*
 * Discovery.
 */
static oc_discovery_flags_t
test_discovery_cb(const char *di, const char *uri, oc_string_array_t types,
                  oc_interface_mask_t interfaces,
                  struct oc_server_handle *server)
{
    if (server->endpoint.ep.oe_type != oc_ip6_transport_id) {
        return 0;
    }
    oic_test_set_endpoint(server);
    switch (test_discovery_state) {
    case 1:
        TEST_ASSERT(!strcmp(uri, "/oic/p"));
        TEST_ASSERT(interfaces == (OC_IF_BASELINE | OC_IF_R));
        os_eventq_put(os_eventq_dflt_get(), &test_discovery_next_ev);
        return OC_STOP_DISCOVERY;
        break;
    case 2: {
        static int seen_p = 0;
        static int seen_d = 0;

        if (!strcmp(uri, "/oic/p")) {
            seen_p = 1;
        } else if (!strcmp(uri, "/oic/d")) {
            seen_d = 1;
        } else {
            TEST_ASSERT(0);
        }
        if (seen_p && seen_d) {
            os_eventq_put(os_eventq_dflt_get(), &test_discovery_next_ev);
            return OC_STOP_DISCOVERY;
        } else {
            return OC_CONTINUE_DISCOVERY;
        }
    }
    case 3: {
        static int seen_p = 0;
        static int seen_d = 0;
        static int seen_light = 0;

        if (!strcmp(uri, "/oic/p")) {
            seen_p = 1;
        } else if (!strcmp(uri, "/oic/d")) {
            seen_d = 1;
        } else if (!strcmp(uri, "/light/test")) {
            seen_light = 1;
        } else {
            TEST_ASSERT(0);
        }
        if (seen_p && seen_d && seen_light) {
            /*
             * Done.
             */
            test_discovery_done = 1;
            return OC_STOP_DISCOVERY;
        } else {
            return OC_CONTINUE_DISCOVERY;
        }
    }
    default:
        break;
    }
    TEST_ASSERT_FATAL(0);
    return 0;
}

static void
test_discovery_get(oc_request_t *req, oc_interface_mask_t interface)
{
}

static void
test_discovery_next_step(struct os_event *ev)
{
    test_discovery_state++;
    switch (test_discovery_state) {
    case 1:
        /*
         * No resources registered yet.
         */
        oc_do_ip_discovery(NULL, test_discovery_cb);
        oic_test_reset_tmo("1st discovery");
        break;
    case 2:
        oc_add_device("/oic/d", "oic.d.light", "TestDev", "1.0", "1.1",
          NULL, NULL);
        oc_do_ip_discovery(NULL, test_discovery_cb);
        oic_test_reset_tmo("2nd discovery");
        break;
    case 3: {
        test_res_light = oc_new_resource("/light/test", 1, 0);

        oc_resource_bind_resource_type(test_res_light, "oic.r.light");
        oc_resource_bind_resource_interface(test_res_light, OC_IF_RW);
        oc_resource_set_default_interface(test_res_light, OC_IF_RW);

        oc_resource_set_discoverable(test_res_light);
        oc_resource_set_request_handler(test_res_light, OC_GET,
                                        test_discovery_get);
        oc_add_resource(test_res_light);
        oc_do_ip_discovery(NULL, test_discovery_cb);
        oic_test_reset_tmo("3rd discovery");
        break;
    }
    default:
        TEST_ASSERT(0);
        break;
    }
}

void
test_discovery(void)
{
    os_eventq_put(os_eventq_dflt_get(), &test_discovery_next_ev);
    while (!test_discovery_done)
        ;

    oc_delete_resource(test_res_light);
}
