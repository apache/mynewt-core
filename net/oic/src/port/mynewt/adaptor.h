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

#ifndef ADAPTOR_H
#define ADAPTOR_H

#ifdef __cplusplus
extern "C" {
#endif

enum oc_resource_properties;

struct os_eventq *oc_evq_get(void);

#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV6) == 1)
int oc_connectivity_init_ip6(void);
void oc_connectivity_shutdown_ip6(void);
void oc_send_buffer_ip6(struct os_mbuf *);
void oc_send_buffer_ip6_mcast(struct os_mbuf *);
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV4) == 1)
int oc_connectivity_init_ip4(void);
void oc_connectivity_shutdown_ip4(void);
void oc_send_buffer_ip4(struct os_mbuf *);
void oc_send_buffer_ip4_mcast(struct os_mbuf *);
#endif

#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
int oc_connectivity_init_gatt(void);
void oc_connectivity_shutdown_gatt(void);
void oc_send_buffer_gatt(struct os_mbuf *);
enum oc_resource_properties
oc_get_trans_security_gatt(const struct oc_endpoint_ble *oe_ble);
#endif

#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
int oc_connectivity_init_serial(void);
void oc_connectivity_shutdown_serial(void);
void oc_send_buffer_serial(struct os_mbuf *);
#endif

#if (MYNEWT_VAL(OC_TRANSPORT_LORA) == 1)
int oc_connectivity_init_lora(void);
void oc_connectivity_shutdown_lora(void);
void oc_send_buffer_lora(struct os_mbuf *);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ADAPTOR_H */

