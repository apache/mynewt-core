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

/*
 * Start mDNS responder on every interface that has host name set.
 * Interfaces are added by drivers in earlier sysinit stages.
 */
void
lwip_mdns_init(void)
{
    struct netif *nif;
    err_t err = ERR_OK;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    LOCK_TCPIP_CORE();
    mdns_resp_init();
    NETIF_FOREACH(nif) {
        if (nif->hostname != NULL && nif->hostname[0] != '\0') {
            err = mdns_resp_add_netif(nif, nif->hostname,
                                      MYNEWT_VAL(LWIP_APPS_MDNS_TTL));
            if (err != ERR_OK) {
                break;
            }
        }
    }
    UNLOCK_TCPIP_CORE();

    /* E.g. host name longer than 63 characters or out of memory */
    SYSINIT_PANIC_ASSERT(err == ERR_OK);
}

#endif
