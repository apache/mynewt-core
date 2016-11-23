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

#include <os/os_eventq.h>

#include "oc_network_events.h"
#include "oc_buffer.h"
#include "port/oc_connectivity.h"
#include "port/oc_signal_main_loop.h"
#include "port/oc_network_events_mutex.h"
#include "port/mynewt/adaptor.h"
#include "util/oc_list.h"

OC_LIST(network_events);
static void oc_network_ev_process(struct os_event *ev);
static struct os_event oc_network_ev = {
    .ev_cb = oc_network_ev_process
};

static void
oc_network_ev_process(struct os_event *ev)
{
    struct oc_message_s *head;

    oc_network_event_handler_mutex_lock();
    while (1) {
        head = oc_list_pop(network_events);
        if (!head) {
            break;
        }
        oc_recv_message(head);
    }
    oc_network_event_handler_mutex_unlock();
}

void
oc_network_event(oc_message_t *message)
{
    oc_network_event_handler_mutex_lock();
    oc_list_add(network_events, message);
    oc_network_event_handler_mutex_unlock();

    os_eventq_put(oc_evq_get(), &oc_network_ev);
}
