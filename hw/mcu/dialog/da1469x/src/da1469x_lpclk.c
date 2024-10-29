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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "syscfg/syscfg.h"
#include "mcu/mcu.h"
#include "mcu/da1469x_clock.h"
#include "mcu/da1469x_lpclk.h"
#include "hal/hal_system.h"
#include "hal/hal_timer.h"
#include "os/os_cputime.h"
#include "da1469x_priv.h"

bool g_mcu_lpclk_available;

static da1469x_lpclk_cb *g_da1469x_lpclk_cmac_cb;
static uint32_t g_lpclk_last_freq = 0;

#if MYNEWT_VAL_CHOICE(MCU_LPCLK_SOURCE, XTAL32K)
static void
da1469x_lpclk_settle_tmr_cb(void *arg)
{
    da1469x_clock_lp_xtal32k_switch();
    da1469x_lpclk_enabled();
}
#endif

static void
da1469x_lpclk_notify(void)
{
    uint32_t lp_curr_freq;

    lp_curr_freq = da1469x_clock_lp_freq_get();

    if (lp_curr_freq != g_lpclk_last_freq) {
        g_lpclk_last_freq = lp_curr_freq;

        if (g_da1469x_lpclk_cmac_cb && g_mcu_lpclk_available) {
            g_da1469x_lpclk_cmac_cb(lp_curr_freq);
        }

        da1469x_clock_lp_set_rtc_divs(lp_curr_freq);
        hal_os_tick_calc_params(lp_curr_freq);
    }
}

void
da1469x_lpclk_register_cmac_cb(da1469x_lpclk_cb *cb)
{
    g_da1469x_lpclk_cmac_cb = cb;
    da1469x_lpclk_notify();
}

void
da1469x_lpclk_enabled(void)
{
    g_mcu_lpclk_available = true;
    da1469x_lpclk_notify();
}

void
da1469x_lpclk_updated(void)
{
    da1469x_lpclk_notify();
}

void
da1469x_lpclk_rc_init(void)
{
#if MYNEWT_VAL_CHOICE(MCU_LPCLK_SOURCE, RCX)
    da1469x_clock_lp_rc32k_disable();
    da1469x_clock_lp_rcx_enable();
    da1469x_clock_lp_rcx_switch();
    da1469x_clock_lp_calibrate();
    da1469x_lpclk_enabled();
#elif MYNEWT_VAL_CHOICE(MCU_LPCLK_SOURCE, RC32K)
    da1469x_clock_lp_rcx_disable();
    da1469x_clock_lp_rc32k_enable();
    da1469x_clock_lp_rc32k_switch();
    da1469x_clock_lp_calibrate();
    da1469x_lpclk_enabled();
#elif MYNEWT_VAL_CHOICE(MCU_LPCLK_SOURCE, XTAL32K)
    /*
     * We cannot switch lp_clk to XTAL32K here since it needs some time to
     * settle, so we just disable RCX (we don't need it) and then we'll handle
     * switch to XTAL32K from sysinit since we need os_cputime for this.
     */
    da1469x_clock_lp_rcx_disable();
#endif
}

void
da1469x_lpclk_init(void)
{
#if MYNEWT_VAL_CHOICE(MCU_LPCLK_SOURCE, XTAL32K)
    static struct hal_timer lpclk_settle_tmr;
    da1469x_clock_lp_xtal32k_enable();
    os_cputime_timer_init(&lpclk_settle_tmr, da1469x_lpclk_settle_tmr_cb, NULL);
    os_cputime_timer_relative(&lpclk_settle_tmr,
                              MYNEWT_VAL(MCU_CLOCK_XTAL32K_SETTLE_TIME_MS) * 1000);
#endif
}
