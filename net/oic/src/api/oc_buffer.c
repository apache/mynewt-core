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
#include <stdint.h>
#include <stdio.h>

#include <os/os_eventq.h>
#include <os/os_mempool.h>
#include <os/os_mbuf.h>

#include "messaging/coap/engine.h"
#include "port/oc_signal_main_loop.h"

#ifdef OC_SECURITY
#include "security/oc_dtls.h"
#endif

#include "config.h"
#include "oc_buffer.h"

#include "port/mynewt/adaptor.h"

static struct os_mempool oc_buffers;
static uint8_t oc_buffer_area[OS_MEMPOOL_BYTES(MAX_NUM_CONCURRENT_REQUESTS * 2,
      sizeof(oc_message_t))];

static void oc_buffer_handler(struct os_event *);

static struct os_mqueue oc_inq;
static struct oc_message *oc_buffer_outq;
static struct os_event oc_buffer_ev = {
    .ev_cb = oc_buffer_handler
};

oc_message_t *
oc_allocate_message(void)
{
    oc_message_t *message = (oc_message_t *)os_memblock_get(&oc_buffers);

    if (message) {
        message->length = 0;
        message->next = 0;
        message->ref_count = 1;
        LOG("buffer: Allocated TX/RX buffer; num free: %d\n",
          oc_buffers.mp_num_free);
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
            os_memblock_put(&oc_buffers, message);
            LOG("buffer: freed TX/RX buffer; num free: %d\n",
              oc_buffers.mp_num_free);
        }
    }
}

static void
oc_queue_msg(struct oc_message **head, struct oc_message *msg)
{
    struct oc_message *tmp;

    msg->next = NULL; /* oc_message has been oc_list once, clear next */
    if (!*head) {
        *head = msg;
    } else {
        for (tmp = *head; tmp->next; tmp = tmp->next);
        tmp->next = msg;
    }
}

void
oc_recv_message(struct os_mbuf *m)
{
    int rc;

    rc = os_mqueue_put(&oc_inq, oc_evq_get(), m);
    assert(rc == 0);
}

void
oc_send_message(oc_message_t *message)
{
    oc_queue_msg(&oc_buffer_outq, message);
    os_eventq_put(oc_evq_get(), &oc_buffer_ev);
}

static void
oc_buffer_tx(struct oc_message *msg)
{
    struct os_mbuf *m;
    struct oc_endpoint *oe;
    int rc;

    /* get a packet header */
    m = os_msys_get_pkthdr(0, sizeof(struct oc_endpoint));
    if (!m) {
        ERROR("oc_buffer_tx: failed to alloc mbuf\n");
        oc_message_unref(msg);
        return;
    }

    /* add this data to the mbuf */
    rc = os_mbuf_append(m, msg->data, msg->length);
    if (rc != 0) {
        ERROR("oc_buffer_tx: could not append data\n");
        oc_message_unref(msg);
        return;
    }

    oe = OC_MBUF_ENDPOINT(m);
    memcpy(oe, &msg->endpoint, sizeof(msg->endpoint));

    oc_message_unref(msg);

#ifdef OC_CLIENT
    if (oe->flags & MULTICAST) {
        LOG("Outbound network event: multicast request\n");
        oc_send_multicast_message(m);
    } else {
#endif
#ifdef OC_SECURITY
        /* XXX convert this */
        if (oe->flags & SECURED) {
            LOG("Outbound network event: forwarding to DTLS\n");

            if (!oc_sec_dtls_connected(oe)) {
                LOG("Posting INIT_DTLS_CONN_EVENT\n");
                oc_process_post(&oc_dtls_handler,
                  oc_events[INIT_DTLS_CONN_EVENT], m);
            } else {
                LOG("Posting RI_TO_DTLS_EVENT\n");
                oc_process_post(&oc_dtls_handler,
                  oc_events[RI_TO_DTLS_EVENT], m);
            }
        } else
#endif
        {
            LOG("Outbound network event: unicast message\n");
            oc_send_buffer(m);
        }
#ifdef OC_CLIENT
    }
#endif
}

static void
oc_buffer_rx(struct os_event *ev)
{
    struct oc_message *msg;
    struct os_mbuf *m;
#if defined(OC_SECURITY)
    uint8_t b;
#endif

    while ((m = os_mqueue_get(&oc_inq)) != NULL) {
        msg = oc_allocate_message();
        if (!msg) {
            ERROR("Could not allocate OC message buffer\n");
            goto free_msg;
        }
        if (OS_MBUF_PKTHDR(m)->omp_len > MAX_PAYLOAD_SIZE) {
            ERROR("Message to large for OC message buffer\n");
            goto free_msg;
        }
        if (os_mbuf_copydata(m, 0, OS_MBUF_PKTHDR(m)->omp_len, msg->data)) {
            ERROR("Failed to copy message from mbuf to OC message buffer \n");
            goto free_msg;
        }
        memcpy(&msg->endpoint, OC_MBUF_ENDPOINT(m), sizeof(msg->endpoint));
        msg->length = OS_MBUF_PKTHDR(m)->omp_len;

#ifdef OC_SECURITY
        b = m->om_data[0];
        if (b > 19 && b < 64) {
            LOG("Inbound network event: encrypted request\n");
            oc_process_post(&oc_dtls_handler, oc_events[UDP_TO_DTLS_EVENT], m);
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
free_msg:
        os_mbuf_free_chain(m);
        if (msg) {
            oc_message_unref(msg);
        }
    }
}

static void
oc_buffer_handler(struct os_event *ev)
{
    struct oc_message *msg;

    while (oc_buffer_outq) {
        msg = oc_buffer_outq;
        if (msg) {
            oc_buffer_outq = msg->next;
            msg->next = NULL;
            oc_buffer_tx(msg);
        }
    }
}

void
oc_buffer_init(void)
{
    os_mempool_init(&oc_buffers, MAX_NUM_CONCURRENT_REQUESTS * 2,
      sizeof(oc_message_t), oc_buffer_area, "oc_bufs");
    os_mqueue_init(&oc_inq, oc_buffer_rx, NULL);
}

