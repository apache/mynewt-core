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

#include "messaging/coap/engine.h"
#include "port/oc_signal_main_loop.h"
#include "util/oc_memb.h"
#include <stdint.h>
#include <stdio.h>

#ifdef OC_SECURITY
#include "security/oc_dtls.h"
#endif

#include "config.h"
#include "oc_buffer.h"
#include "oc_events.h"

OC_PROCESS(message_buffer_handler, "OC Message Buffer Handler");
OC_MEMB(oc_buffers_s, oc_message_t, (MAX_NUM_CONCURRENT_REQUESTS * 2));

oc_message_t *
oc_allocate_message(void)
{
  oc_message_t *message = (oc_message_t *)oc_memb_alloc(&oc_buffers_s);
  if (message) {
    message->length = 0;
    message->next = 0;
    message->ref_count = 1;
    LOG("buffer: Allocated TX/RX buffer; num free: %d\n",
        oc_memb_numfree(&oc_buffers_s));
  } else
    LOG("buffer: No free TX/RX buffers!\n");
  return message;
}

void
oc_message_add_ref(oc_message_t *message)
{
  if (message)
    message->ref_count++;
}

void
oc_message_unref(oc_message_t *message)
{
  if (message) {
    message->ref_count--;
    if (message->ref_count == 0) {
      oc_memb_free(&oc_buffers_s, message);
      LOG("buffer: freed TX/RX buffer; num free: %d\n",
          oc_memb_numfree(&oc_buffers_s));
    }
  }
}

void
oc_recv_message(oc_message_t *message)
{
  oc_process_post(&message_buffer_handler, oc_events[INBOUND_NETWORK_EVENT],
                  message);
}

void
oc_send_message(oc_message_t *message)
{
  oc_process_post(&message_buffer_handler, oc_events[OUTBOUND_NETWORK_EVENT],
                  message);

  oc_signal_main_loop();
}

OC_PROCESS_THREAD(message_buffer_handler, ev, data)
{
  OC_PROCESS_BEGIN();
  LOG("Started buffer handler process\n");
  while (1) {
    OC_PROCESS_YIELD();

    if (ev == oc_events[INBOUND_NETWORK_EVENT]) {
#ifdef OC_SECURITY
      uint8_t b = (uint8_t)((oc_message_t *)data)->data[0];
      if (b > 19 && b < 64) {
        LOG("Inbound network event: encrypted request\n");
        oc_process_post(&oc_dtls_handler, oc_events[UDP_TO_DTLS_EVENT], data);
      } else {
        LOG("Inbound network event: decrypted request\n");
        oc_process_post(&coap_engine, oc_events[INBOUND_RI_EVENT], data);
      }
#else
      LOG("Inbound network event: decrypted request\n");
      oc_process_post(&coap_engine, oc_events[INBOUND_RI_EVENT], data);
#endif
    } else if (ev == oc_events[OUTBOUND_NETWORK_EVENT]) {
      oc_message_t *message = (oc_message_t *)data;

#ifdef OC_CLIENT
      if (message->endpoint.flags & MULTICAST) {
        LOG("Outbound network event: multicast request\n");
        oc_send_multicast_message(message);
        oc_message_unref(message);
      } else
#endif
#ifdef OC_SECURITY
        if (message->endpoint.flags & SECURED) {
        LOG("Outbound network event: forwarding to DTLS\n");

        if (!oc_sec_dtls_connected(&message->endpoint)) {
          LOG("Posting INIT_DTLS_CONN_EVENT\n");
          oc_process_post(&oc_dtls_handler, oc_events[INIT_DTLS_CONN_EVENT],
                          data);
        } else {
          LOG("Posting RI_TO_DTLS_EVENT\n");
          oc_process_post(&oc_dtls_handler, oc_events[RI_TO_DTLS_EVENT], data);
        }
      } else
#endif
      {
        LOG("Outbound network event: unicast message\n");
        oc_send_buffer(message);
        oc_message_unref(message);
      }
    }
  }
  OC_PROCESS_END();
}
