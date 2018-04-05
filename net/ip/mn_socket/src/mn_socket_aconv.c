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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "os/mynewt.h"
#include "mn_socket/mn_socket.h"

int
mn_inet_pton(int af, const char *src, void *dst)
{
    const char *ch_src;
    uint8_t *ch_tgt;
    int val;
    int cnt;

    if (af == MN_PF_INET) {
        cnt = 0;
        ch_tgt = dst;
        val = 0;
        for (ch_src = src; *ch_src; ch_src++) {
            if (cnt > 4) {
                return 0;
            }
            if (isdigit(*ch_src)) {
                val = val * 10 + *ch_src - '0';
                if (val > 255) {
                    return 0;
                }
                *ch_tgt = val;
            } else if (*ch_src == '.') {
                ++cnt;
                val = 0;
                ch_tgt++;
            } else {
                return 0;
            }
        }
        return 1;
    } else {
        /*
         * Add code here. XXX
         */
        return 0;
    }
}

const char *
mn_inet_ntop(int af, const void *src_v, void *dst, int len)
{
    const unsigned char *src = src_v;
    const struct mn_in6_addr *a6;
    uint16_t u16;
    int rc;
    int i;

    if (af == MN_PF_INET) {
        rc = snprintf(dst, len, "%u.%u.%u.%u",
          src[0], src[1], src[2], src[3]);
        if (rc >= len) {
            return NULL;
        } else {
            return dst;
        }
    } else {
        a6 = src_v;
        rc = 0;

        for (i = 0; i < sizeof(*a6); i += 2) {
            memcpy(&u16, &a6->s_addr[i], 2);
            u16 = htons(u16);
            rc += snprintf(dst + rc, len - rc, "%x", u16);
            if (rc >= len) {
                return NULL;
            }
            if (i < sizeof(*a6) - 2) {
                rc += snprintf(dst + rc, len - rc, ":");
                if (rc >= len) {
                    return NULL;
                }
            }
        }
        return dst;
    }
}
