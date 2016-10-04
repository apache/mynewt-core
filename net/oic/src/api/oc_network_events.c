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

#include "oc_network_events.h"
#include "oc_buffer.h"
#include "port/oc_connectivity.h"
#include "port/oc_signal_main_loop.h"
#include "util/oc_list.h"

OC_LIST(network_events);

static void
oc_process_network_event(void)
{
  oc_network_event_handler_mutex_lock();
  oc_message_t *head = (oc_message_t *)oc_list_pop(network_events);
  while (head != NULL) {
    oc_recv_message(head);
    head = oc_list_pop(network_events);
  }
  oc_network_event_handler_mutex_unlock();
}

OC_PROCESS(oc_network_events, "");
OC_PROCESS_THREAD(oc_network_events, ev, data)
{
  OC_PROCESS_POLLHANDLER(oc_process_network_event());
  OC_PROCESS_BEGIN();
  while (oc_process_is_running(&(oc_network_events))) {
    OC_PROCESS_YIELD();
  }
  OC_PROCESS_END();
}

void
oc_network_event(oc_message_t *message)
{
  oc_network_event_handler_mutex_lock();
  oc_list_add(network_events, message);
  oc_network_event_handler_mutex_unlock();

  oc_process_poll(&(oc_network_events));
  oc_signal_main_loop();
}
