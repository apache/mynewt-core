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
#include <sysinit/sysinit.h>
#include <os/os.h>
#include <os/endian.h>
#include <string.h>
#include <log/log.h>
#include "oic/oc_log.h"
#include "oic/oc_ri.h"
#include "api/oc_priv.h"
#include "port/oc_connectivity.h"
#include "adaptor.h"

static struct os_eventq *oc_evq;

struct log oc_log;

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

void
oc_send_buffer(struct os_mbuf *m)
{
    struct oc_endpoint *oe;

    oe = OC_MBUF_ENDPOINT(m);

    switch (oe->oe.flags) {
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV6) == 1)
    case IP:
        oc_send_buffer_ip6(m);
        break;
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV4) == 1)
    case IP4:
        oc_send_buffer_ip4(m);
        break;
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
    case GATT:
        oc_send_buffer_gatt(m);
        break;
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_LORA) == 1)
    case LORA:
        oc_send_buffer_lora(m);
        break;
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
    case SERIAL:
        oc_send_buffer_serial(m);
        break;
#endif
    default:
        OC_LOG_ERROR("Unknown transport option %u\n", oe->oe.flags);
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
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV6) == 1)
        oc_send_buffer_ip6_mcast,
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV4) == 1)
        oc_send_buffer_ip4_mcast,
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
        /* no multicast for GATT, just send unicast */
        oc_send_buffer_gatt,
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_LORA) == 1)
        /* no multi-cast for serial.  just send unicast */
        oc_send_buffer_lora,
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

/**
 * Retrieves the specified endpoint's transport layer security properties.
 */
oc_resource_properties_t
oc_get_trans_security(const struct oc_endpoint *oe)
{
    switch (oe->oe.flags) {
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
    case GATT:
        return oc_get_trans_security_gatt(&oe->oe_ble);
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV6) == 1)
    case IP:
        return 0;
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV4) == 1)
    case IP4:
        return 0;
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_LORA) == 1)
    case LORA:
        return 0;
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
    case SERIAL:
        return 0;
#endif
    default:
        OC_LOG_ERROR("Unknown transport option %u\n", oe->oe.flags);
        return 0;
    }
}

void
oc_connectivity_shutdown(void)
{
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV6) == 1)
    oc_connectivity_shutdown_ip6();
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV4) == 1)
    oc_connectivity_shutdown_ip4();
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
    oc_connectivity_shutdown_serial();
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_LORA) == 1)
    oc_connectivity_shutdown_lora();
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
    oc_connectivity_shutdown_gatt();
#endif
}

int
oc_connectivity_init(void)
{
    int rc = -1;

#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV6) == 1)
    if (oc_connectivity_init_ip6() == 0) {
        rc = 0;
    }
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV4) == 1)
    if (oc_connectivity_init_ip4() == 0) {
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
#if (MYNEWT_VAL(OC_TRANSPORT_LORA) == 1)
    if (oc_connectivity_init_lora() == 0) {
        rc = 0;
    }
#endif

    if (rc != 0) {
        oc_connectivity_shutdown();
        return rc;
    }

    return 0;
}

void
oc_init(void)
{
    SYSINIT_ASSERT_ACTIVE();
    oc_ri_mem_init();
    oc_evq_set(os_eventq_dflt_get());
}
