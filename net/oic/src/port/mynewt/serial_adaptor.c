/*
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

#include <syscfg/syscfg.h>
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)

#include <assert.h>

#include <os/os.h>

#include <shell/shell.h>

#include "oc_buffer.h"
#include "port/oc_connectivity.h"
#include "../oc_log.h"
#include "adaptor.h"

struct os_mqueue oc_serial_mqueue;

static struct os_mbuf *oc_attempt_rx_serial(void);

static int
oc_serial_in(struct os_mbuf *m, void *arg)
{
    return os_mqueue_put(&oc_serial_mqueue, oc_evq_get(), m);
}

void
oc_connectivity_shutdown_serial(void)
{
    shell_nlip_input_register(NULL, NULL);
}

static void
oc_event_serial(struct os_event *ev)
{
    struct os_mbuf *m;

    while ((m = oc_attempt_rx_serial()) != NULL) {
        oc_recv_message(m);
    }
}

int
oc_connectivity_init_serial(void) {
    int rc;

    rc = shell_nlip_input_register(oc_serial_in, NULL);
    if (rc != 0) {
        goto err;
    }

    rc = os_mqueue_init(&oc_serial_mqueue, oc_event_serial, NULL);
    if (rc != 0) {
        goto err;
    }

    return 0;

err:
    oc_connectivity_shutdown_serial();
    return rc;
}


void
oc_send_buffer_serial(oc_message_t *message)
{
    int rc;
    struct os_mbuf *m;

    /* get a packet header */
    m = os_msys_get_pkthdr(0, 0);
    if (m == NULL) {
        ERROR("oc_transport_serial: No mbuf available\n");
        goto err;
    }

    /* add this data to the mbuf */
    rc = os_mbuf_append(m, message->data, message->length);
    if (rc != 0) {

        ERROR("oc_transport_serial: could not append data \n");
        goto err;
    }

    /* send over the shell output */
    rc = shell_nlip_output(m);
    if (rc != 0) {
        ERROR("oc_transport_serial: nlip output failed \n");
        goto err;
    }

    LOG("oc_transport_serial: send buffer %zu\n", message->length);

err:
    oc_message_unref(message);
    return;

}

static struct os_mbuf *
oc_attempt_rx_serial(void)
{
    struct os_mbuf *m;
    struct os_mbuf *n;
    struct oc_endpoint *oe;

    LOG("oc_transport_serial attempt rx\n");

    /* get an mbuf from the queue */
    n = os_mqueue_get(&oc_serial_mqueue);
    if (NULL == n) {
        ERROR("oc_transport_serial: Woke for for receive but found no mbufs\n");
        return NULL;
    }

    m = os_msys_get_pkthdr(0, sizeof(struct oc_endpoint));
    if (!m) {
        ERROR("Could not allocate OC message buffer\n");
        goto rx_attempt_err;
    }
    OS_MBUF_PKTHDR(m)->omp_len = OS_MBUF_PKTHDR(n)->omp_len;
    SLIST_NEXT(m, om_next) = n;

    oe = OC_MBUF_ENDPOINT(m);
    oe->flags = SERIAL;

    LOG("oc_transport_serial rx %p-%u\n", n, OS_MBUF_PKTHDR(n)->omp_len);

    return m;

rx_attempt_err:
    os_mbuf_free_chain(n);
    return NULL;
}
#endif
