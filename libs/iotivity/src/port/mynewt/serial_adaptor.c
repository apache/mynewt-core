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
#include <shell/shell.h>
#include "oc_buffer.h"
#include "../oc_log.h"
#include "adaptor.h"

#ifdef OC_TRANSPORT_SERIAL

struct os_mqueue oc_serial_mqueue;

static int
oc_serial_in(struct os_mbuf *m, void *arg)
{
    return os_mqueue_put(&oc_serial_mqueue, &oc_event_q, m);
}

void
oc_connectivity_shutdown_serial(void) {
    shell_nlip_input_register(NULL, NULL);
}

int
oc_connectivity_init_serial(void) {
    int rc;

    rc = shell_nlip_input_register(oc_serial_in, NULL);
    if (rc != 0) {
        goto err;
    }

    rc = os_mqueue_init(&oc_serial_mqueue, NULL);
    if (rc != 0) {
        goto err;
    }
    /* override the eventq type */
    oc_serial_mqueue.mq_ev.ev_type = OC_ADATOR_EVENT_SERIAL;

err:
    oc_connectivity_shutdown_serial();
    return rc;
}


void oc_send_buffer_serial(oc_message_t *message) {
    int rc;
    struct os_mbuf *m;

    /* get a packet header */
    m = os_msys_get_pkthdr(0, 0);
    if (m == NULL) {
        goto err;
    }

    /* add this data to the mbuf */
    rc = os_mbuf_append(m, message->data, message->length);
    if (rc != 0) {
        goto err;
    }

    /* send over the shell output */
    rc = shell_nlip_output(m);
    if (rc != 0) {
        goto err;
    }

    return;

    err:
    ERROR("Unable to send message via oc_serial %d\n", rc);
    oc_message_unref(message);
    return;

}

oc_message_t *
oc_attempt_rx_serial(void) {
    int rc;
    struct os_mbuf *m = NULL;
    struct os_mbuf_pkthdr *pkt;
    oc_message_t *message = NULL;

    LOG("oc_transport_serial attempt rx\n");

    /* get an mbuf from the queue */
    m = os_mqueue_get(&oc_serial_mqueue);

    if (!OS_MBUF_IS_PKTHDR(m)) {
        goto rx_attempt_err;
    }

    pkt = OS_MBUF_PKTHDR(m);

    LOG("oc_transport_serial rx %p-%u\n", pkt, pkt->omp_len);

    message = oc_allocate_message();
    if (NULL == message) {
        ERROR("Could not allocate OC message buffer\n");
        goto rx_attempt_err;
    }

    if (pkt->omp_len > MAX_PAYLOAD_SIZE) {
        ERROR("Message to large for OC message buffer\n");
        goto rx_attempt_err;
    }
    /* copy to message from mbuf chain */
    rc = os_mbuf_copydata(m, 0, pkt->omp_len, message->data);
    if (rc != 0) {
        ERROR("Failed to copy message from mbuf to OC message buffer \n");
        goto rx_attempt_err;
    }

    os_mbuf_free_chain(m);

    message->endpoint.flags = SERIAL;
    message->length = pkt->omp_len;

    LOG("Successfully rx length %lu\n", message->length);
    return message;

    /* add the addr info to the message */
rx_attempt_err:
    if (m) {
        os_mbuf_free_chain(m);
    }

    if (message) {
        oc_message_unref(message);
    }

    return NULL;
}

#endif 