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
#include <syscfg/syscfg.h>
#include <os/os.h>
#include <os/endian.h>
#include <string.h>
#include <log/log.h>
#include "oic/oc_log.h"
#include "port/oc_network_events_mutex.h"
#include "port/oc_connectivity.h"
#include "oc_buffer.h"
#include "adaptor.h"

static struct os_eventq *oc_evq;

struct log oc_log;

/* not sure if these semaphores are necessary yet.  If we are running
 * all of this from one task, we may not need these */
static struct os_mutex oc_net_mutex;

struct os_eventq *
oc_evq_get(void)
{
    os_eventq_ensure(&oc_evq, NULL);
    return oc_evq;
}

void
oc_evq_set(struct os_eventq *evq)
{
    os_eventq_designate(&oc_evq, evq, NULL);
}

void
oc_network_event_handler_mutex_init(void)
{
    os_error_t rc;
    rc = os_mutex_init(&oc_net_mutex);
    assert(rc == 0);
}

void
oc_network_event_handler_mutex_lock(void)
{
    os_mutex_pend(&oc_net_mutex, OS_TIMEOUT_NEVER);
}

void
oc_network_event_handler_mutex_unlock(void)
{
    os_mutex_release(&oc_net_mutex);
}

void
oc_send_buffer(struct os_mbuf *m)
{
    struct oc_endpoint *oe;

    oe = OC_MBUF_ENDPOINT(m);

    switch (oe->flags) {
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1)
    case IP:
        oc_send_buffer_ip(m);
        break;
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
    case GATT:
        oc_send_buffer_gatt(m);
        break;
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
    case SERIAL:
        oc_send_buffer_serial(m);
        break;
#endif
    default:
        ERROR("Unknown transport option %u\n", oe->flags);
        os_mbuf_free_chain(m);
    }
}

void
oc_send_multicast_message(struct os_mbuf *m)
{
    /*
     * Send on all the transports.
     */
    void (*funcs[])(struct os_mbuf *) = {
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1)
        oc_send_buffer_ip_mcast,
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
        /* no multicast for GATT, just send unicast */
        oc_send_buffer_gatt,
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
        /* no multi-cast for serial.  just send unicast */
        oc_send_buffer_serial,
#endif
    };
    struct os_mbuf *n;
    int i;

    for (i = 0; i < (sizeof(funcs) / sizeof(funcs[0])) - 1; i++) {
        n = os_mbuf_dup(m);
        funcs[i](m);
        if (!n) {
            return;
        }
        m = n;
    }
    funcs[(sizeof(funcs) / sizeof(funcs[0])) - 1](m);
}

void
oc_connectivity_shutdown(void)
{
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1)
    oc_connectivity_shutdown_ip();
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
    oc_connectivity_shutdown_serial();
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
    oc_connectivity_shutdown_gatt();
#endif
}

int
oc_connectivity_init(void)
{
    int rc = -1;

#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1)
    if (oc_connectivity_init_ip() == 0) {
        rc = 0;
    }
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
    if (oc_connectivity_init_serial() == 0) {
        rc = 0;
    }
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
    if (oc_connectivity_init_gatt() == 0) {
        rc = 0;
    }
#endif

    if (rc != 0) {
        oc_connectivity_shutdown();
        return rc;
    }

    return 0;
}
