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

#include "hal/hal_watchdog.h"
#include "wdt.h"

struct wdt_conf g_wdt_config;

int
hal_watchdog_init(uint32_t expire_msecs)
{
    uint32_t clocks;
    uint16_t temp;
    uint16_t to_period;
    enum status_code rc;
    struct system_gclk_gen_config gcfg;

    /* Get the defaults */
    wdt_get_config_defaults(&g_wdt_config);
    g_wdt_config.enable = false;

    /*
     * We are using the low-power oscillator for the WDT and we use clock
     * generator 4. This clock generator has a maximum of 8 divide bits. The
     * samd21 allows the divisor to be 2^divisor to increase the divisor range.
     * Since we want to allow for a long watchdog timeout, we want to divide
     * the 32.768 clock by a large amount, as the watchdog timeout can be a
     * maximum of 16384 clock cycles. We thus chose 2048 as a divisor. This
     * gives the watchdog timeout a range of 2048 seconds, or ~34 minutes.
     * If a large watchdog timeout period is requested, we return error.
     *
     * XXX: change this!
     * Another note: since we dont know if the oscillator is calibrated,
     * we multiply the number of clocks by 2.
     */
    clocks = 32768 / 2048;
    clocks = (clocks * expire_msecs) / 1000;
    clocks = clocks * 2;

    /* The watchdog count starts at 8 and goes up to 16384 max. */
    temp = 8;
    to_period = WDT_PERIOD_8CLK;
    while (1) {
        if (clocks <= temp) {
            break;
        }
        temp <<= 1;
        ++to_period;
        if (to_period > WDT_PERIOD_16384CLK) {
            return -1;
        }
    }
    g_wdt_config.timeout_period = to_period;

    gcfg.division_factor = 2048;
    gcfg.high_when_disabled = false;
    gcfg.output_enable = true;  /* XXX: why is this set true? */
    gcfg.run_in_standby = true;
    gcfg.source_clock = GCLK_SOURCE_OSCULP32K;
    system_gclk_gen_set_config(g_wdt_config.clock_source, &gcfg);
    system_gclk_gen_enable(g_wdt_config.clock_source);

    /* We init but dont turn on device */
    rc = wdt_set_config(&g_wdt_config);

    return ((int)rc);
}

void
hal_watchdog_enable(void)
{
    g_wdt_config.enable = true;
    wdt_set_config(&g_wdt_config);
}

void
hal_watchdog_tickle(void)
{
    wdt_reset_count();
}
