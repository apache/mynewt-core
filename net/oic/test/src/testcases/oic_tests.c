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
#include <mn_socket/mn_socket.h>
#include "test_oic.h"

/*
 * How long to wait before declaring discovery process failure.
 */
#define OIC_TEST_FAIL_DLY   (OS_TICKS_PER_SEC * 4)

static const char *oic_test_phase;
static struct os_callout oic_test_timer;
static struct oc_server_handle oic_tgt;

static void
oic_test_timer_cb(struct os_event *ev)
{
    TEST_ASSERT_FATAL(0, "test_phase: %s\n",
                      oic_test_phase ? oic_test_phase : "unknwn");
}

void
oic_test_reset_tmo(const char *phase)
{
    oic_test_phase = phase;
    os_callout_reset(&oic_test_timer, OIC_TEST_FAIL_DLY);
}

static void
test_platform_init(void)
{
    oc_init_platform("TestPlatform", NULL, NULL);
}

static void
test_handle_client_requests(void)
{
}

static oc_handler_t test_handler = {
    .init = test_platform_init,
    .requests_entry = test_handle_client_requests
};

void
oic_test_set_endpoint(struct oc_server_handle *ose)
{
    memcpy(&oic_tgt, ose, sizeof(*ose));
}

void
oic_test_get_endpoint(struct oc_server_handle *ose)
{
    memcpy(ose, &oic_tgt, sizeof(*ose));
}

TEST_CASE_TASK(oic_tests)
{
    os_callout_init(&oic_test_timer, os_eventq_dflt_get(), oic_test_timer_cb, NULL);
    oc_evq_set(os_eventq_dflt_get());

    oc_main_init(&test_handler);
    test_discovery();
    test_getset();
    test_observe();
    oc_main_shutdown();
}
