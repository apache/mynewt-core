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
#include "mcu/da1469x_clock.h"
#include "mcu/da1469x_pdc.h"
#include "mcu/da1469x_prail.h"
#include "mcu/mcu.h"
#include "hal/hal_system.h"
#include "DA1469xAB.h"
#include "da1469x_priv.h"

#if MYNEWT_VAL(MCU_DEEP_SLEEP)

extern int da1469x_m33_sleep(void) __attribute__((naked));

uint8_t g_mcu_pdc_combo_idx;

static bool g_mcu_wait_for_jtag;
static os_time_t g_mcu_wait_for_jtag_until;

static inline bool
da1469x_sleep_any_irq_pending(void)
{
    return ((NVIC->ISPR[0] & NVIC->ISER[0]) | (NVIC->ISPR[1] & NVIC->ISER[1]));
}

static bool
da1469x_sleep_is_blocked(void)
{
    if (g_mcu_wait_for_jtag &&
        OS_TIME_TICK_GEQ(os_time_get(), g_mcu_wait_for_jtag_until)) {
        g_mcu_wait_for_jtag = false;
    }

    return hal_debugger_connected() || da1469x_sleep_any_irq_pending() ||
           !g_mcu_lpclk_available || g_mcu_wait_for_jtag;
}

void
da1469x_sleep(os_time_t ticks)
{
    int ret;

    da1469x_pdc_ack_all_m33();

    if (da1469x_sleep_is_blocked() || ticks < 3) {
        __DSB();
        __WFI();
        return;
    }

    mcu_gpio_enter_sleep();
    ret = da1469x_m33_sleep();
    mcu_gpio_exit_sleep();
    if (!ret) {
        /* We were not sleeping, just return */
        return;
    }

#if MYNEWT_VAL(MCU_DCDC_ENABLE)
    da1469x_prail_dcdc_restore();
#endif

    /*
     * If PDC entry for "combo" wakeup is pending, but none of CMAC2SYS, WKUP
     * or VBUS is pending it means we were woken up from JTAG. We need to block
     * deep sleep for a moment so debugger can attach.
     */
    if (da1469x_pdc_is_pending(g_mcu_pdc_combo_idx) &&
        !(NVIC->ISPR[0] & ((1 << CMAC2SYS_IRQn) |
                           (1 << KEY_WKUP_GPIO_IRQn) |
                           (1 << VBUS_IRQn)))) {
        g_mcu_wait_for_jtag = true;
        g_mcu_wait_for_jtag_until = os_time_get() + os_time_ms_to_ticks32(100);
    }

    /* XXX for now we always run on XTAL32M and assume PDC was configure to enable it */
    da1469x_clock_sys_xtal32m_switch_safe();
}

#else

void
da1469x_sleep(os_time_t ticks)
{
    __DSB();
    __WFI();
}

#endif
