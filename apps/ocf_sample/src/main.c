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
#include <os/os.h>
#include <bsp/bsp.h>
#include <console/console.h>
#include <console/prompt.h>
#include <shell/shell.h>
#include <log/log.h>
#include <hal/hal_cputime.h>
#include <iotivity/oc_api.h>
#include "mn_socket/mn_socket.h"
#include "mn_socket/arch/sim/native_sock.h"

#ifdef OC_TRANSPORT_GATT

#include <iotivity/oc_gatt.h>

/* BLE */
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_uuid.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_l2cap.h"
#include "host/ble_sm.h"
#include "controller/ble_ll.h"

/* RAM HCI transport. */
#include "transport/ram/ble_hci_ram.h"

/* RAM persistence layer. */
#include "store/ram/ble_store_ram.h"

/* Mandatory services. */
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

uint8_t g_random_addr[6] = {6,5,4,3,2,1};
uint8_t g_dev_addr[6] = {1,2,3,4,5,6};

/** Priority of the nimble host and controller tasks. */
#define BLE_LL_TASK_PRI             (OS_TASK_PRI_HIGHEST)

#endif

/* Shell */
#ifdef OC_TRANSPORT_SERIAL
#define SHELL_TASK_PRIO         (8)
#define SHELL_MAX_INPUT_LEN     (512)
#define SHELL_TASK_STACK_SIZE (OS_STACK_ALIGN(2048))
static os_stack_t shell_stack[SHELL_TASK_STACK_SIZE];
#endif

#define OCF_TASK_PRIO      (8)
#define OCF_TASK_STACK_SIZE (OS_STACK_ALIGN(2048))
static os_stack_t ocf_stack[OCF_TASK_STACK_SIZE];
struct os_task ocf_task;

#ifdef OC_TRANSPORT_GATT
#define MBUF_PAYLOAD_SIZE BLE_MBUF_PAYLOAD_SIZE
#else
#define MBUF_PAYLOAD_SIZE 128
#endif

#define DEFAULT_MBUF_MPOOL_BUF_LEN OS_ALIGN(MBUF_PAYLOAD_SIZE, 4)
#define DEFAULT_MBUF_MPOOL_NBUFS (12)

static uint8_t default_mbuf_mpool_data[DEFAULT_MBUF_MPOOL_BUF_LEN *
    DEFAULT_MBUF_MPOOL_NBUFS];

static struct os_mbuf_pool default_mbuf_pool;
static struct os_mempool default_mbuf_mpool;

#ifdef OC_CLIENT
static void issue_requests(void);
#endif

#ifdef OC_SERVER
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

#ifdef OC_CLIENT
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
#ifdef OC_CLIENT
  oc_add_device("/oic/d", "oic.d.phone", "MynewtClient", "1.0", "1.0",
                set_device_custom_property, NULL);
#endif

#ifdef OC_SERVER
    oc_add_device("/oic/d", "oic.d.light", "MynewtServer", "1.0", "1.0", NULL, NULL);
#endif
}


 oc_handler_t ocf_handler = {.init = app_init,
#ifdef OC_SERVER
                          .register_resources = register_resources,
#endif
#ifdef OC_CLIENT
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

int
main(int argc, char **argv)
{
    int rc;
#ifdef OC_TRANSPORT_GATT
    struct os_eventq *ev;
    struct ble_hci_ram_cfg hci_cfg;
    struct ble_hs_cfg cfg;
#endif
    /* Initialize OS */
    os_init();

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(1000000);
    assert(rc == 0);


    rc = os_mempool_init(&default_mbuf_mpool, DEFAULT_MBUF_MPOOL_NBUFS,
            DEFAULT_MBUF_MPOOL_BUF_LEN, default_mbuf_mpool_data,
            "default_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&default_mbuf_pool, &default_mbuf_mpool,
            DEFAULT_MBUF_MPOOL_BUF_LEN, DEFAULT_MBUF_MPOOL_NBUFS);
    assert(rc == 0);

    rc = os_msys_register(&default_mbuf_pool);
    assert(rc == 0);

#ifdef OC_TRANSPORT_SERIAL
    console_echo(0);
    console_no_prompt();

    /* Init tasks */
    rc = shell_task_init(SHELL_TASK_PRIO, shell_stack, SHELL_TASK_STACK_SIZE,
                         SHELL_MAX_INPUT_LEN);
    assert(rc == 0);
#else
    console_init(NULL);
#endif

#ifdef OC_TRANSPORT_IP
    rc = native_sock_init();
    assert(rc == 0);
#endif

#ifdef OC_TRANSPORT_GATT
    /* Initialize the BLE LL */
    rc = ble_ll_init(BLE_LL_TASK_PRI, DEFAULT_MBUF_MPOOL_NBUFS,
                     DEFAULT_MBUF_MPOOL_BUF_LEN - BLE_HCI_DATA_HDR_SZ);
    assert(rc == 0);

    /* Initialize the RAM HCI transport. */
    hci_cfg = ble_hci_ram_cfg_dflt;
    rc = ble_hci_ram_init(&hci_cfg);
    assert(rc == 0);

    /* Initialize the BLE host. */
    cfg = ble_hs_cfg_dflt;
    cfg.max_hci_bufs = hci_cfg.num_evt_hi_bufs + hci_cfg.num_evt_lo_bufs;
    cfg.max_connections = 1;
    cfg.max_gattc_procs = 2;
    cfg.max_l2cap_chans = 3;
    cfg.max_l2cap_sig_procs = 1;
    cfg.sm_bonding = 1;
    cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    cfg.sync_cb = NULL; /* TODO */
    cfg.store_read_cb = ble_store_ram_read;
    cfg.store_write_cb = ble_store_ram_write;

    /* Populate config with the required GATT server settings. */
    cfg.max_attrs = 0;
    cfg.max_services = 0;
    cfg.max_client_configs = 0;

    rc = ble_svc_gap_init(&cfg);
    assert(rc == 0);

    rc = ble_svc_gatt_init(&cfg);
    assert(rc == 0);

    /* COAP Gatt server initialization */
    rc = ble_coap_gatt_srv_init(&cfg, &ev);
    assert(rc == 0);

#ifdef OC_CLIENT
    /* TODO INIT CLIENT */
#endif

    rc = ble_hs_init(ev, &cfg);
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("Mynewt_OCF");
    assert(rc == 0);
#endif

    ocf_task_init();

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);
}
