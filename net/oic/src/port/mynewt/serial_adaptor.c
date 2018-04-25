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

#include <assert.h>
#include <stdint.h>

#include "os/mynewt.h"

#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)

#include <shell/shell.h>

#include "oic/oc_log.h"
#include "oic/port/oc_connectivity.h"
#include "oic/port/mynewt/adaptor.h"
#include "oic/port/mynewt/transport.h"

static void oc_send_buffer_serial(struct os_mbuf *m);
static uint8_t oc_ep_serial_size(const struct oc_endpoint *oe);
static char *oc_log_ep_serial(char *ptr, int max, const struct oc_endpoint *);
static int oc_connectivity_init_serial(void);
void oc_connectivity_shutdown_serial(void);

static const struct oc_transport oc_serial_transport = {
    .ot_flags = 0,
    .ot_ep_size = oc_ep_serial_size,
    .ot_tx_ucast = oc_send_buffer_serial,
    .ot_tx_mcast = oc_send_buffer_serial,
    .ot_get_trans_security = NULL,
    .ot_ep_str = oc_log_ep_serial,
    .ot_init = oc_connectivity_init_serial,
    .ot_shutdown = oc_connectivity_shutdown_serial
};

uint8_t oc_serial_transport_id;
static struct os_mqueue oc_serial_mqueue;
static struct os_mbuf *oc_attempt_rx_serial(void);

static char *
oc_log_ep_serial(char *ptr, int max, const struct oc_endpoint *oe)
{
    return "serial";
}

static uint8_t
oc_ep_serial_size(const struct oc_endpoint *oe)
{
    return sizeof(struct oc_endpoint_plain);
}

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
oc_connectivity_init_serial(void)
{
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
oc_send_buffer_serial(struct os_mbuf *m)
{
    /* send over the shell output */
    if (shell_nlip_output(m)) {
        OC_LOG_ERROR("oc_transport_serial: nlip output failed\n");
    }
}

static struct os_mbuf *
oc_attempt_rx_serial(void)
{
    struct os_mbuf *m;
    struct os_mbuf *n;
    struct oc_endpoint_plain *oe_plain;

    /* get an mbuf from the queue */
    n = os_mqueue_get(&oc_serial_mqueue);
    if (NULL == n) {
        return NULL;
    }

    m = os_msys_get_pkthdr(0, sizeof(struct oc_endpoint_plain));
    if (!m) {
        OC_LOG_ERROR("Could not allocate OC message buffer\n");
        goto rx_attempt_err;
    }
    OS_MBUF_PKTHDR(m)->omp_len = OS_MBUF_PKTHDR(n)->omp_len;
    SLIST_NEXT(m, om_next) = n;

    oe_plain = (struct oc_endpoint_plain *)OC_MBUF_ENDPOINT(m);
    oe_plain->ep.oe_type = oc_serial_transport_id;
    oe_plain->ep.oe_flags = 0;

    return m;

rx_attempt_err:
    os_mbuf_free_chain(n);
    return NULL;
}

#endif

void
oc_register_serial(void)
{
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
    oc_serial_transport_id = oc_transport_register(&oc_serial_transport);
#endif
}
