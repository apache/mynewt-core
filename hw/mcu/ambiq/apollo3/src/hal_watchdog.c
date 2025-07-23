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
#include "hal/hal_watchdog.h"
#include "hal/am_hal_wdt.h"

#if MYNEWT_VAL(WATCHDOG_INTERVAL) > 0
_Static_assert(255 * 1000 / MYNEWT_VAL(AM_WATCHDOG_CLOCK) >=
                   MYNEWT_VAL(WATCHDOG_INTERVAL),
               "Watchdog interval out of range, decrease value WATCHDOG_INTERVAL in syscfg.yml");
#endif

am_hal_wdt_config_t g_wdt_cfg;

int
hal_watchdog_init(uint32_t expire_msecs)
{
    uint32_t reload;

    reload = (expire_msecs * MYNEWT_VAL(AM_WATCHDOG_CLOCK)) / 1000;

    if (reload > 255) {
        return -1;
    }

    uint32_t wdt_clk;

#if MYNEWT_VAL(AM_WATCHDOG_CLOCK) == 1
    wdt_clk = AM_HAL_WDT_LFRC_CLK_1HZ;
#elif MYNEWT_VAL(AM_WATCHDOG_CLOCK) == 16
    wdt_clk = AM_HAL_WDT_LFRC_CLK_16HZ;
#elif MYNEWT_VAL(AM_WATCHDOG_CLOCK) == 128
    wdt_clk = AM_HAL_WDT_LFRC_CLK_128HZ;
#else
#error "Unsupported WDT clock frequency, set AM_WATCHDOG_CLOCK to 1, 16 or 128"
#endif

    g_wdt_cfg.ui32Config = wdt_clk | AM_HAL_WDT_ENABLE_RESET;
    g_wdt_cfg.ui16ResetCount = reload;
    g_wdt_cfg.ui16InterruptCount = 0;

    am_hal_wdt_init(&g_wdt_cfg);
    return 0;
}

void
hal_watchdog_enable(void)
{
    am_hal_wdt_start();
}

void
hal_watchdog_tickle(void)
{
    am_hal_wdt_restart();
}
