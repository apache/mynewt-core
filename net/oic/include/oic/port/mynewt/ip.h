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

#ifndef __OIC_MYNEWT_IP_H_
#define __OIC_MYNEWT_IP_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t address[16];
    uint8_t scope;
} oc_ipv6_addr_t;

typedef struct {
    uint8_t address[4];
} oc_ipv4_addr_t;

/*
 * oc_endpoint for IPv4/IPv6
 */
struct oc_endpoint_ip {
    struct oc_ep_hdr ep;
    uint16_t port;
    union {
        oc_ipv6_addr_t v6;
        oc_ipv4_addr_t v4;
    };
};

extern uint8_t oc_ip6_transport_id;
extern uint8_t oc_ip4_transport_id;

static inline int
oc_endpoint_is_ip(struct oc_endpoint *oe)
{
    return oe->ep.oe_type == oc_ip6_transport_id ||
      oe->ep.oe_type == oc_ip4_transport_id;
}

#define oc_make_ip6_endpoint(__name__, __flags__, __port__, ...)        \
    struct oc_endpoint_ip __name__ = {.ep = {.oe_type = oc_ip6_transport_id, \
                                             .oe_flags = __flags__ },   \
                                      .port = __port__,                 \
                                      .v6 = {.scope = 0,                \
                                             .address = { __VA_ARGS__ } } }
#define oc_make_ip4_endpoint(__name__, __flags__, __port__, ...)        \
    struct oc_endpoint_ip __name__ = {.ep = {.oe_type = oc_ip4_transport_id, \
                                             .oe_flags = __flags__},    \
                                      .port = __port__,                 \
                                      .v4 = {.address = { __VA_ARGS__ } } }

#ifdef __cplusplus
}
#endif

#endif /* __OIC_MYNEWT_IP_H_ */
