/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef OC_CONNECTIVITY_H
#define OC_CONNECTIVITY_H

#include "mynewt/config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t port;
    uint8_t address[16];
    uint8_t scope;
} oc_ipv6_addr_t;

typedef struct {
    uint8_t type;
    uint8_t address[6];
    uint16_t conn_handle;
} oc_le_addr_t;

typedef struct oc_endpoint {
    enum transport_flags {
        IP = 1 << 0,
        GATT = 1 << 1,
        IPSP = 1 << 2,
        MULTICAST = 1 << 3,
        SECURED = 1 << 4,
        SERIAL = 1 <<5,
    } flags;

    union {
        oc_ipv6_addr_t ipv6_addr;
        oc_le_addr_t bt_addr;
    };
} oc_endpoint_t;

#define OC_MBUF_ENDPOINT(m)                                            \
    ((struct oc_endpoint *)((uint8_t *)m + sizeof(struct os_mbuf) +    \
                            sizeof(struct os_mbuf_pkthdr)))


#define oc_make_ip_endpoint(__name__, __flags__, __port__, ...)                \
  oc_endpoint_t __name__ = {.flags = __flags__,                                \
                            .ipv6_addr = {.port = __port__,                    \
                                          .address = { __VA_ARGS__ } } }

typedef struct oc_message {
    oc_endpoint_t endpoint;
    size_t length;
    uint8_t ref_count;
    uint8_t data[MAX_PAYLOAD_SIZE];
} oc_message_t;

#ifdef OC_SECURITY
uint16_t oc_connectivity_get_dtls_port(void);
#endif /* OC_SECURITY */

int oc_connectivity_init(void);
void oc_connectivity_shutdown(void);

static inline int
oc_endpoint_use_tcp(struct oc_endpoint *oe)
{
    if (oe->flags & GATT) {
        return 1;
    }
    return 0;
}


void oc_send_buffer(struct os_mbuf *);
void oc_send_multicast_message(struct os_mbuf *);

#ifdef __cplusplus
}
#endif

#endif /* OC_CONNECTIVITY_H */
