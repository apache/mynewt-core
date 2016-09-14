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

#ifndef OC_EVENTS_H
#define OC_EVENTS_H

#include "util/oc_process.h"

typedef enum {
  INBOUND_NETWORK_EVENT,
  UDP_TO_DTLS_EVENT,
  INIT_DTLS_CONN_EVENT,
  RI_TO_DTLS_EVENT,
  INBOUND_RI_EVENT,
  OUTBOUND_NETWORK_EVENT,
  __NUM_OC_EVENT_TYPES__
} oc_events_t;

oc_process_event_t oc_events[__NUM_OC_EVENT_TYPES__];

#endif /* OC_EVENTS_H */
