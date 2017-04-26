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
#include <ifaddrs.h>
#include <net/if.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mn_socket/mn_socket.h"
#include "native_sock_priv.h"

static uint8_t
itf_flags(int if_flags)
{
    uint8_t flags;

    flags = 0;

    if (if_flags & IFF_UP) {
        flags |= MN_ITF_F_UP;
    }
    if (if_flags & IFF_RUNNING) {
        flags |= MN_ITF_F_LINK;
    }
    if (if_flags & IFF_MULTICAST) {
        flags |= MN_ITF_F_MULTICAST;
    }
    return flags;
}

int
native_sock_itf_getnext(struct mn_itf *mi)
{
    int prev_idx, cur_idx;
    struct ifaddrs *ifap;
    struct ifaddrs *ifa;
    int rc;

    if (mi->mif_name[0] == '\0') {
        prev_idx = 0;
    } else {
        prev_idx = mi->mif_idx;
    }
    mi->mif_idx = UCHAR_MAX;
    rc = getifaddrs(&ifap);
    if (rc < 0) {
        rc = native_sock_err_to_mn_err(errno);
        return rc;
    }

    rc = MN_ENOBUFS;
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        cur_idx = if_nametoindex(ifa->ifa_name);
        if (cur_idx <= prev_idx || cur_idx >= mi->mif_idx) {
            continue;
        }
        strncpy(mi->mif_name, ifa->ifa_name, sizeof(mi->mif_name));
        mi->mif_idx = cur_idx;
        mi->mif_flags = itf_flags(ifa->ifa_flags);
        rc = 0;
    }
    freeifaddrs(ifap);
    return rc;
}

static int
addrcmp(uint8_t fam1, void *addr1, uint8_t fam2, void *addr2, int alen)
{
    if (fam1 != fam2) {
        return fam1 - fam2;
    }
    return memcmp(addr1, addr2, alen);
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
native_sock_itf_addr(int idx, uint32_t *addr)
{
    struct ifaddrs *ifap;
    struct ifaddrs *ifa;
    struct sockaddr_in *sin;
    int rc;

    rc = getifaddrs(&ifap);
    if (rc < 0) {
        rc = native_sock_err_to_mn_err(errno);
        return rc;
    }

    rc = MN_EADDRNOTAVAIL;
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (if_nametoindex(ifa->ifa_name) != idx) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) {
            sin = (struct sockaddr_in *)ifa->ifa_addr;
            *addr = sin->sin_addr.s_addr;
            rc = 0;
            break;
        }
    }
    freeifaddrs(ifap);
    return rc;
}

int
native_sock_itf_addr_getnext(struct mn_itf *mi, struct mn_itf_addr *mia)
{
    struct ifaddrs *ifap;
    struct ifaddrs *ifa;
    struct sockaddr_in *sin;
    struct sockaddr_in6 *sin6;
    int rc;
    uint8_t prev_family;
    uint8_t prev_addr[16];

    rc = getifaddrs(&ifap);
    if (rc < 0) {
        rc = native_sock_err_to_mn_err(errno);
        return rc;
    }

    prev_family = mia->mifa_family;
    memcpy(prev_addr, &mia->mifa_addr, sizeof(mia->mifa_addr));
    mia->mifa_family = UCHAR_MAX;
    memset(&mia->mifa_addr, 0xff, sizeof(mia->mifa_addr));

    rc = MN_ENOBUFS;

    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (if_nametoindex(ifa->ifa_name) != mi->mif_idx) {
            continue;
        }
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) {
            sin = (struct sockaddr_in *)ifa->ifa_addr;
            if (addrcmp(MN_AF_INET, &sin->sin_addr,
                prev_family, prev_addr,
                sizeof(struct in_addr)) <= 0) {
                continue;
            }
            if (addrcmp(MN_AF_INET, &sin->sin_addr,
                mia->mifa_family, &mia->mifa_addr,
                sizeof(struct in_addr)) >= 0) {
                continue;
            }
            mia->mifa_family = MN_AF_INET;
            memcpy(&mia->mifa_addr, &sin->sin_addr, sizeof(struct in_addr));

            sin = (struct sockaddr_in *)ifa->ifa_netmask;
            mia->mifa_plen = plen(&sin->sin_addr, sizeof(struct in_addr));
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
            if (addrcmp(MN_AF_INET6, &sin6->sin6_addr,
                prev_family, prev_addr,
                sizeof(struct in6_addr)) <= 0) {
                continue;
            }
            if (addrcmp(MN_AF_INET6, &sin6->sin6_addr,
                mia->mifa_family, &mia->mifa_addr,
                sizeof(struct in6_addr)) >= 0) {
                continue;
            }
            mia->mifa_family = MN_AF_INET6;
            memcpy(&mia->mifa_addr, &sin6->sin6_addr, sizeof(struct in6_addr));

            sin6 = (struct sockaddr_in6 *)ifa->ifa_netmask;
            mia->mifa_plen = plen(&sin6->sin6_addr, sizeof(struct in6_addr));
        } else {
            continue;
        }
        rc = 0;
    }
    freeifaddrs(ifap);
    return rc;
}


