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

#include <stdint.h>

#include "oic/port/mynewt/config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t port;
    uint8_t address[16];
    uint8_t scope;
} oc_ipv6_addr_t;

typedef struct {
    uint16_t port;
    uint8_t address[4];
} oc_ipv4_addr_t;

enum oc_transport_flags {
    IP = 1 << 0,
    GATT = 1 << 1,
    IPSP = 1 << 2,
    MULTICAST = 1 << 3,
    SECURED = 1 << 4,
    SERIAL = 1 << 5,
    IP4 = 1 << 6,
};

/*
 * OC endpoint data structure comes in different variations,
 * depending on flags field.
 */
/*
 * oc_endpoint for IPv6 source
 */
struct oc_endpoint_ip {
    enum oc_transport_flags flags;
    union {
        oc_ipv6_addr_t v6;
        oc_ipv4_addr_t v4;
    };
};

/*
 * oc_endpoint for BLE source.
 */
struct oc_endpoint_ble {
    enum oc_transport_flags flags;
    uint8_t srv_idx;
    uint16_t conn_handle;
};

/*
 * oc_endpoint for multicast target and serial port.
 */
struct oc_endpoint_plain {
    enum oc_transport_flags flags;
};

typedef struct oc_endpoint {
    union {
        struct oc_endpoint_ip oe_ip;
        struct oc_endpoint_ble oe_ble;
        struct oc_endpoint_plain oe;
    };
} oc_endpoint_t;

static inline int
oc_endpoint_size(struct oc_endpoint *oe)
{
    if (oe->oe.flags & (IP | IP4)) {
        return sizeof (struct oc_endpoint_ip);
    } else if (oe->oe.flags & GATT) {
        return sizeof (struct oc_endpoint_ble);
    } else if (oe->oe.flags & SERIAL) {
        return sizeof (struct oc_endpoint_plain);
    } else {
        return sizeof (struct oc_endpoint);
    }
}

#define OC_MBUF_ENDPOINT(m)                                            \
    ((struct oc_endpoint *)((uint8_t *)m + sizeof(struct os_mbuf) +    \
                            sizeof(struct os_mbuf_pkthdr)))


#define oc_make_ip_endpoint(__name__, __flags__, __port__, ...)         \
    oc_endpoint_t __name__ = {.oe_ip = {.flags = __flags__,             \
                                        .v6 = {.port = __port__,        \
                                               .address = { __VA_ARGS__ } } } }
#define oc_make_ip4_endpoint(__name__, __flags__, __port__, ...)        \
    oc_endpoint_t __name__ = {.oe_ip = {.flags = __flags__,             \
                                        .v4 = {.port = __port__,        \
                                               .address = { __VA_ARGS__ } } } }

#ifdef OC_SECURITY
uint16_t oc_connectivity_get_dtls_port(void);
#endif /* OC_SECURITY */

enum oc_resource_properties
oc_get_trans_security(const struct oc_endpoint *oe);
int oc_connectivity_init(void);
void oc_connectivity_shutdown(void);

static inline int
oc_endpoint_use_tcp(struct oc_endpoint *oe)
{
    if (oe->oe.flags & GATT) {
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
