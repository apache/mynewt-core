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

#include "port/oc_assert.h"
#include "port/oc_clock.h"
#include "port/oc_connectivity.h"

#include "util/oc_etimer.h"
#include "util/oc_process.h"

#include "oc_api.h"

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

  if (initialized == true)
    return 0;

  oc_ri_init();

#ifdef OC_SECURITY
  handler->get_credentials();

  oc_sec_load_pstat();
  oc_sec_load_doxm();
  oc_sec_load_cred();

  oc_sec_dtls_init_context();
#endif

  oc_network_event_handler_mutex_init();
  ret = oc_connectivity_init();
  if (ret < 0)
    goto err;

  handler->init();

#ifdef OC_SERVER
  handler->register_resources();
#endif

#ifdef OC_SECURITY
  oc_sec_create_svr();
  oc_sec_load_acl();
#endif

  if (oc_stack_errno != 0) {
    ret = -oc_stack_errno;
    goto err;
  }

  PRINT("oc_main: Stack successfully initialized\n");

#ifdef OC_CLIENT
  handler->requests_entry();
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
  oc_clock_time_t ticks_until_next_event = oc_etimer_request_poll();
  while (oc_process_run()) {
    ticks_until_next_event = oc_etimer_request_poll();
  }
  return ticks_until_next_event;
}

void
oc_main_shutdown(void)
{
  if (initialized == false) {
    PRINT("tiny_ocf is not initialized\n");
    return;
  }

  oc_connectivity_shutdown();
  oc_ri_shutdown();

#ifdef OC_SECURITY /* fix ensure this gets executed on constraied platforms */
  oc_sec_dump_state();
#endif

  initialized = false;
}
