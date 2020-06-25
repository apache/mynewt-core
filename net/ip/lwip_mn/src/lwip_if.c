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
#include <limits.h>
#include <string.h>

#include "os/mynewt.h"
#include <mn_socket/mn_socket.h>

#include <lwip/tcpip.h>
#include <lwip/netif.h>
#include <lwip/ip_addr.h>

#include "ip_priv.h"

static uint8_t
lwip_if_flags(uint8_t if_flags)
{
    uint8_t flags;

    flags = 0;

    if (if_flags & NETIF_FLAG_UP) {
        flags |= MN_ITF_F_UP;
    }
    if (if_flags & NETIF_FLAG_LINK_UP) {
        flags |= MN_ITF_F_LINK;
    }
    if (if_flags & (NETIF_FLAG_IGMP | NETIF_FLAG_MLD6)) {
        flags |= MN_ITF_F_MULTICAST;
    }
    return flags;
}

int
lwip_itf_getnext(struct mn_itf *mi)
{
    int rc;
    int prev_idx, cur_idx;
    struct netif *nif;

    if (mi->mif_name[0] == '\0') {
        prev_idx = -1;
    } else {
        prev_idx = mi->mif_idx;
    }
    mi->mif_idx = UCHAR_MAX;

    rc = MN_ENOBUFS;
    LOCK_TCPIP_CORE();
    for (nif = netif_list; nif; nif = nif->next) {
        cur_idx = nif->num;
        if (cur_idx <= prev_idx || cur_idx >= mi->mif_idx) {
            continue;
        }
        memcpy(mi->mif_name, nif->name, sizeof(nif->name));
        mi->mif_name[sizeof(nif->name)] = '0' + nif->num;
        mi->mif_name[sizeof(nif->name) + 1] = '\0';
        mi->mif_idx = cur_idx;
        mi->mif_flags = lwip_if_flags(nif->flags);
        rc = 0;
    }
    UNLOCK_TCPIP_CORE();
    return rc;
}

static int
plen(void *addr, int alen)
{
    int i;
    int j;
    uint8_t b;

    for (i = 0; i < alen; i++) {
        b = ((uint8_t *)addr)[i];
        if (b == 0xff) {
            continue;
        }
        for (j = 0; j < 7; j++) {
            if ((b & (0x80 >> j)) == 0) {
                return i * 8 + j;
            }
        }
    }
    return alen * 8;
}

int
lwip_itf_addr_getnext(struct mn_itf *mi, struct mn_itf_addr *mia)
{
    struct netif *nif;
#if LWIP_IPV6
    int i;
    int copy_next = 0;
#endif

    LOCK_TCPIP_CORE();
    nif = netif_find(mi->mif_name);
    if (!nif) {
        UNLOCK_TCPIP_CORE();
        return MN_EINVAL;
    }

    if (mia->mifa_family < MN_AF_INET) {
#if LWIP_IPV4
        if (!ip_addr_isany(&nif->ip_addr)) {
            mia->mifa_family = MN_AF_INET;
            memcpy(&mia->mifa_addr, ip_2_ip4(&nif->ip_addr),
              sizeof(struct mn_in_addr));
            mia->mifa_plen = plen(ip_2_ip4(&nif->netmask),
              sizeof(struct mn_in_addr));
            UNLOCK_TCPIP_CORE();
            return 0;
        }
#endif
    }
#if LWIP_IPV6
    if (mia->mifa_family < MN_AF_INET6) {
        copy_next = 1;
    }
    for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (!ip6_addr_isvalid(netif_ip6_addr_state(nif, i))) {
            continue;
        }
        if (copy_next) {
            mia->mifa_family = MN_AF_INET6;
            memcpy(&mia->mifa_addr, netif_ip6_addr(nif, i),
              sizeof(struct mn_in6_addr));
            mia->mifa_plen = 64;
            UNLOCK_TCPIP_CORE();
            return 0;
        }
        if (!memcmp(&mia->mifa_addr, netif_ip6_addr(nif, i),
            sizeof(struct mn_in6_addr))) {
            copy_next = 1;
        }
    }
#endif
    UNLOCK_TCPIP_CORE();
    return -1;
}
