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

#define OIC_TAPP_PRIO       9
#define OIC_TAPP_STACK_SIZE 1024

/*
 * How long to wait before declaring discovery process failure.
 */
#define OIC_TEST_FAIL_DLY   (OS_TICKS_PER_SEC * 2)

static int oic_test_state;

static struct os_task oic_tapp;
static os_stack_t oic_tapp_stack[OS_STACK_ALIGN(OIC_TAPP_STACK_SIZE)];
static struct os_eventq oic_tapp_evq;
static void oic_test_next_step(struct os_event *);
static struct os_event oic_test_next_step_ev = {
    .ev_cb = oic_test_next_step
};
static struct os_callout oic_test_timer;

/*
 * Discovery.
 */
static void
oic_test_discover_init(void)
{
    oc_init_platform("TestPlatform", NULL, NULL);
}

static void
oic_test_discover_client_requests(void)
{
}

static oc_handler_t oic_test_discover_handler = {
    .init = oic_test_discover_init,
    .requests_entry = oic_test_discover_client_requests
};

static oc_discovery_flags_t
discover_cb(const char *di, const char *uri, oc_string_array_t types,
            oc_interface_mask_t interfaces, oc_server_handle_t *server)
{
    if ((server->endpoint.oe.flags & IP) == 0) {
        return 0;
    }
    switch (oic_test_state) {
    case 1:
        TEST_ASSERT(!strcmp(uri, "/oic/p"));
        TEST_ASSERT(interfaces == (OC_IF_BASELINE | OC_IF_R));
        os_eventq_put(&oic_tapp_evq, &oic_test_next_step_ev);
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
            os_eventq_put(&oic_tapp_evq, &oic_test_next_step_ev);
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
            os_eventq_put(&oic_tapp_evq, &oic_test_next_step_ev);
            /*
             * Done.
             */
            tu_restart();
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
oic_test_timer_cb(struct os_event *ev)
{
    TEST_ASSERT_FATAL(0);
}

static void
oic_light_get(oc_request_t *req, oc_interface_mask_t interface)
{
}

static void
oic_test_next_step(struct os_event *ev)
{
    oic_test_state++;
    switch (oic_test_state) {
    case 1:
        /*
         * No resources registered yet.
         */
        oc_do_ip_discovery(NULL, discover_cb);
        os_callout_reset(&oic_test_timer, OIC_TEST_FAIL_DLY);
        break;
    case 2:
        oc_add_device("/oic/d", "oic.d.light", "TestDev", "1.0", "1.1",
          NULL, NULL);
        oc_do_ip_discovery(NULL, discover_cb);
        os_callout_reset(&oic_test_timer, OIC_TEST_FAIL_DLY);
        break;
    case 3: {
        oc_resource_t *res = oc_new_resource("/light/test", 1, 0);

        oc_resource_bind_resource_type(res, "oic.r.light");
        oc_resource_bind_resource_interface(res, OC_IF_RW);
        oc_resource_set_default_interface(res, OC_IF_RW);

        oc_resource_set_discoverable(res);
        oc_resource_set_request_handler(res, OC_GET, oic_light_get);
        oc_add_resource(res);
        oc_do_ip_discovery(NULL, discover_cb);
        os_callout_reset(&oic_test_timer, OIC_TEST_FAIL_DLY);
        break;
    }
    default:
        TEST_ASSERT(0);
        break;
    }
}


static void
oic_test_handler(void *arg)
{
    oc_main_init(&oic_test_discover_handler);

    while (1) {
        os_eventq_run(&oic_tapp_evq);
    }
}

static void
oic_test_init(void)
{
    int rc;

    os_eventq_init(&oic_tapp_evq);

    /*
     * Starts tests.
     */
    os_eventq_put(&oic_tapp_evq, &oic_test_next_step_ev);
    os_callout_init(&oic_test_timer, &oic_tapp_evq, oic_test_timer_cb, NULL);

    rc = os_task_init(&oic_tapp, "oic_test", oic_test_handler, NULL,
                      OIC_TAPP_PRIO, OS_WAIT_FOREVER,
                      oic_tapp_stack, OS_STACK_ALIGN(OIC_TAPP_STACK_SIZE));
    TEST_ASSERT_FATAL(rc == 0);
    oc_evq_set(&oic_tapp_evq);

    os_start();
}

TEST_CASE(oic_test_discover)
{
    oic_test_init();
}
