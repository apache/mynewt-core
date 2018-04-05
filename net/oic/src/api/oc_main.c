/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <stdint.h>
#include <stdio.h>

#include "os/mynewt.h"

#include "oic/port/mynewt/config.h"
#include "port/oc_assert.h"
#include "port/oc_clock.h"
#include "oic/port/oc_connectivity.h"

#include "oic/oc_api.h"
#include "oic/oc_log.h"

#ifdef OC_SECURITY
#include "security/oc_dtls.h"
#include "security/oc_store.h"
#include "security/oc_svr.h"
#endif /* OC_SECURITY */

static bool initialized = false;

int
oc_main_init(oc_handler_t *handler)
{
    int ret;
    extern int oc_stack_errno;

    if (initialized == true) {
        return 0;
    }
    oc_ri_init();

#ifdef OC_SECURITY
    handler->get_credentials();

    oc_sec_load_pstat();
    oc_sec_load_doxm();
    oc_sec_load_cred();

    oc_sec_dtls_init_context();
#endif

    ret = oc_connectivity_init();
    if (ret < 0) {
        goto err;
    }
    handler->init();

#ifdef OC_SERVER
    if (handler->register_resources) {
        handler->register_resources();
    }
#endif

#ifdef OC_SECURITY
    oc_sec_create_svr();
    oc_sec_load_acl();
#endif

    if (oc_stack_errno != 0) {
        ret = -oc_stack_errno;
        goto err;
    }

    OC_LOG_INFO("oic: Initialized\n");

#ifdef OC_CLIENT
    if (handler->requests_entry) {
        handler->requests_entry();
    }
#endif

    initialized = true;
    return 0;

err:
    oc_abort("oc_main: Error in stack initialization\n");
    return ret;
}

oc_clock_time_t
oc_main_poll(void)
{
    return 0;
}

void
oc_main_shutdown(void)
{
    if (initialized == false) {
        OC_LOG_ERROR("oic: not initialized\n");
        return;
    }

    oc_connectivity_shutdown();
    oc_ri_shutdown();

#ifdef OC_SECURITY /* fix ensure this gets executed on constrained platforms */
    oc_sec_dump_state();
#endif

    initialized = false;
}
