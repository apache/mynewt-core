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
#include <os/mynewt.h>
#include <mcu/cmsis_nvic.h>
#include <mcu/nrf5340_net_hal.h>
#include <hal/hal_ipc.h>
#include <nrfx_ipc.h>

#ifndef BIT
#define BIT(n)  (1UL << (n))
#endif

#define IPC_MAX_CHANS 4

static hal_ipc_cb cbs[IPC_MAX_CHANS];

int
hal_ipc_signal(uint8_t channel)
{
    assert(channel < IPC_MAX_CHANS);

    nrfx_ipc_signal(channel);

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
    assert(channel < IPC_MAX_CHANS);

    if (enable) {
        NRF_IPC->RECEIVE_CNF[channel] = BIT(channel);
        NRF_IPC->INTENSET = BIT(channel);
    } else {
        NRF_IPC->INTENCLR = BIT(channel);
        NRF_IPC->RECEIVE_CNF[channel] = 0;
    }
}

static void
ipc_isr(void)
{
    uint32_t irq_pend;
    uint8_t channel;

    os_trace_isr_enter();

    /* Handle only interrupts that were enabled */
    irq_pend = NRF_IPC->INTPEND & NRF_IPC->INTEN;

    for (channel = 0; channel < IPC_MAX_CHANS; ++channel) {
        if (irq_pend & BIT(channel)) {
            NRF_IPC->EVENTS_RECEIVE[channel] = 0;

            if (cbs[channel] != NULL) {
                cbs[channel](channel);
            }
        }
    }

    os_trace_isr_exit();
}

void
hal_ipc_init(void)
{
    uint8_t i;

#if MYNEWT_VAL(MCU_APP_CORE)
    /* Make sure network core if off when we set up IPC */
    nrf_reset_network_force_off(NRF_RESET, true)

    if (MYNEWT_VAL(MCU_APP_SECURE) && !MYNEWT_VAL(IPC_NRF5340_PRE_TRUSTZONE_NETCORE_BOOT)) {
        /*
         * When bootloader is secure and application is not all peripherals are
         * in unsecure mode. This is done by bootloader.
         * If application runs in secure mode IPC manually chooses to use unsecure version
         * so net core can always use same peripheral.
         */
        NRF_SPU->PERIPHID[42].PERM &= ~SPU_PERIPHID_PERM_SECATTR_Msk;
    }
#endif

    /* Enable IPC channels */
    for (i = 0; i < IPC_MAX_CHANS; ++i) {
        NRF_IPC->SEND_CNF[i] = BIT(i);
        NRF_IPC->RECEIVE_CNF[i] = 0;
    }

    NRF_IPC->INTENCLR = 0xFFFF;
    NVIC_ClearPendingIRQ(IPC_IRQn);
    NVIC_SetVector(IPC_IRQn, (uint32_t)ipc_isr);
    NVIC_EnableIRQ(IPC_IRQn);
}

void
hal_ipc_start(void)
{
#if MYNEWT_VAL(MCU_APP_CORE)
    if (MYNEWT_VAL(MCU_APP_SECURE)) {
        /* this allows netcore to access appcore RAM */
        NRF_SPU_S->EXTDOMAIN[0].PERM = SPU_EXTDOMAIN_PERM_SECATTR_Secure << SPU_EXTDOMAIN_PERM_SECATTR_Pos;
    }

    /* Start Network Core */
    nrf_reset_network_force_off(NRF_RESET, false);

    /*
     * Wait for NET core to start and init it's side of IPC.
     * It may take several seconds if there is net core
     * embedded image in the application flash.
     */
#endif
}
