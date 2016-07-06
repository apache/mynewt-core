/**
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

#include "console/console.h"
#include "host/ble_uuid.h"
#include "host/ble_gap.h"

#include "bletiny.h"

/**
 * Utility function to log an array of bytes.
 */
void
print_bytes(uint8_t *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        console_printf("%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

void
print_addr(void *addr)
{
    uint8_t *u8p;

    u8p = addr;
    console_printf("%02x:%02x:%02x:%02x:%02x:%02x",
                   u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

void
print_uuid(void *uuid128)
{
    uint16_t uuid16;
    uint8_t *u8p;

    uuid16 = ble_uuid_128_to_16(uuid128);
    if (uuid16 != 0) {
        console_printf("0x%04x", uuid16);
        return;
    }

    u8p = uuid128;

    /* 00001101-0000-1000-8000-00805f9b34fb */
    console_printf("%02x%02x%02x%02x-", u8p[15], u8p[14], u8p[13], u8p[12]);
    console_printf("%02x%02x-%02x%02x-", u8p[11], u8p[10], u8p[9], u8p[8]);
    console_printf("%02x%02x%02x%02x%02x%02x%02x%02x",
                   u8p[7], u8p[6], u8p[5], u8p[4],
                   u8p[3], u8p[2], u8p[1], u8p[0]);
}

int
svc_is_empty(struct bletiny_svc *svc)
{
    return svc->svc.end_handle < svc->svc.start_handle;
}

uint16_t
chr_end_handle(struct bletiny_svc *svc, struct bletiny_chr *chr)
{
    struct bletiny_chr *next_chr;

    next_chr = SLIST_NEXT(chr, next);
    if (next_chr != NULL) {
        return next_chr->chr.def_handle - 1;
    } else {
        return svc->svc.end_handle;
    }
}

int
chr_is_empty(struct bletiny_svc *svc, struct bletiny_chr *chr)
{
    return chr_end_handle(svc, chr) <= chr->chr.val_handle;
}

void
print_conn_desc(struct ble_gap_conn_desc *desc)
{
    console_printf("handle=%d our_ota_addr_type=%d our_ota_addr=",
                   desc->conn_handle, desc->our_ota_addr_type);
    print_addr(desc->our_ota_addr);
    console_printf(" our_id_addr_type=%d our_id_addr=",
                   desc->our_id_addr_type);
    print_addr(desc->our_id_addr);
    console_printf(" peer_ota_addr_type=%d peer_ota_addr=",
                   desc->peer_ota_addr_type);
    print_addr(desc->peer_ota_addr);
    console_printf(" peer_id_addr_type=%d peer_id_addr=",
                   desc->peer_id_addr_type);
    print_addr(desc->peer_id_addr);
    console_printf(" conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                   "encrypted=%d authenticated=%d bonded=%d\n",
                   desc->conn_itvl, desc->conn_latency,
                   desc->supervision_timeout,
                   desc->sec_state.encrypted,
                   desc->sec_state.authenticated,
                   desc->sec_state.bonded);
}
