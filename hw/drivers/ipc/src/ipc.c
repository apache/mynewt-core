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
#define IPC_SYNC_TX_CHANNEL MYNEWT_VAL(IPC_SYNC_TX_CHANNEL)
#define IPC_SYNC_RX_CHANNEL MYNEWT_VAL(IPC_SYNC_RX_CHANNEL)

static void
ipc_cb(uint8_t channel)
{
    assert(channel == IPC_SYNC_RX_CHANNEL);

    os_trace_isr_enter();

    ipc_process_signal(IPC_SYNC_ID);

    os_trace_isr_exit();
}

int
ipc_signal(uint8_t channel)
{
    return hal_ipc_signal(channel);
}

void
ipc_init(void)
{
    uint32_t start;

    hal_ipc_init();

    hal_ipc_register_callback(IPC_SYNC_RX_CHANNEL, ipc_cb);

    ipc_open(IPC_SYNC_ID);

    hal_ipc_enable_irq(IPC_SYNC_RX_CHANNEL, 1);

    hal_ipc_start();

    ipc_signal(IPC_SYNC_TX_CHANNEL);
    start = os_cputime_ticks_to_usecs(os_cputime_get32());

    while (!ipc_ready(IPC_SYNC_ID)) {
        ipc_cb(IPC_SYNC_RX_CHANNEL);

        if ((os_cputime_ticks_to_usecs(os_cputime_get32()) - start) > 1000) {
            ipc_signal(IPC_SYNC_TX_CHANNEL);
            start = os_cputime_ticks_to_usecs(os_cputime_get32());
		}
    }
}
