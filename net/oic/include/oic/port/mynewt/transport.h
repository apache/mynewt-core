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

#ifndef __MYNEWT_TRANSPORT_H_
#define __MYNEWT_TRANSPORT_H_

#include "os/mynewt.h"
#include "oic/oc_ri.h"
#include "oic/oc_ri_const.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OC_TRANSPORT_MAX 8

#define OC_TRANSPORT_USE_TCP (1 << 0)

struct oc_endpoint;

struct oc_transport {
    uint8_t ot_flags;
    uint8_t (*ot_ep_size)(const struct oc_endpoint *);
    void (*ot_tx_ucast)(struct os_mbuf *);
    void (*ot_tx_mcast)(struct os_mbuf *);
    enum oc_resource_properties
         (*ot_get_trans_security)(const struct oc_endpoint *);
    char *(*ot_ep_str)(char *ptr, int maxlen, const struct oc_endpoint *);
    int (*ot_init)(void);
    void (*ot_shutdown)(void);
};

extern const struct oc_transport *oc_transports[OC_TRANSPORT_MAX];

int8_t oc_transport_register(const struct oc_transport *);
int8_t oc_transport_lookup(const struct oc_transport *);
void oc_transport_unregister(const struct oc_transport *);

#ifdef __cplusplus
}
#endif

#endif /* __MYNEWT_TRANSPORT_H_ */
