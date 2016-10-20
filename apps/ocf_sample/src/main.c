/**
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
#include "sysinit/sysinit.h"
#include <os/os.h>
#include <sysinit/sysinit.h>
#include <bsp/bsp.h>
#include <log/log.h>
#include <hal/hal_cputime.h>
#include <oic/oc_api.h>
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
#include <console/console.h>
#include <console/prompt.h>
#include <shell/shell.h>
#endif

#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1)
#include "mn_socket/mn_socket.h"
#include "mn_socket/arch/sim/native_sock.h"
#endif

#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
#include <oic/oc_gatt.h>
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "controller/ble_ll.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#endif

#define OCF_TASK_PRIO      (8)
#define OCF_TASK_STACK_SIZE (OS_STACK_ALIGN(512))
static os_stack_t ocf_stack[OCF_TASK_STACK_SIZE];
struct os_task ocf_task;

#if (MYNEWT_VAL(OC_CLIENT) == 1)
static void issue_requests(void);
#endif

#if (MYNEWT_VAL(OC_SERVER) == 1)
static bool light_state = false;

static void
get_light(oc_request_t *request, oc_interface_mask_t interface)
{
  PRINT("GET_light:\n");
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
  PRINT("Light state %d\n", light_state);
}

static void
put_light(oc_request_t *request, oc_interface_mask_t interface)
{
  PRINT("PUT_light:\n");
  bool state = false;
  oc_rep_t *rep = request->request_payload;
  while (rep != NULL) {
    PRINT("key: %s ", oc_string(rep->name));
    switch (rep->type) {
    case BOOL:
      state = rep->value_boolean;
      PRINT("value: %d\n", state);
      break;
    default:
      oc_send_response(request, OC_STATUS_BAD_REQUEST);
      return;
      break;
    }
    rep = rep->next;
  }
  oc_send_response(request, OC_STATUS_CHANGED);
  light_state = state;
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

static void
set_device_custom_property(void *data)
{
  oc_set_custom_device_property(purpose, "operate mynewt-light");
}

static oc_event_callback_retval_t
stop_observe(void *data)
{
  PRINT("Stopping OBSERVE\n");
  oc_stop_observe(light_1, &light_server);
  return DONE;
}

static void
put_light(oc_client_response_t *data)
{
  PRINT("PUT_light:\n");
  if (data->code == OC_STATUS_CHANGED)
    PRINT("PUT response OK\n");
  else
    PRINT("PUT response code %d\n", data->code);
}

static void
observe_light(oc_client_response_t *data)
{
  PRINT("OBSERVE_light:\n");
  oc_rep_t *rep = data->payload;
  while (rep != NULL) {
    PRINT("key %s, value ", oc_string(rep->name));
    switch (rep->type) {
    case BOOL:
      PRINT("%d\n", rep->value_boolean);
      light_state = rep->value_boolean;
      break;
    default:
      break;
    }
    rep = rep->next;
  }

  if (oc_init_put(light_1, &light_server, NULL, &put_light, LOW_QOS)) {
    oc_rep_start_root_object();
    oc_rep_set_boolean(root, state, !light_state);
    oc_rep_end_root_object();
    if (oc_do_put())
      PRINT("Sent PUT request\n");
    else
      PRINT("Could not send PUT\n");
  } else
    PRINT("Could not init PUT\n");
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

      oc_do_observe(light_1, &light_server, NULL, &observe_light, LOW_QOS);
      oc_set_delayed_callback(NULL, &stop_observe, 30);
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
    oc_add_device("/oic/d", "oic.d.light", "MynewtServer", "1.0", "1.0", NULL, NULL);
#endif
}

 oc_handler_t ocf_handler = {.init = app_init,
#if (MYNEWT_VAL(OC_SERVER) == 1)
                          .register_resources = register_resources,
#endif
#if (MYNEWT_VAL(OC_CLIENT) == 1)
                          .requests_entry = issue_requests,
#endif
 };

struct os_sem ocf_main_loop_sem;

 void
oc_signal_main_loop(void) {
     os_sem_release(&ocf_main_loop_sem);
}

void ocf_task_handler(void *arg) {

    os_sem_init(&ocf_main_loop_sem, 1);

    while (1) {
        uint32_t ticks;
        oc_clock_time_t next_event;
        next_event = oc_main_poll();
        ticks = oc_clock_time();
        if (next_event == 0) {
            ticks = OS_TIMEOUT_NEVER;
        } else {
            ticks = next_event - ticks;
        }
        os_sem_pend(&ocf_main_loop_sem, ticks);
    }
    oc_main_shutdown();
}

void
ocf_task_init(void) {

    int rc;
    rc = os_task_init(&ocf_task, "ocf", ocf_task_handler, NULL,
            OCF_TASK_PRIO, OS_WAIT_FOREVER, ocf_stack, OCF_TASK_STACK_SIZE);
    assert(rc == 0);

    oc_main_init(&ocf_handler);
}

static const uint8_t addr[6] = {1,2,3,4,5,6};

int
main(int argc, char **argv)
{
    int rc;
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
    struct os_eventq *ev;
#endif
    /* Initialize OS */
    sysinit();

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(1000000);
    assert(rc == 0);

#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1)
    rc = native_sock_init();
    assert(rc == 0);
#endif

#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
    memcpy(g_dev_addr, addr, sizeof(g_dev_addr));

    /* COAP Gatt server initialization */
    rc = ble_coap_gatt_srv_init(&ev);
    assert(rc == 0);

#if (MYNEWT_VAL(OC_CLIENT) == 1)
    /* TODO INIT CLIENT */
#endif
    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("Mynewt_OCF");
    assert(rc == 0);

    /* Initialize the BLE host. */
    log_register("ble_hs", &ble_hs_log, &log_console_handler, NULL, LOG_SYSLEVEL);
    ble_hs_cfg.parent_evq = ev;
#endif

    ocf_task_init();

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);
}
