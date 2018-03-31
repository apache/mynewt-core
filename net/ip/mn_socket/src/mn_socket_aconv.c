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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "os/mynewt.h"
#include "mn_socket/mn_socket.h"

/**
 * Indicates the length of an IPv6 address segment (i.e., 0 to 4 characters
 * enclosed by ':').
 */
static int
mn_inet6_segment_len(const char *s)
{
    int i;

    for (i = 0; s[i] != '\0'; i++) {
        if (s[i] == ':') {
            break;
        }
    }

    return i;
}


/**
 * Counts the number of segments in the specified IPv6 address string.
 *
 * @param s                     The input string to parse.
 * @param pre                   On success, the number of segments before the
 *                                  "::" string gets written here.  8 if there
 *                                  is no "::" string.
 * @param mid                   On success, the number of segments compressed
 *                                  by a "::" string gets written here.  0 If
 *                                  there is no "::" string.
 * @param post                  On success, the number of segments after
 *                                  the "::" string gets written here.  0 If
 *                                  there is no "::" string.
 *
 * @return                      0 on success; -1 on failure.
 */
static int
mn_inet6_segment_count(const char *s, int *pre, int *mid, int *post)
{
    bool compressed;
    int total_segs;
    int seglen;
    int off;

    *pre = 0;
    *mid = 0;
    *post = 0;
    compressed = false;

    off = 0;
    while (1) {
        if (s[off] == ':') {
            off++;
        }

        seglen = mn_inet6_segment_len(s + off);
        if (seglen == 0) {
            if (s[off] == '\0') {
                break;
            }

            if (compressed) {
                return -1;
            }

            compressed = true;
        } else if (!compressed) {
            (*pre)++;
        } else {
            (*post)++;
        }

        off += seglen;
    }

    total_segs = *pre + *post;
    if (!compressed) {
        if (total_segs != 8) {
            return -1;
        }
    } else {
        *mid = 8 - total_segs;
        if (*mid <= 0) {
            return -1;
        }
    }

    return 0;
}

/**
 * Converts a single IPv6 address segment string to four-byte array.
 */
static int
mn_inet6_pton_segment(const char *s, void *dst)
{
    uint16_t u16;
    char buf[5];
    char *endptr;
    int seglen;

    seglen = mn_inet6_segment_len(s);
    if (seglen == 0 || seglen > 4) {
        return -1;
    }

    memcpy(buf, s, seglen);
    buf[seglen] = '\0';

    u16 = strtoul(buf, &endptr, 16);
    if (*endptr != '\0') {
        return -1;
    }

    u16 = htons(u16);
    memcpy(dst, &u16, sizeof u16);

    return seglen;
}

/**
 * Converts an IPv6 address string to its binary representation.
 */
static int
mn_inet6_pton(const char *s, void *dst)
{
    uint8_t *u8p;
    int post;
    int mid;
    int pre;
    int soff;
    int doff;
    int rc;
    int i;

    rc = mn_inet6_segment_count(s, &pre, &mid, &post);
    if (rc != 0) {
        return rc;
    }

    u8p = dst;
    soff = 0;
    doff = 0;

    for (i = 0; i < pre; i++) {
        rc = mn_inet6_pton_segment(s + soff, u8p + doff);
        if (rc == -1) {
            return rc;
        }

        soff += rc + 1;
        doff += 2;
    }

    if (mid == 0) {
        return 0;
    }

    /* If the string started with a double-colon, skip one of the colons now.
     * Normally this gets skipped during parsing of a leading segment.
     */
    if (pre == 0) {
        soff++;
    }

    /* Skip the second colon. */
    soff++;

    memset(u8p + doff, 0, mid * 2);
    doff += mid * 2;

    for (i = 0; i < post; i++) {
        rc = mn_inet6_pton_segment(s + soff, u8p + doff);
        if (rc == -1) {
            return rc;
        }

        soff += rc + 1;
        doff += 2;
    }

    return 0;
}

int
mn_inet_pton(int af, const char *src, void *dst)
{
    const char *ch_src;
    uint8_t *ch_tgt;
    int val;
    int cnt;
    int rc;

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
        rc = mn_inet6_pton(src, dst);
        if (rc != 0) {
            return 0;
        }
        return 1;
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
