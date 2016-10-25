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

#include "hal/hal_watchdog.h"
#include "bsp/cmsis_nvic.h"

#include <assert.h>

#include "app_util_platform.h"
#include "nrf.h"
#include "nrf_drv_common.h"
#include "nrf_wdt.h"

static void
nrf51_hal_wdt_default_handler(void)
{
    assert(0);
}

/**@brief WDT interrupt handler. */
static void
nrf51_wdt_irq_handler(void)
{
    if (nrf_wdt_int_enable_check(NRF_WDT_INT_TIMEOUT_MASK) == true) {
        nrf_wdt_event_clear(NRF_WDT_EVENT_TIMEOUT);
        nrf51_hal_wdt_default_handler();
    }
}

int
hal_watchdog_init(uint32_t expire_msecs)
{
    NVIC_SetVector(WDT_IRQn, (uint32_t) nrf51_wdt_irq_handler);

    nrf_wdt_behaviour_set(NRF_WDT_BEHAVIOUR_RUN_SLEEP);

    /* Program in timeout in msecs */
    nrf_wdt_reload_value_set((expire_msecs * 32768ULL) / 1000);
    nrf_drv_common_irq_enable(WDT_IRQn, APP_IRQ_PRIORITY_HIGH);
    nrf_wdt_reload_request_enable(NRF_WDT_RR0);

    return (0);
}

void
hal_watchdog_enable(void)
{
    nrf_wdt_int_enable(NRF_WDT_INT_TIMEOUT_MASK);
    nrf_wdt_task_trigger(NRF_WDT_TASK_START);
}

void
hal_watchdog_tickle(void)
{
    nrf_wdt_reload_request_set(NRF_WDT_RR0);
}

