/**
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
#include <assert.h>
#include "os/os.h"
#include "hal/hal_cputime.h"
#include "hal/hal_uart.h"

/* BLE */
#include "nimble/ble.h"
#include "controller/ble_ll.h"
#include "transport/uart/ble_hci_uart.h"

/* Nimble task priorities */
#define BLE_LL_TASK_PRI         (OS_TASK_PRI_HIGHEST)

/* Create a mbuf pool of BLE mbufs */
#define MBUF_NUM_MBUFS      (7)
#define MBUF_BUF_SIZE       OS_ALIGN(BLE_MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

/* Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN] = { 0 };

/* Our random address (in case we need it) */
uint8_t g_random_addr[BLE_DEV_ADDR_LEN] = { 0 };

#define HCI_MAX_BUFS        (5)

os_membuf_t default_mbuf_mpool_data[MBUF_MEMPOOL_SIZE];

struct os_mbuf_pool default_mbuf_pool;
struct os_mempool default_mbuf_mpool;

int
main(void)
{
    int rc;

    /* Initialize OS */
    os_init();

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(1000000);
    assert(rc == 0);

    rc = os_mempool_init(&default_mbuf_mpool, MBUF_NUM_MBUFS,
                         MBUF_MEMBLOCK_SIZE, default_mbuf_mpool_data,
                         "default_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&default_mbuf_pool, &default_mbuf_mpool,
                           MBUF_MEMBLOCK_SIZE, MBUF_NUM_MBUFS);
    assert(rc == 0);

    rc = os_msys_register(&default_mbuf_pool);
    assert(rc == 0);

    /* Initialize the BLE LL */
    rc = ble_ll_init(BLE_LL_TASK_PRI, MBUF_NUM_MBUFS, BLE_MBUF_PAYLOAD_SIZE);
    assert(rc == 0);

    rc = ble_hci_uart_init(&ble_hci_uart_cfg_dflt);
    assert(rc == 0);

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}
