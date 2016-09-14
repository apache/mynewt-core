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

#include "../oc_network_events_mutex.h"
#include "../oc_connectivity.h"

void oc_network_event_handler_mutex_init(void)
{
}

void oc_network_event_handler_mutex_lock(void)
{
}

void oc_network_event_handler_mutex_unlock(void)
{
}

void oc_send_buffer(oc_message_t *message)
{
}

#ifdef OC_SECURITY
uint16_t oc_connectivity_get_dtls_port(void)
{
}
#endif /* OC_SECURITY */

int oc_connectivity_init(void)
{
    return -1;
}

void oc_connectivity_shutdown(void)
{
}

void oc_send_multicast_message(oc_message_t *message)
{
}