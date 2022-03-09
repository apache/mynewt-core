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

#ifndef _HW_DRIVERS_IPC_NRF5340_PRIV_H
#define _HW_DRIVERS_IPC_NRF5340_PRIV_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialization structure passed from APP core to NET core.
 * Keeps various parameters that otherwise should be configured on
 * both sides.
 */
struct ipc_shared {
    /** NET core embedded image address in application flash */
    void *net_core_image_address;
    /** NET core embedded image size */
    uint32_t net_core_image_size;
    /** Number of IPC channels */
    uint8_t ipc_channel_count;
    /* Array of shared memories used for IPC */
    struct ipc_shm *ipc_shms;
    /* Set by netcore during IPC initialization */
    volatile enum {
        APP_WAITS_FOR_NET,
        APP_AND_NET_RUNNING,
        NET_RESTARTED,
    } ipc_state;
#if MYNEWT_VAL(BLE_TRANSPORT_INT_FLOW_CTL)
    uint8_t acl_from_ll_count;
#endif
};

#ifdef __cplusplus
}
#endif

#endif /* _HW_DRIVERS_IPC_NRF5340_PRIV_H */
