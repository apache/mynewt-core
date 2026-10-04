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

#ifndef _HW_DRIVERS_IPC_ICBMSG_H
#define _HW_DRIVERS_IPC_ICBMSG_H

#include <stdint.h>
#include <stdbool.h>
#include <os/os.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ipc_service_cb {
    void (*received)(const void *data, size_t len, void *user_data);
};

struct ipc_ept_cfg {
    const char *name;
    struct ipc_service_cb cb;
    void *user_data;
    uint8_t tx_channel;
    uint8_t rx_channel;
};

struct ipc_icmsg_buf {
    size_t block_id;
    uint8_t *data;
    uint16_t len;
};

typedef void (*ipc_icbmsg_recv_cb)(uint8_t ipc_id, void *user_data);

uint8_t ipc_icmsg_register_ept(uint8_t ipc_id, struct ipc_ept_cfg *cfg);

int ipc_icbmsg_send(uint8_t ipc_id, uint8_t ept_addr, const void *data, uint16_t len);

int ipc_icbmsg_send_buf(uint8_t ipc_id, uint8_t ept_addr, struct ipc_icmsg_buf *buf);

int ipc_icbmsg_alloc_tx_buf(uint8_t ipc_id, struct ipc_icmsg_buf *buf, uint32_t size);

uint8_t ipc_icsmsg_ept_ready(uint8_t ipc_id, uint8_t ept_addr);

#ifdef __cplusplus
}
#endif

#endif /* _HW_DRIVERS_IPC_ICBMSG_H */
