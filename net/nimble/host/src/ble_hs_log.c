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

#include <stdio.h>
#include "os/os.h"
#include "host/ble_hs.h"

#define HEXBUF_LENGTH   128
#define HEXBUF_COUNT    4

static const char hex[] = "0123456789abcdef";

struct log ble_hs_log;

void
ble_hs_log_mbuf(const struct os_mbuf *om)
{
    uint8_t u8;
    int i;

    for (i = 0; i < OS_MBUF_PKTLEN(om); i++) {
        os_mbuf_copydata(om, i, 1, &u8);
        BLE_HS_LOG(DEBUG, "0x%02x ", u8);
    }
}

void
ble_hs_log_flat_buf(const void *data, int len)
{
    const uint8_t *u8ptr;
    int i;

    u8ptr = data;
    for (i = 0; i < len; i++) {
        BLE_HS_LOG(DEBUG, "0x%02x ", u8ptr[i]);
    }
}

const char *
ble_addr_str(const ble_addr_t *addr)
{
    static char bufs[2][27];
    static uint8_t cur;
    char type[7];
    char *str;

    str = bufs[cur++];
    cur %= sizeof(bufs) / sizeof(bufs[0]);

    if (!addr) {
        return "(none)";
    }

    switch (addr->type) {
    case BLE_ADDR_PUBLIC:
        strcpy(type, "public");
        break;
    case BLE_ADDR_RANDOM:
        strcpy(type, "random");
        break;
    default:
        snprintf(type, sizeof(type), "0x%02x", addr->type);
        break;
    }

    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X (%s)",
            addr->val[5], addr->val[4], addr->val[3],
            addr->val[2], addr->val[1], addr->val[0], type);

    return str;
}

static char *
get_buf(void)
{
    static char hexbufs[HEXBUF_COUNT][HEXBUF_LENGTH + 1];
    static uint8_t curbuf;
    char *str;
    int sr;

    OS_ENTER_CRITICAL(sr);
    str = hexbufs[curbuf++];
    curbuf %= sizeof(hexbufs) / sizeof(hexbufs[0]);
    OS_EXIT_CRITICAL(sr);

    return str;
}

const char *
ble_hex(const void *buf, size_t len)
{
    const uint8_t *b = buf;
    char *str;
    int i;

    str = get_buf();

    len = min(len, (HEXBUF_LENGTH) / 2);

    for (i = 0; i < len; i++) {
        str[i * 2]     = hex[b[i] >> 4];
        str[i * 2 + 1] = hex[b[i] & 0xf];
    }

    str[i * 2] = '\0';

    return str;
}

const char *
ble_hex_mbuf(const struct os_mbuf *om)
{
    uint8_t b;
    char *str;
    size_t len;
    int i;

    str = get_buf();

    len = min(OS_MBUF_PKTLEN(om), (HEXBUF_LENGTH) / 2);

    for (i = 0; i < len; i++) {
        os_mbuf_copydata(om, i, 1, &b);
        str[i * 2]     = hex[b >> 4];
        str[i * 2 + 1] = hex[b & 0xf];
    }

    str[i * 2] = '\0';

    return str;
}
