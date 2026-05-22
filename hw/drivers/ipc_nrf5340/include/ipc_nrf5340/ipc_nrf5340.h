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

#ifndef _HW_DRIVERS_IPC_NRF5340_H
#define _HW_DRIVERS_IPC_NRF5340_H

#include <stdint.h>
#include <stdbool.h>
#include <os/os.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Type of callback called when signal was received on IPC
 *
 * @param channel     IPC channel number the signal was received on
 * @param user_data   User data
 */
typedef void (*ipc_nrf5340_recv_cb)(int channel, void *user_data);

/**
 * Initialize IPC driver. Should be called only once. On application core
 * this also releases networking core force-reset and thus enable it.
 */
void ipc_nrf5340_init(void);

/**
 * Reset IPC and NetCore. Can be used to re-sync with Network Core without
 * restarting Application Core.
 */
void ipc_nrf5340_reset(void);

/**
 * Enable reception on specified IPC channel. Calling with NULL callback
 * disables reception.
 *
 * @param channel     IPC channel number to receive on
 * @param cb          Callback to be called when IPC signal is received
 * @param user_data   User data passed to callback
 */
void ipc_nrf5340_recv(int channel, ipc_nrf5340_recv_cb cb, void *user_data);

/**
 * Sends data over specified IPC channel. IPC uses ring buffer for data passing.
 * If IPC_NRF5340_BLOCKING_WRITE is not enable and there is not enough space to
 * store data SYS_ENOMEM error is returned.
 *
 * @param channel     IPC channel number to send on
 * @param data        Data to be sent over IPC. If this is NULL only signal is
 *                    sent
 * @param len         Data length
 *
 * @return            0 on success and negative error on failure
 */
int ipc_nrf5340_send(int channel, const void *data, uint16_t len);

/**
 * Sends data over specified IPC channel. IPC uses ring buffer for data passing.
 * If IPC_NRF5340_BLOCKING_WRITE is not enable and there is not enough space to
 * store data SYS_ENOMEM error is returned.
 *
 * @param channel     IPC channel number to send on
 * @param data        Data to be sent over IPC. If this is NULL only signal is
 *                    sent
 * @param len         Data length
 * @param last        If this is the last send, so other side can be notified
 *
 * @return            0 on success and negative error on failure
 */
int ipc_nrf5340_write(int channel, const void *data, uint16_t len, bool last);

/**
 * Reads data from IPC ring buffer to specified flat buffer.
 *
 * Note: this function does not use any kind of locking so it's up to caller
 *       to make sure it's not used during write from other context as it may
 *       yield incorrect results.
 *
 * @param channel     IPC channel number to read from
 * @param buf         Buffer to read data to
 * @param len         Length of buffer
 *
 * @return            Number of bytes read from IPC
 */
uint16_t ipc_nrf5340_read(int channel, void *buf, uint16_t len);

/**
 * Reads data from IPC ring buffer to specified mbuf.
 *
 * Note: this function does not use any kind of locking so it's up to caller
 *       to make sure it's not used during write from other context as it may
 *       yield incorrect results.
 *
 * @param channel     IPC channel number to read from
 * @param om          mbuf to read data to
 * @param len         Length of buffer
 *
 * @return            Number of bytes read from IPC.
 */
uint16_t ipc_nrf5340_read_om(int channel, struct os_mbuf *om, uint16_t len);

/**
 * Returns number of data bytes available in IPC ring buffer.
 *
 * Note: use ipc_nrf5340_data_available_get() instead
 *
 * Note: this function does not use any kind of locking so it's up to caller
 *       to make sure it's not used during write from other context as it may
 *       yield incorrect results.
 *
 * @param channel     IPC channel number
 *
 * @return            Number of bytes available in IPC ring buffer
 */
uint16_t ipc_nrf5340_available(int channel);

/**
 * Returns number of continuous data bytes available in IPC ring buffer with
 * pointer to that data.
 *
 * Note: this function does not use any kind of locking so it's up to caller
 *       to make sure it's not used during write from other context as it may
 *       yield incorrect results.
 *
 *
 * @param channel     IPC channel number
 * @param dptr        Pointer to data buffer
 *
 * @return            Number of bytes available in IPC ring buffer
 */
uint16_t ipc_nrf5340_available_buf(int channel, void **dptr);

/**
 * Returns number of data bytes available in IPC ring buffer for reading.
 *
 * Note: this function does not use any kind of locking so it's up to caller
 *       to make sure it's not used during write from other context as it may
 *       yield incorrect results.
 *
 * @param channel     IPC channel number
 *
 * @return            Number of bytes available in IPC ring buffer
 */
uint16_t ipc_nrf5340_data_available_get(int channel);

/**
 * Returns number of free data bytes available in IPC ring buffer for writing.
 *
 * Note: this function does not use any kind of locking so it's up to caller
 *       to make sure it's not used during write from other context as it may
 *       yield incorrect results.
 *
 * @param channel     IPC channel number
 *
 * @return            Number of bytes available in IPC ring buffer
 */
uint16_t ipc_nrf5340_data_free_get(int channel);

/**
 * Consumes data from IPC ring buffer without copying.
 *
 * Note: this function does not use any kind of locking so it's up to caller
 *       to make sure it's not used during write from other context as it may
 *       yield incorrect results.
 *
 * @param channel     IPC channel number to consume from
 * @param len         Number of bytes to consume
 *
 * @return            Number of bytes consumed from IPC.
 */
uint16_t ipc_nrf5340_consume(int channel, uint16_t len);

/**
 * Set function to be called when net core restarts.
 *
 * Note: Function will be called from IPC interrupt context.
 *
 * @param on_crash - function to be called
 */
void ipc_nrf5340_set_net_core_restart_cb(void (*on_restart)(void));

#if MYNEWT_VAL(MCU_NET_CORE)
/**
 * Get embedded netcore image and its size.
 *
 * @param size        Size of embedded image in bytes.
 *
 * @return            Pointer to image, NULL if not present
 */
const void *ipc_nrf5340_net_image_get(uint32_t *size);
#endif


#if MYNEWT_PKG_apache_mynewt_nimble__nimble_transport_common_hci_ipc
volatile struct hci_ipc_shm *ipc_nrf5340_hci_shm_get(void);
#endif

struct shm_memory_region {
    uint32_t region_id;
    void *region_start;
    uint32_t region_size;
};

#if MYNEWT_VAL(MCU_APP_CORE)

#define __REGION_ID(id) shm_region_ ## id
/**
 * Macro for shared memory region declaration
 *
 * It should be used on application core to specify memory region that should be accessible
 * on the network core.
 * example declaration form application core:
 * \code
 * struct application_shared_data {
 *   int anything;
 *   uint8_t buffer[1234]
 * } shared_data;
 *
 * #define MY_REGION_ID 112233
 * SHM_REGION(MY_REGION_ID, &shared_data, sizeof(shared_data));
 * \endcode
 *
 * @param id number that will be used on netcore to locate this region by
 * @param address start of shared memory region
 * @param size size of shared memory region
 */
#define SHM_REGION(id, address, size) \
    static struct shm_memory_region __attribute__((section(".shm_descriptor"), used)) __REGION_ID(id) = {     \
        .region_id = id,                                                                          \
        .region_start = address,                                                                  \
        .region_size = size,                                                                      \
    }

#else
/**
 * Find shared memory region by it's ID.
 *
 * Region should be declared on application core with SHM_REGION macro.
 * example declaration form application core:
 * \code
 * struct application_shared_data {
 *   int anything;
 *   uint8_t buffer[1234]
 * } shared_data;
 *
 * SHM_REGION(112233, &shared_data, sizeof(shared_data));
 * \endcode
 * access on netcode:
 * \code
 * ...
 * const struct shm_memory_region *shared_region;
 * region = ipc_nrf5340_find_region(122233);
 * if (region) {
 *   struct application_shared_data *shared_data = region->region_start;
 *   shared_data->anything = 1;
 *   ...
 * }
 * \endcode
 * @param region_id   Region ID to find.
 *
 * @return            Pointer to region, NULL if not present
 */
const struct shm_memory_region *ipc_nrf5340_find_region(uint32_t region_id);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _HW_DRIVERS_IPC_NRF5340_H */
