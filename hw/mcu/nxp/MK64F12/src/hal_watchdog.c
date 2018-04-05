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
#include "mcu/cmsis_nvic.h"

#include <assert.h>

#include "fsl_wdog.h"
#include "fsl_rcm.h"

/* #define WATCHDOG_STUB */

#ifndef WATCHDOG_STUB
static WDOG_Type *wdog_base = WDOG;

static void nxp_hal_wdt_default_handler(void)
{
    assert(0);
}

/**@brief WDT interrupt handler. */
static void nxp_wdt_irq_handler(void)
{
    if (WDOG_GetStatusFlags(wdog_base) && kWDOG_RunningFlag) {
        WDOG_ClearStatusFlags(wdog_base, kWDOG_TimeoutFlag);
        nxp_hal_wdt_default_handler();
    }
}
#endif

int hal_watchdog_init(uint32_t expire_msecs)
{
#ifndef WATCHDOG_STUB
    wdog_config_t config;

    NVIC_SetVector(WDOG_EWM_IRQn, (uint32_t) nxp_wdt_irq_handler);
    WDOG_GetDefaultConfig(&config);
    config.timeoutValue = (expire_msecs * 32768ULL) / 1000;
    config.enableUpdate = true;
    WDOG_Init(wdog_base, &config);
#endif

    return (0);
}

void hal_watchdog_enable(void)
{
#ifndef WATCHDOG_STUB
    WDOG_EnableInterrupts(wdog_base, kWDOG_InterruptEnable);
#endif
}

void hal_watchdog_tickle(void)
{
#ifndef WATCHDOG_STUB
    WDOG_Refresh(wdog_base);
#endif
}
