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

#ifndef OC_NETWORK_EVENTS_H
#define OC_NETWORK_EVENTS_H

#include "port/oc_network_events_mutex.h"
#include "util/oc_process.h"

OC_PROCESS_NAME(oc_network_events);

typedef struct oc_message_s oc_message_t;

void oc_network_event(oc_message_t *message);

#endif /* OC_NETWORK_EVENTS_H */
