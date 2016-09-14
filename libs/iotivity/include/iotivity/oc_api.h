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

#ifndef OC_API_H
#define OC_API_H

#include "messaging/coap/oc_coap.h"
#include "oc_ri.h"
#include "port/oc_signal_main_loop.h"
#include "port/oc_storage.h"

typedef struct
{
  void (*init)(void);

#ifdef OC_SECURITY
  void (*get_credentials)(void);
#endif /* OC_SECURITY */

#ifdef OC_SERVER
  void (*register_resources)(void);
#endif /* OC_SERVER */

#ifdef OC_CLIENT
  void (*requests_entry)(void);
#endif /* OC_CLIENT */
} oc_handler_t;

typedef void (*oc_init_platform_cb_t)(void *data);
typedef void (*oc_add_device_cb_t)(void *data);

int oc_main_init(oc_handler_t *handler);

oc_clock_time_t oc_main_poll(void);

void oc_main_shutdown(void);

void oc_add_device(const char *uri, const char *rt, const char *name,
                   const char *spec_version, const char *data_model_version,
                   oc_add_device_cb_t add_device_cb, void *data);

#define oc_set_custom_device_property(prop, value)                             \
  oc_rep_set_text_string(root, prop, value)

void oc_init_platform(const char *mfg_name,
                      oc_init_platform_cb_t init_platform_cb, void *data);

#define oc_set_custom_platform_property(prop, value)                           \
  oc_rep_set_text_string(root, prop, value)

/** Server side */
oc_resource_t *oc_new_resource(const char *uri, uint8_t num_resource_types,
                               int device);
void oc_resource_bind_resource_interface(oc_resource_t *resource,
                                         uint8_t interface);
void oc_resource_set_default_interface(oc_resource_t *resource,
                                       oc_interface_mask_t interface);
void oc_resource_bind_resource_type(oc_resource_t *resource, const char *type);

void oc_process_baseline_interface(oc_resource_t *resource);

#ifdef OC_SECURITY
void oc_resource_make_secure(oc_resource_t *resource);
#endif /* OC_SECURITY */

void oc_resource_set_discoverable(oc_resource_t *resource);
void oc_resource_set_observable(oc_resource_t *resource);
void oc_resource_set_periodic_observable(oc_resource_t *resource,
                                         uint16_t seconds);
void oc_resource_set_request_handler(oc_resource_t *resource,
                                     oc_method_t method,
                                     oc_request_handler_t handler);
bool oc_add_resource(oc_resource_t *resource);
void oc_delete_resource(oc_resource_t *resource);
void oc_deactivate_resource(oc_resource_t *resource);

void oc_init_query_iterator(oc_request_t *request);
int oc_interate_query(oc_request_t *request, char **key, int *key_len,
                      char **value, int *value_len);
int oc_get_query_value(oc_request_t *request, const char *key, char **value);

void oc_send_response(oc_request_t *request, oc_status_t response_code);
void oc_ignore_request(oc_request_t *request);

void oc_indicate_separate_response(oc_request_t *request,
                                   oc_separate_response_t *response);
void oc_set_separate_response_buffer(oc_separate_response_t *handle);
void oc_send_separate_response(oc_separate_response_t *handle,
                               oc_status_t response_code);

int oc_notify_observers(oc_resource_t *resource);

/** Client side */
#include "oc_client_state.h"

bool oc_do_ip_discovery(const char *rt, oc_discovery_cb_t handler);

bool oc_do_get(const char *uri, oc_server_handle_t *server, const char *query,
               oc_response_handler_t handler, oc_qos_t qos);

bool oc_do_delete(const char *uri, oc_server_handle_t *server,
                  oc_response_handler_t handler, oc_qos_t qos);

bool oc_init_put(const char *uri, oc_server_handle_t *server, const char *query,
                 oc_response_handler_t handler, oc_qos_t qos);

bool oc_do_put(void);

bool oc_init_post(const char *uri, oc_server_handle_t *server,
                  const char *query, oc_response_handler_t handler,
                  oc_qos_t qos);

bool oc_do_post(void);

bool oc_do_observe(const char *uri, oc_server_handle_t *server,
                   const char *query, oc_response_handler_t handler,
                   oc_qos_t qos);

bool oc_stop_observe(const char *uri, oc_server_handle_t *server);

/** Common operations */

void oc_set_delayed_callback(void *cb_data, oc_trigger_t callback,
                             uint16_t seconds);
void oc_remove_delayed_callback(void *cb_data, oc_trigger_t callback);

/** API for setting handlers for interrupts */

#define oc_signal_interrupt_handler(name)                                      \
  do {                                                                         \
    oc_process_poll(&(name##_interrupt_x));                                    \
    oc_signal_main_loop();                                                     \
  } while (0)

#define oc_activate_interrupt_handler(name)                                    \
  (oc_process_start(&(name##_interrupt_x), 0))

#define oc_define_interrupt_handler(name)                                      \
  void name##_interrupt_x_handler(void);                                       \
  OC_PROCESS(name##_interrupt_x, "");                                          \
  OC_PROCESS_THREAD(name##_interrupt_x, ev, data)                              \
  {                                                                            \
    OC_PROCESS_POLLHANDLER(name##_interrupt_x_handler());                      \
    OC_PROCESS_BEGIN();                                                        \
    while (oc_process_is_running(&(name##_interrupt_x))) {                     \
      OC_PROCESS_YIELD();                                                      \
    }                                                                          \
    OC_PROCESS_END();                                                          \
  }                                                                            \
  void name##_interrupt_x_handler(void)

#endif /* OC_API_H */
