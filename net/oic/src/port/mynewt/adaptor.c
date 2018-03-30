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
#include <string.h>
#include "os/mynewt.h"
#include <log/log.h>
#include "oic/oc_log.h"
#include "oic/oc_ri.h"
#include "api/oc_priv.h"
#include "oic/port/oc_connectivity.h"
#include "oic/port/mynewt/adaptor.h"
#include "oic/port/mynewt/transport.h"

static struct os_eventq *oc_evq;
struct log oc_log;
const struct oc_transport *oc_transports[OC_TRANSPORT_MAX];

struct os_eventq *
oc_evq_get(void)
{
    return oc_evq;
}

void
oc_evq_set(struct os_eventq *evq)
{
    oc_evq = evq;
}

int8_t
oc_transport_register(const struct oc_transport *ot)
{
    int i;
    int first = -1;

    for (i = 0; i < OC_TRANSPORT_MAX; i++) {
        if (oc_transports[i] == ot) {
            return -1;
        }
        if (oc_transports[i] == NULL && first < 0) {
            oc_transports[i] = ot;
            first = i;
        }
    }
    return first;
}

int8_t
oc_transport_lookup(const struct oc_transport *ot)
{
    int i;

    for (i = 0; i < OC_TRANSPORT_MAX; i++) {
        if (oc_transports[i] == ot) {
            return i;
        }
    }
    return -1;
}

void
oc_transport_unregister(const struct oc_transport *ot)
{
    int i;

    i = oc_transport_lookup(ot);
    if (i >= 0) {
        oc_transports[i] = NULL;
    }
}

void
oc_send_buffer(struct os_mbuf *m)
{
    struct oc_endpoint *oe;
    const struct oc_transport *ot;

    oe = OC_MBUF_ENDPOINT(m);

    ot = oc_transports[oe->ep.oe_type];
    if (ot) {
        ot->ot_tx_ucast(m);
    } else {
        OC_LOG_ERROR("Unknown transport option %u\n", oe->ep.oe_type);
        os_mbuf_free_chain(m);
    }
}

/*
 * Send on all the transports.
 */
void
oc_send_multicast_message(struct os_mbuf *m)
{
    const struct oc_transport *ot;
    const struct oc_transport *prev = NULL;
    struct os_mbuf *n;
    int i;

    for (i = 0; i < OC_TRANSPORT_MAX; i++) {
        if (!oc_transports[i]) {
            continue;
        }

        ot = oc_transports[i];
        if (prev) {
            n = os_mbuf_dup(m);
            prev->ot_tx_mcast(m);
            if (!n) {
                return;
            }
            m = n;
        }
        prev = ot;
    }
    if (prev) {
        prev->ot_tx_mcast(m);
    }
}

/**
 * Retrieves the specified endpoint's transport layer security properties.
 */
oc_resource_properties_t
oc_get_trans_security(const struct oc_endpoint *oe)
{
    const struct oc_transport *ot;

    ot = oc_transports[oe->ep.oe_type];
    if (ot) {
        if (ot->ot_get_trans_security) {
            return ot->ot_get_trans_security(oe);
        } else {
            return 0;
        }
    }
    OC_LOG_ERROR("Unknown transport option %u\n", oe->ep.oe_type);
    return 0;
}

void
oc_connectivity_shutdown(void)
{
    int i;
    const struct oc_transport *ot;

    for (i = 0; i < OC_TRANSPORT_MAX; i++) {
        if (!oc_transports[i]) {
            continue;
        }
        ot = oc_transports[i];
        ot->ot_shutdown();
    }
}

int
oc_connectivity_init(void)
{
    int rc = -1;
    int i;
    const struct oc_transport *ot;

    for (i = 0; i < OC_TRANSPORT_MAX; i++) {
        if (!oc_transports[i]) {
            continue;
        }
        ot = oc_transports[i];
        if (ot->ot_init() == 0) {
            rc = 0;
        }
    }
    return rc;
}

void
oc_init(void)
{
    SYSINIT_ASSERT_ACTIVE();
    oc_ri_mem_init();
    oc_evq_set(os_eventq_dflt_get());
}
