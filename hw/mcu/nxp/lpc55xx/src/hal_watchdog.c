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

#include <hal/hal_watchdog.h>

#include <fsl_wwdt.h>

#if MYNEWT_VAL(WATCHDOG_INTERVAL) > 0
_Static_assert(((1ULL << 24) * 4 * 1000) / 1000000 >= MYNEWT_VAL(WATCHDOG_INTERVAL),
               "Watchdog interval out of range, decrease value WATCHDOG_INTERVAL in syscfg.yml");
#endif

int
hal_watchdog_init(uint32_t expire_msecs)
{
#ifndef WATCHDOG_STUB
    wwdt_config_t config;

    SYSCON->CLOCK_CTRL |= SYSCON_CLOCK_CTRL_FRO1MHZ_CLK_ENA_MASK;
    SYSCON->WDTCLKDIV &= ~SYSCON_WDTCLKDIV_HALT_MASK;

    WWDT_GetDefaultConfig(&config);
    config.clockFreq_Hz = 1000000;
    config.enableWatchdogReset = true;
    /* Convert timeout from milliseconds to watchdog timer ticks
     * (1 tick = 4 us due to fixed prescaler of 4) */
    config.timeoutValue = expire_msecs * 250;
    WWDT_Init(WWDT, &config);
#endif

    return 0;
}

void
hal_watchdog_enable(void)
{
#ifndef WATCHDOG_STUB
    WWDT_Enable(WWDT);
#endif
}

void
hal_watchdog_tickle(void)
{
#ifndef WATCHDOG_STUB
    WWDT_Refresh(WWDT);
#endif
}
