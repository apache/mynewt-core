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
#include <os/os.h>
#include <ipc_icbmsg/ipc_icbmsg.h>
#include <nrfx.h>
#include <nrfx_ipc.h>

#define IPC_MAX_CHANS MYNEWT_VAL(IPC_ICBMSG_CHANNELS)
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
ipc_nrf_isr(void)
{
    uint32_t irq_pend;
    uint8_t i;

    os_trace_isr_enter();

    /* Handle only interrupts that were enabled */
    irq_pend = NRF_IPC->INTPEND & NRF_IPC->INTEN;

    for (i = 0; i < IPC_MAX_CHANS; ++i) {
        if (irq_pend & (0x1UL << i)) {
            NRF_IPC->EVENTS_RECEIVE[i] = 0;

            ipc_icbmsg_process_icmsg(i);
        }
    }

    os_trace_isr_exit();
}

void
ipc_icbmsg_signal(uint8_t channel)
{
    nrfx_ipc_signal(channel);
}

static void
ipc_nrf_init_ipc(void)
{
    int i;

    /* Enable IPC channels */
    for (i = 0; i < IPC_MAX_CHANS; ++i) {
        NRF_IPC->SEND_CNF[i] = (0x01UL << i);
        NRF_IPC->RECEIVE_CNF[i] = 0;
    }

    NRF_IPC->INTENCLR = 0xFFFF;
    NVIC_ClearPendingIRQ(IPC_IRQn);
    NVIC_SetVector(IPC_IRQn, (uint32_t)ipc_nrf_isr);
    NVIC_EnableIRQ(IPC_IRQn);

    ipc_icbmsg_open(IPC_SYNC_ID);
    ipc_icbmsg_enable_ipc_irq(IPC_RX_SYNC_CHANNEL, 1);
}

#if MYNEWT_VAL(MCU_APP_CORE)
void
ipc_icbmsg_init(void)
{
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

    ipc_nrf_init_ipc();

    if (MYNEWT_VAL(MCU_APP_SECURE)) {
        /* this allows netcore to access appcore RAM */
        NRF_SPU_S->EXTDOMAIN[0].PERM = SPU_EXTDOMAIN_PERM_SECATTR_Secure << SPU_EXTDOMAIN_PERM_SECATTR_Pos;
    }
}

void
ipc_icbmsg_netcore_init(void)
{
    /* Start Network Core */
    nrf_reset_network_force_off(NRF_RESET, false);

    /*
     * Waits for NET core to start and init it's side of IPC.
     * It may take several seconds if there is net core
     * embedded image in the application flash.
     */

    while (!ipc_icsmsg_ready(IPC_SYNC_ID)) {
        ipc_icbmsg_signal(IPC_TX_SYNC_CHANNEL);
    }
}
#endif

#if MYNEWT_VAL(MCU_NET_CORE)
void
ipc_icbmsg_init(void)
{
    ipc_nrf_init_ipc();
}

void
ipc_icbmsg_netcore_init(void)
{
    while (!ipc_icsmsg_ready(IPC_SYNC_ID));
}
#endif

#if MYNEWT_VAL(MCU_APP_CORE)
void
ipc_icbmsg_reset(void)
{
    /* Make sure network core if off when we reset IPC */
    nrf_reset_network_force_off(NRF_RESET, true)

    /* Start Network Core */
    ipc_icbmsg_netcore_init();
}
#endif

void
ipc_icbmsg_enable_ipc_irq(uint8_t channel, uint8_t on)
{
    assert(channel < IPC_MAX_CHANS);

    if (on) {
        NRF_IPC->RECEIVE_CNF[channel] = (0x1UL << channel);
        NRF_IPC->INTENSET = (0x1UL << channel);
    } else {
        NRF_IPC->INTENCLR = (0x1UL << channel);
        NRF_IPC->RECEIVE_CNF[channel] = 0;
    }
}
