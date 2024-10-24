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

#include <errno.h>
#include <assert.h>
#include <os/os.h>
#include <ipc/ipc.h>
#include <hal/hal_ipc.h>

#define IPC_MAX_CHANS MYNEWT_VAL(IPC_CHANNELS)
#define IPC_SYNC_ID 0

/* IPC channels used for startup sync */
#if MYNEWT_VAL(MCU_APP_CORE)
#define IPC_TX_SYNC_CHANNEL 0
#define IPC_RX_SYNC_CHANNEL 1
#else
#define IPC_TX_SYNC_CHANNEL 1
#define IPC_RX_SYNC_CHANNEL 0
#endif

static void
ipc_cb(uint8_t channel)
{
    assert(channel == IPC_RX_SYNC_CHANNEL);

    os_trace_isr_enter();

    ipc_process_signal(IPC_SYNC_ID);

    os_trace_isr_exit();
}

static void
ipc_common_init(void)
{
    hal_ipc_init();
    hal_ipc_register_callback(IPC_RX_SYNC_CHANNEL, ipc_cb);
    ipc_open(IPC_SYNC_ID);
    hal_ipc_enable_irq(IPC_RX_SYNC_CHANNEL, 1);
    hal_ipc_start();
}

int
ipc_signal(uint8_t channel)
{
    return hal_ipc_signal(channel);
}

void
ipc_init(void)
{
    ipc_common_init();

    while (!ipc_ready(IPC_SYNC_ID)) {
#if MYNEWT_VAL(MCU_APP_CORE)
        ipc_signal(IPC_TX_SYNC_CHANNEL);
#endif
    }
}
