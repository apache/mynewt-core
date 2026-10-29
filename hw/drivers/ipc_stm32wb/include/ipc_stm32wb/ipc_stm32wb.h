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

#ifndef _HW_DRIVERS_IPC_STM32WB_H
#define _HW_DRIVERS_IPC_STM32WB_H

#include <stdint.h>
#include <os/os_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Driver for the mailbox (IPCC + shared SRAM2) used by STM32WB CPU2 wireless
 * firmware. Packets exchanged with CPU2 start with HCI packet indicator
 * (as in H:4) followed by HCI packet.
 */

#define IPC_STM32WB_PKT_HCI_CMD              0x01
#define IPC_STM32WB_PKT_HCI_ACL              0x02
#define IPC_STM32WB_PKT_HCI_EVT              0x04
#define IPC_STM32WB_PKT_SYS_CMD              0x10
#define IPC_STM32WB_PKT_SYS_RSP              0x11
#define IPC_STM32WB_PKT_SYS_EVT              0x12

/* System command opcodes (OGF 0x3f) */
#define IPC_STM32WB_SYS_OP(ocf)              ((0x3f << 10) | (ocf))
#define IPC_STM32WB_SYS_FUS_GET_STATE        IPC_STM32WB_SYS_OP(0x52)
#define IPC_STM32WB_SYS_BLE_INIT             IPC_STM32WB_SYS_OP(0x66)
#define IPC_STM32WB_SYS_FLASH_ERASE_ACTIVITY IPC_STM32WB_SYS_OP(0x69)

/* Wireless firmware stack types (lowest byte of info_stack) */
#define IPC_STM32WB_STACK_BLE_FULL           0x01
#define IPC_STM32WB_STACK_BLE_HCI            0x02
#define IPC_STM32WB_STACK_BLE_LIGHT          0x03
#define IPC_STM32WB_STACK_BLE_BEACON         0x04
#define IPC_STM32WB_STACK_BLE_BASIC          0x05
#define IPC_STM32WB_STACK_BLE_FULL_EXT_ADV   0x06
#define IPC_STM32WB_STACK_BLE_HCI_EXT_ADV    0x07

/* Version words are major.minor.sub in bits 31..8 */
#define IPC_STM32WB_VER_MAJOR(v)             (((v) >> 24) & 0xff)
#define IPC_STM32WB_VER_MINOR(v)             (((v) >> 16) & 0xff)
#define IPC_STM32WB_VER_SUB(v)               (((v) >> 8) & 0xff)

struct ipc_stm32wb_fw_info {
    uint32_t safeboot_version;
    uint32_t fus_version;
    uint32_t fus_memory_size;
    uint32_t ws_version;
    uint32_t ws_memory_size;
    uint32_t ws_info_stack;
};

typedef void ipc_stm32wb_cb(void *arg);

/**
 * Waits for CPU2 ready event.
 *
 * @param timeout_ms    Maximum time to wait.
 *
 * @return 0 when wireless stack is running,
 *         SYS_ENOENT when only FUS is running (no wireless stack installed),
 *         SYS_ETIMEOUT when CPU2 did not report ready in time.
 */
int ipc_stm32wb_wait_ready(uint32_t timeout_ms);

/**
 * Reads firmware versions reported by CPU2. Valid after CPU2 is ready.
 */
void ipc_stm32wb_fw_info_get(struct ipc_stm32wb_fw_info *info);

/**
 * Sends system command to CPU2 and waits for response. Must be called
 * from task context.
 *
 * @param opcode        System command opcode.
 * @param params        Command parameters.
 * @param len           Length of parameters.
 * @param status        Status from command complete response.
 *
 * @return 0 on success, SYS_ETIMEOUT if there was no response.
 */
int ipc_stm32wb_sys_cmd(uint16_t opcode, const void *params, uint8_t len,
                        uint8_t *status);

/**
 * Sets callbacks for BLE channels. Both are called from interrupt context.
 *
 * @param rx_cb         Called when new BLE packets were received.
 * @param tx_cb         Called when BLE command or ACL channel becomes free.
 * @param arg           Argument passed to callbacks.
 */
void ipc_stm32wb_ble_cb_set(ipc_stm32wb_cb *rx_cb, ipc_stm32wb_cb *tx_cb, void *arg);

/**
 * Starts reception of BLE packets. To be called after BLE stack on CPU2
 * was initialized.
 */
void ipc_stm32wb_ble_enable(void);

/**
 * Sends HCI command.
 *
 * @param cmd           HCI command (opcode, length, parameters).
 *
 * @return 0 on success, SYS_EBUSY when command channel is busy.
 */
int ipc_stm32wb_ble_cmd_send(const uint8_t *cmd);

/**
 * Sends HCI ACL data packet. Data are copied, mbuf is not freed.
 *
 * @param om            ACL packet (handle, length, data).
 *
 * @return 0 on success, SYS_EBUSY when ACL channel is busy,
 *         SYS_EINVAL when packet is too long.
 */
int ipc_stm32wb_ble_acl_send(const struct os_mbuf *om);

/**
 * Gets next packet received on BLE channel.
 *
 * @return Pointer to packet indicator byte followed by HCI event or
 *         ACL packet, NULL if there is nothing to read.
 */
uint8_t *ipc_stm32wb_ble_rx_get(void);

/**
 * Returns packet obtained by ipc_stm32wb_ble_rx_get() to CPU2.
 */
void ipc_stm32wb_ble_rx_release(uint8_t *pkt);

#ifdef __cplusplus
}
#endif

#endif /* _HW_DRIVERS_IPC_STM32WB_H */
