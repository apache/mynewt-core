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

// #include <string.h>
#include <errno.h>
#include <assert.h>
#include <os/mynewt.h>
#include <mcu/cmsis_nvic.h>
#include <mcu/nrf54h20_hal.h>
#include <hal/hal_ipc.h>
#include <hal/nrf_bellboard.h>

#ifndef BIT
#define BIT(n)  (1UL << (n))
#endif

#define IPC_MAX_CHANS BELLBOARD_EVENTS_TRIGGERED_MaxCount
#define BELLBOARD_IPC0_IRQ BELLBOARD_0_IRQn
#define BELLBOARD_NUM_IRQS 4U
#if MYNEWT_VAL(MCU_APP_CORE)
#define BELLBOARD_LOCAL NRF_APPLICATION_BELLBOARD_S
#define BELLBOARD_REMOTE NRF_RADIOCORE_BELLBOARD_S
#else
#define BELLBOARD_LOCAL NRF_RADIOCORE_BELLBOARD_S
#define BELLBOARD_REMOTE NRF_APPLICATION_BELLBOARD_S
#endif

/* ipc0: 0: cpurad-cpusec, 6: cpurad-cpusys, 12: cpurad-cpuapp */
#define BELLBOARD_IPC0_EVENTS_MAP (0x00001041)

static const uint32_t evt_mappings[BELLBOARD_NUM_IRQS] = {
        BELLBOARD_IPC0_EVENTS_MAP, 0, 0, 0
};

static hal_ipc_cb cbs[IPC_MAX_CHANS];

int
hal_ipc_signal(uint8_t channel)
{
    assert(channel < IPC_MAX_CHANS);

    nrf_bellboard_task_trigger(BELLBOARD_REMOTE, nrf_bellboard_trigger_task_get(channel));

    return 0;
}

void
hal_ipc_register_callback(uint8_t channel, hal_ipc_cb cb)
{
    assert(channel < IPC_MAX_CHANS);

    cbs[channel] = cb;
}

void
hal_ipc_enable_irq(uint8_t channel, bool enable)
{
    uint8_t i;

    assert(channel < IPC_MAX_CHANS);

    for (i = 0U; i < BELLBOARD_NUM_IRQS; i++) {

        if ((evt_mappings[i] & BIT(channel)) == 0U) {
            continue;
        }

        if (enable) {
            nrf_bellboard_int_enable(BELLBOARD_LOCAL, i, BIT(channel));
        } else {
            nrf_bellboard_int_disable(BELLBOARD_LOCAL, i, BIT(channel));
        }
    }
}

static void
ipc0_isr(void)
{
    uint32_t int_pend;
    nrf_bellboard_event_t event;
    uint8_t channel;

    int_pend = nrf_bellboard_int_pending_get(BELLBOARD_LOCAL, 0);

    for (channel = 0; channel < IPC_MAX_CHANS; channel++) {
        event = nrf_bellboard_triggered_event_get(channel);

        if ((int_pend & BIT(channel)) != 0U) {
            if (nrf_bellboard_event_check(BELLBOARD_LOCAL, event)) {
                nrf_bellboard_event_clear(BELLBOARD_LOCAL, event);
            }

            if (cbs[channel] != NULL) {
                cbs[channel](channel);
            }
        }
    }
}

void
hal_ipc_init(void)
{
    uint32_t evt_all_mappings = evt_mappings[0] | evt_mappings[1] |
                                evt_mappings[2] | evt_mappings[3];
    uint8_t i = 0U;

    for (i = 0U; i < BELLBOARD_NUM_IRQS; ++i) {
        nrf_bellboard_int_disable(BELLBOARD_LOCAL, i, evt_mappings[i]);
    }

    for (i = 0U; i < IPC_MAX_CHANS; ++i) {
        if ((evt_all_mappings & BIT(i)) != 0U) {
            nrf_bellboard_event_clear(BELLBOARD_LOCAL, nrf_bellboard_triggered_event_get(i));
        }
    }

    NVIC_SetPriority(BELLBOARD_IPC0_IRQ, 1);
    NVIC_SetVector(BELLBOARD_IPC0_IRQ, (uint32_t)ipc0_isr);
    NVIC_EnableIRQ(BELLBOARD_IPC0_IRQ);
}

void
hal_ipc_start(void)
{

}
