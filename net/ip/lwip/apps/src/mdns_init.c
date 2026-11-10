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

#include <os/mynewt.h>

#if MYNEWT_VAL(LWIP_APPS_MDNS)

#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <lwip/apps/mdns.h>

static bool
lwip_mdns_has_hostname(const struct netif *nif)
{
    return nif->hostname != NULL && nif->hostname[0] != '\0';
}

/*
 * Start mDNS responder on interface.  At most LWIP_APPS_MDNS_NUM_NETIF
 * interfaces, each probing interface needs lwIP timeout counted in
 * MEMP_NUM_SYS_TIMEOUT.  More would take timeouts of other users (e.g.
 * TCP timer) and make it random which interface gets its probe timeout.
 */
static void
lwip_mdns_add(struct netif *nif, int *count)
{
    err_t err;

    if (*count >= MYNEWT_VAL(LWIP_APPS_MDNS_NUM_NETIF)) {
        LWIP_APPS_MDNS_LOG_WARN("Not started on %c%c%u, "
                                "LWIP_APPS_MDNS_NUM_NETIF is %d\n",
                                nif->name[0], nif->name[1], nif->num,
                                MYNEWT_VAL(LWIP_APPS_MDNS_NUM_NETIF));
        return;
    }

    err = mdns_resp_add_netif(nif, nif->hostname);
    if (err == ERR_OK) {
        ++*count;
    } else {
        /* E.g. host name longer than 63 characters or out of memory */
        LWIP_APPS_MDNS_LOG_ERROR("Not started on %c%c%u, err %d\n",
                                 nif->name[0], nif->name[1], nif->num, err);
    }
}

/*
 * Start mDNS responder on every interface that has host name set, default
 * interface first.  Interfaces are added by drivers in earlier sysinit
 * stages.  Failure is logged, device starts without mDNS on interface.
 */
void
lwip_mdns_init(void)
{
    struct netif *nif;
    int count = 0;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    LOCK_TCPIP_CORE();
    mdns_resp_init();
    if (netif_default != NULL && lwip_mdns_has_hostname(netif_default)) {
        lwip_mdns_add(netif_default, &count);
    }
    NETIF_FOREACH(nif) {
        if (nif != netif_default && lwip_mdns_has_hostname(nif)) {
            lwip_mdns_add(nif, &count);
        }
    }
    UNLOCK_TCPIP_CORE();
}

#endif
