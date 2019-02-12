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

#ifndef __OIC_MYNEWT_BLE_H_
#define __OIC_MYNEWT_BLE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * oc_endpoint for BLE source.
 */
struct oc_endpoint_ble {
    struct oc_ep_hdr ep;
    uint8_t srv_idx;
    uint16_t conn_handle;
};

/**
 * @brief Indicates whether the provided endpoint is the GATT (BLE) endpoint.
 *
 * @param oe                    The endpoint to inspect.
 *
 * @return                      1 if `oe` is the GATT endpoint; 0 otherwise.
 */
int oc_endpoint_is_gatt(const struct oc_endpoint *oe);


/**
 * @brief Indicates whether the endpoints belong to same GATT (BLE) connection.
 *
 * @param oe1                   The endpoint to compare.
 * @param oe2                   The endpoint to compare.
 *
 * @return                      1 if yes; 0 otherwise.
 */
int oc_endpoint_gatt_conn_eq(const struct oc_endpoint *oe1,
                             const struct oc_endpoint *oe2);

#ifdef __cplusplus
}
#endif

#endif /* __OIC_MYNEWT_BLE_H_ */
