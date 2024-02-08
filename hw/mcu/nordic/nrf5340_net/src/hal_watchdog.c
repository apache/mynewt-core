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

#include <assert.h>
#include <os/mynewt.h>
#include <hal/hal_watchdog.h>
#include <mcu/cmsis_nvic.h>
#include <nrf.h>

static void
nrf5340_net_hal_wdt_default_handler(void)
{
    assert(0);
}

/**@brief WDT interrupt handler. */
static void
nrf5340_net_wdt_irq_handler(void)
{
    os_trace_isr_enter();
    if (NRF_WDT_NS->INTENSET & WDT_INTENSET_TIMEOUT_Msk) {
        NRF_WDT_NS->EVENTS_TIMEOUT = 0;
        nrf5340_net_hal_wdt_default_handler();
    }
    os_trace_isr_exit();
}

int
hal_watchdog_init(uint32_t expire_msecs)
{
    uint64_t expiration;

    NRF_WDT_NS->CONFIG = WDT_CONFIG_SLEEP_Msk;

    /* Convert msec timeout to counts of a 32768 crystal */
    expiration = ((uint64_t)expire_msecs * 32768) / 1000;
    NRF_WDT_NS->CRV = (uint32_t)expiration;

    NRF_WDT_NS->CRV = (expire_msecs * 32) + ((expire_msecs * 96) / 125);

    NVIC_SetVector(WDT_IRQn, (uint32_t)nrf5340_net_wdt_irq_handler);
    NVIC_SetPriority(WDT_IRQn, 0);
    NVIC_ClearPendingIRQ(WDT_IRQn);
    NVIC_EnableIRQ(WDT_IRQn);
    NRF_WDT_NS->RREN |= 0x1;

    return 0;
}

void
hal_watchdog_enable(void)
{
    NRF_WDT_NS->INTENSET = WDT_INTENSET_TIMEOUT_Msk;
    NRF_WDT_NS->TASKS_START = 1;
}

void
hal_watchdog_tickle(void)
{
    NRF_WDT_NS->RR[0] = WDT_RR_RR_Reload;
}

