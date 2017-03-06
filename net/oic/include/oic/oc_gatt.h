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

#ifndef OC_GATT_H
#define OC_GATT_H

#include "host/ble_uuid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ADE3D529-C784-4F63-A987-EB69F70EE816 */
#define OC_GATT_SERVICE_UUID  0x16, 0xe8, 0x0e, 0xf7, 0x69, 0xeb, 0x87, 0xa9, \
                              0x63, 0x4f, 0x84, 0xc7, 0x29, 0xd5, 0xe3, 0xad

int oc_ble_coap_gatt_srv_init(void);
void oc_ble_coap_conn_new(uint16_t conn_handle);
void oc_ble_coap_conn_del(uint16_t conn_handle);

#ifdef __cplusplus
}
#endif

#endif /* OC_GATT_H */

