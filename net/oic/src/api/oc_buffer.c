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

#include "port/mynewt/adaptor.h"

OC_MEMB(oc_buffers_s, oc_message_t, (MAX_NUM_CONCURRENT_REQUESTS * 2));

static void oc_buffer_handler(struct os_event *);

static struct oc_message_s *oc_buffer_inq;
static struct oc_message_s *oc_buffer_outq;
static struct os_event oc_buffer_ev = {
    .ev_cb = oc_buffer_handler
};

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
    } else {
        LOG("buffer: No free TX/RX buffers!\n");
    }
    return message;
}

void
oc_message_add_ref(oc_message_t *message)
{
    if (message) {
        message->ref_count++;
    }
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

static void
oc_queue_msg(struct oc_message_s **head, struct oc_message_s *msg)
{
    struct oc_message_s *tmp;

    msg->next = NULL; /* oc_message_s has been oc_list once, clear next */
    if (!*head) {
        *head = msg;
    } else {
        for (tmp = *head; tmp->next; tmp = tmp->next);
        tmp->next = msg;
    }
}

void
oc_recv_message(oc_message_t *message)
{
    oc_queue_msg(&oc_buffer_inq, message);
    os_eventq_put(oc_evq_get(), &oc_buffer_ev);
}

void
oc_send_message(oc_message_t *message)
{
    oc_queue_msg(&oc_buffer_outq, message);
    os_eventq_put(oc_evq_get(), &oc_buffer_ev);
}

static void
oc_buffer_tx(struct oc_message_s *message)
{
#ifdef OC_CLIENT
    if (message->endpoint.flags & MULTICAST) {
        LOG("Outbound network event: multicast request\n");
        oc_send_multicast_message(message);
        oc_message_unref(message);
    } else {
#endif
#ifdef OC_SECURITY
        if (message->endpoint.flags & SECURED) {
            LOG("Outbound network event: forwarding to DTLS\n");

            if (!oc_sec_dtls_connected(&message->endpoint)) {
                LOG("Posting INIT_DTLS_CONN_EVENT\n");
                oc_process_post(&oc_dtls_handler,
                  oc_events[INIT_DTLS_CONN_EVENT], msg);
            } else {
                LOG("Posting RI_TO_DTLS_EVENT\n");
                oc_process_post(&oc_dtls_handler,
                  oc_events[RI_TO_DTLS_EVENT], msg);
            }
        } else
#endif
        {
            LOG("Outbound network event: unicast message\n");
            oc_send_buffer(message);
            oc_message_unref(message);
        }
#ifdef OC_CLIENT
    }
#endif
}

static void
oc_buffer_rx(struct oc_message_s *msg)
{
#ifdef OC_SECURITY
    uint8_t b = (uint8_t)(msg->data[0];
    if (b > 19 && b < 64) {
        LOG("Inbound network event: encrypted request\n");
        oc_process_post(&oc_dtls_handler, oc_events[UDP_TO_DTLS_EVENT], msg);
    } else {
        LOG("Inbound network event: decrypted request\n");
        coap_receive(msg);
        oc_message_unref(msg);
    }
#else
    LOG("Inbound network event: decrypted request\n");
    coap_receive(msg);
    oc_message_unref(msg);
#endif
}

static void
oc_buffer_handler(struct os_event *ev)
{
    struct oc_message_s *msg;

    while (oc_buffer_outq || oc_buffer_inq) {
        msg = oc_buffer_outq;
        if (msg) {
            oc_buffer_outq = msg->next;
            msg->next = NULL;
            oc_buffer_tx(msg);
        }
        msg = oc_buffer_inq;
        if (msg) {
            oc_buffer_inq = msg->next;
            msg->next = NULL;
            oc_buffer_rx(msg);
        }
    }
}
