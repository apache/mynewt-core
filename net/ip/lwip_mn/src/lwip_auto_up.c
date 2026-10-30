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
#include <lwip/dhcp.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>

/*
 * Bring up interfaces which drivers added but left down, for applications
 * that do not configure network interfaces themselves.  Interfaces that
 * are already up (e.g. loopback, or configured by their driver) are not
 * touched.  First interface brought up becomes default route.
 */
void
lwip_auto_up_init(void)
{
    struct netif *nif;
    int first = 1;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    LOCK_TCPIP_CORE();
    NETIF_FOREACH(nif) {
        if (netif_is_up(nif)) {
            continue;
        }
        netif_set_up(nif);
        if (first) {
            netif_set_default(nif);
            first = 0;
        }
#if LWIP_DHCP && MYNEWT_VAL(LWIP_AUTO_UP_DHCP)
        dhcp_start(nif);
#endif
    }
    UNLOCK_TCPIP_CORE();
}
