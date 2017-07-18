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

#ifdef __cplusplus
extern "C" {
#endif

/* Unsecure CoAP service: ADE3D529-C784-4F63-A987-EB69F70EE816 */
#define OC_GATT_UNSEC_SVC_UUID                      \
    0x16, 0xe8, 0x0e, 0xf7, 0x69, 0xeb, 0x87, 0xa9, \
    0x63, 0x4f, 0x84, 0xc7, 0x29, 0xd5, 0xe3, 0xad

/* Unsecure CoAP req. characteristic: AD7B334F-4637-4B86-90B6-9D787F03D218 */
#define OC_GATT_UNSEC_REQ_CHR_UUID                  \
    0x18, 0xd2, 0x03, 0x7f, 0x78, 0x9d, 0xb6, 0x90, \
    0x86, 0x4b, 0x37, 0x46, 0x4f, 0x33, 0x7b, 0xad

/* Unsecure CoAP rsp. characteristic: E9241982-4580-42C4-8831-95048216B256 */
#define OC_GATT_UNSEC_RSP_CHR_UUID                  \
    0x56, 0xb2, 0x16, 0x82, 0x04, 0x95, 0x31, 0x88, \
    0xc4, 0x42, 0x80, 0x45, 0x82, 0x19, 0x24, 0xe9

/* Secure CoAP service: 0xfe18 */
#define OC_GATT_SEC_SVC_UUID        0xfe18

/* Secure CoAP req. characteristic: 0x1000 */
#define OC_GATT_SEC_REQ_CHR_UUID    0x1000

/* Secure CoAP rsp. characteristic: 0x1001 */
#define OC_GATT_SEC_RSP_CHR_UUID    0x1001

int oc_ble_coap_gatt_srv_init(void);
void oc_ble_coap_conn_new(uint16_t conn_handle);
void oc_ble_coap_conn_del(uint16_t conn_handle);

#ifdef __cplusplus
}
#endif

#endif /* OC_GATT_H */

