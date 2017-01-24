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

#include <string.h>

#include <log/log.h>
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1)
#include <mn_socket/mn_socket.h>
#endif
#include "oic/oc_log.h"
#include "port/oc_connectivity.h"

void
oc_log_endpoint(uint16_t lvl, struct oc_endpoint *oe)
{
    char *str;
    char tmp[46 + 6];

    (void)tmp;
    (void)str;

    switch (oe->oe.flags) {
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1)
    case IP: {
        int len;

        mn_inet_ntop(MN_PF_INET6, oe->oe_ip.v6.address, tmp, sizeof(tmp));
        len = strlen(tmp);
        snprintf(tmp + len, sizeof(tmp) - len, "-%u\n", oe->oe_ip.v6.port);
        str = tmp;
        break;
    }
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
    case GATT:
        snprintf(tmp, sizeof(tmp), "ble %u\n", oe->oe_ble.conn_handle);
        str = tmp;
        break;
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
    case SERIAL:
        str = "serial\n";
        break;
#endif
    case MULTICAST:
        str = "multicast\n";
        break;
    default:
        str = "<unkwn>\n";
        break;
    }
    log_printf(&oc_log, LOG_MODULE_IOTIVITY, lvl, str);
}

void
oc_log_bytes(uint16_t lvl, void *addr, int len, int print_char)
{
    int i;
    uint8_t *p = (uint8_t *)addr;

    (void)p;
    log_printf(&oc_log, LOG_MODULE_IOTIVITY, lvl, "[");
    for (i = 0; i < len; i++) {
        if (print_char) {
            log_printf(&oc_log, LOG_MODULE_IOTIVITY, lvl, "%c", p[i]);
        } else {
            log_printf(&oc_log, LOG_MODULE_IOTIVITY, lvl, "%02x", p[i]);
        }
    }
    log_printf(&oc_log, LOG_MODULE_IOTIVITY, lvl, "]\n");
}

void
oc_log_bytes_mbuf(uint16_t lvl, struct os_mbuf *m, int off, int len,
                  int print_char)
{
    int i;
    uint8_t tmp[4];
    int blen;

    log_printf(&oc_log, LOG_MODULE_IOTIVITY, lvl, "[");
    while (len) {
        blen = len;
        if (blen > sizeof(tmp)) {
            blen = sizeof(tmp);
        }
        os_mbuf_copydata(m, off, blen, tmp);
        for (i = 0; i < blen; i++) {
            if (print_char) {
                log_printf(&oc_log, LOG_MODULE_IOTIVITY, lvl, "%c", tmp[i]);
            } else {
                log_printf(&oc_log, LOG_MODULE_IOTIVITY, lvl, "%02x", tmp[i]);
            }
        }
        off += blen;
        len -= blen;
    }
    log_printf(&oc_log, LOG_MODULE_IOTIVITY, lvl, "]\n");
}
