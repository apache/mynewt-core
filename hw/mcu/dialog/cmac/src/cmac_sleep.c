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

#include "syscfg/syscfg.h"
#if !MYNEWT_VAL(MCU_DEBUG_DSER_CMAC_SLEEP)
#define MCU_DIAG_SER_DISABLE
#endif

#include <assert.h>
#include "mcu/cmac_pdc.h"
#include "cmac_driver/cmac_shared.h"
#include "mcu/cmac_timer.h"
#include "mcu/mcu.h"
#include "hal/hal_system.h"
#include "controller/ble_phy.h"
#include "os/os.h"
#include "CMAC.h"
#include "cmac_priv.h"

/* Registers to retain during deep sleep */
static __IOM uint32_t * const retained_regs[] = {
    &CMAC->CM_PHY_CTRL_REG,
    &CMAC->CM_PHY_CTRL2_REG,
    &CMAC->CM_CTRL2_REG,
    &CMAC->CM_LL_INT_MSK_SET_REG,
    &CMAC->CM_LL_INT_SEL_REG,
    &CMAC->CM_LL_TIMER1_EQ_X_HI_REG,
    &CMAC->CM_LL_TIMER1_EQ_X_LO_REG,
    &CMAC->CM_LL_TIMER1_EQ_Y_HI_REG,
    &CMAC->CM_LL_TIMER1_EQ_Y_LO_REG,
    &CMAC->CM_LL_TIMER1_EQ_Y_CTRL_REG,
    &CMAC->CM_ERROR_DIS_REG,
#if MYNEWT_VAL(CMAC_DEBUG_DIAG_ENABLE)
    &CMAC->CM_DIAG_PORT0_REG,
    &CMAC->CM_DIAG_PORT1_REG,
    &CMAC->CM_DIAG_PORT2_REG,
    &CMAC->CM_DIAG_PORT3_REG,
    &CMAC->CM_DIAG_PORT4_REG,
    &CMAC->CM_DIAG_PORT5_REG,
    &CMAC->CM_DIAG_PORT6_REG,
    &CMAC->CM_DIAG_PORT7_REG,
    &CMAC->CM_DIAG_PORT8_REG,
    &CMAC->CM_DIAG_PORT9_REG,
    &CMAC->CM_DIAG_PORT10_REG,
    &CMAC->CM_DIAG_PORT11_REG,
    &CMAC->CM_DIAG_PORT12_REG,
    &CMAC->CM_DIAG_PORT13_REG,
    &CMAC->CM_DIAG_PORT14_REG,
    &CMAC->CM_DIAG_PORT15_REG,
#endif
};

static uint32_t g_retained_regs_val[ ARRAY_SIZE(retained_regs) ];

#define WAIT_FOR_SWD_INVALID (UINT32_MAX)
static uint32_t g_mcu_wait_for_swd_start;

/* Minimum time required to go to sleep (until switch to SLP) and then wake up */
static uint32_t g_mcu_wakeup_usecs_min;

static bool
cmac_sleep_is_switch_allowed(void)
{
    return (ble_phy_xcvr_state_get() == 0) &&
           !os_arch_cmac_pending_irq() &&
           cmac_timer_slp_is_ready();
}

static inline int
sub27(uint32_t x, uint32_t y)
{
    int result;

    result = x - y;
    if (result & 0x4000000) {
        return (result | 0xf8000000);
    } else {
        return (result & 0x07ffffff);
    }
}

static bool
cmac_sleep_is_deep_sleep_allowed(void)
{
    /*
     * We wait for SWD attach until high part of LL Timer increases by 2 which
     * is anywhere in 1-2ms range which is enough.
     */
    if ((g_mcu_wait_for_swd_start != WAIT_FOR_SWD_INVALID) &&
        sub27(cmac_timer_read_hi(), g_mcu_wait_for_swd_start) >= 2) {
        g_mcu_wait_for_swd_start = WAIT_FOR_SWD_INVALID;
    }

    return (g_mcu_wait_for_swd_start == WAIT_FOR_SWD_INVALID) &&
           !hal_debugger_connected();
}

static void
cmac_sleep_regs_save(void)
{
    __asm__ volatile (".syntax unified              \n"
                      "1: subs %[idx], %[idx], #4   \n"
                      "   ldr  r3, [%[reg], %[idx]] \n"
                      "   ldr  r4, [r3]             \n"
                      "   str  r4, [%[val], %[idx]] \n"
                      "   bne  1b                   \n"
                      :
                      : [reg] "l" (retained_regs),
                        [val] "l" (g_retained_regs_val),
                        [idx] "l" (sizeof(retained_regs))
                      : "r3", "r4", "memory");
}

static void
cmac_sleep_regs_restore(void)
{
    __asm__ volatile (".syntax unified              \n"
                      "1: subs %[idx], %[idx], #4   \n"
                      "   ldr  r3, [%[reg], %[idx]] \n"
                      "   ldr  r4, [%[val], %[idx]] \n"
                      "   str  r4, [r3]             \n"
                      "   bne  1b                   \n"
                      :
                      : [reg] "l" (retained_regs),
                        [val] "l" (g_retained_regs_val),
                        [idx] "l" (sizeof(retained_regs))
                      : "r3", "r4", "memory");
}

static void
cmac_sleep_enable_dcdc(void)
{
    if (!g_cmac_shared_data.dcdc.enabled) {
        return;
    }

    *(volatile uint32_t *)0x50000314 = g_cmac_shared_data.dcdc.v18;
    *(volatile uint32_t *)0x50000318 = g_cmac_shared_data.dcdc.v18p;
    *(volatile uint32_t *)0x50000310 = g_cmac_shared_data.dcdc.vdd;
    *(volatile uint32_t *)0x5000030c = g_cmac_shared_data.dcdc.v14;
    *(volatile uint32_t *)0x50000304 = g_cmac_shared_data.dcdc.ctrl1;
}

static void
cmac_sleep_wait4xtal(void)
{
    if (*(volatile uint32_t *)0x50000014 & 0x4000) {
        return;
    }

    while (*(volatile uint32_t *)0x5001001c & 0x0000ff00);
    *(volatile uint32_t *)0x5000001c = 1;
}

#define T_USEC(_t)          (_t)
#define T_LPTICK(_t)        ((_t) * cmac_timer_slp_tick_us())
#define T_LPTICK_U(_t)      (T_LPTICK(_t) * 15 / 10)

static void
cmac_sleep_calculate_wakeup_time(void)
{
    assert(g_cmac_shared_data.xtal32m_settle_us);

    g_mcu_wakeup_usecs_min =
        /*
         * We need ~12us to prepare for sleep before starting switch to SLP.
         * Switch to SLP is done by switching SLP clock to LPCLK first and then
         * enabling SLP. The former has to be synchronized with negative edge of
         * LPCLK and the latter happens on positive edge of LPCLK so we just
         * assume 2 LPCLK ticks in worst case.
         */
        T_USEC(12) + T_LPTICK(2) +
        /*
         * On wake up we assume fast wake up mode which has 3 phases that take
         * up to 2, 2 and 3 LPCLK ticks respectively (need to add some margin
         * here for worst-worst case). XTAL32M is started at 3rd phase and we
         * need to wait for it to settle before switch back to LLT. This is done
         * by disabling SLP and then switching SLP clock to PCLK. Both actions
         * are synchronized with LPCLK negative edge so take 2 LPCLK ticks in
         * worst case. Finally, LLP compensation takes around 50us.
         */
        T_LPTICK_U(2) + T_LPTICK_U(2) +
        max(T_LPTICK_U(3), T_USEC(g_cmac_shared_data.xtal32m_settle_us)) +
        T_LPTICK(2) + T_USEC(50) +

        /*
         * We are adding 250 usecs as a safeguard as we know the above
         * calculations are not correct and need modification. This is
         * to prevent EQ_X comparator from firing off and causing an
         * assert when we compensate the LL timer
         */
        T_USEC(250);
}

void
cmac_sleep_recalculate(void)
{
    if (cmac_timer_slp_update()) {
        cmac_sleep_calculate_wakeup_time();
    }
}

extern bool ble_rf_try_recalibrate(uint32_t idle_time_us);

void
cmac_sleep(void)
{
    bool switch_to_slp;
    bool deep_sleep;
    uint32_t wakeup_at;
    uint32_t sleep_usecs;
    uint32_t sleep_lp_ticks;
    int32_t wakeup_diff;

    MCU_DIAG_SER('<');

    switch_to_slp = MYNEWT_VAL(MCU_SLP_TIMER);
    deep_sleep = MYNEWT_VAL(MCU_DEEP_SLEEP);

    cmac_pdc_ack_all();

    wakeup_at = cmac_timer_next_at();

    /*
     * At this point in time we know exactly when next LLT interrupt should
     * happen so need to make sure we can be up and running on time.
     */

    sleep_usecs = wakeup_at - cmac_timer_read32() - g_mcu_wakeup_usecs_min;
    if ((int32_t)sleep_usecs <= 0) {
        switch_to_slp = false;
        deep_sleep = false;
        goto do_sleep;
    }

    if (ble_rf_try_recalibrate(sleep_usecs)) {
        goto skip_sleep;
    }

    sleep_lp_ticks = cmac_timer_usecs_to_lp_ticks(sleep_usecs);
    if (sleep_lp_ticks <= 1) {
        switch_to_slp = false;
        deep_sleep = false;
        goto do_sleep;
    }

    if (!cmac_sleep_is_switch_allowed()) {
        switch_to_slp = false;
        deep_sleep = false;
    } else if (!cmac_sleep_is_deep_sleep_allowed()) {
        deep_sleep = false;
    }

    if (deep_sleep) {
        MCU_DIAG_SER('R');
        cmac_sleep_regs_save();
    }

    if (switch_to_slp) {
        MCU_DIAG_SER('T');
        cmac_timer_slp_enable(sleep_lp_ticks);
    }

do_sleep:
    if (deep_sleep) {
        MCU_DIAG_SER('S');
        deep_sleep = os_arch_cmac_deep_sleep();
        if (deep_sleep) {
            cmac_sleep_regs_restore();
        } else {
            /* We did not enter deep sleep, no need to restore registers */
            MCU_DIAG_SER('X');
        }
    } else {
        MCU_DIAG_SER('s');
        os_arch_cmac_wfi();
    }

    if (deep_sleep) {
        cmac_sleep_enable_dcdc();
        cmac_sleep_wait4xtal();
    }

    if (switch_to_slp) {
        cmac_timer_slp_disable(sleep_lp_ticks);

        /*
         * XXX
         * This should not be really necessary if all calculations are correct
         * and timings are as in spec, however for some reason (rounding?) when
         * running on RCX we occasionally are few usecs past expected time here.
         * This means LLT comparator did not match most likely and no interrupt
         * was triggered thus anything scheduled at this wakeup is broken. So as
         * a last resort, let's just trigger LLT interrupt manually. Note that
         * it's ok that only LLT for HAL is triggered since os_tick is handled
         * anyway when leaving idle.
         */
        wakeup_diff = (int32_t)(wakeup_at - cmac_timer_read32());
        if (wakeup_diff <= 0) {
            cmac_timer_trigger_hal();
        }
    }

    if (CMAC_TIMER_SLP->CM_SLP_CTRL2_REG & CMAC_TIMER_SLP_CM_SLP_CTRL2_REG_CMAC_WAKEUP_ON_SWD_STATE_Msk) {
        CMAC_TIMER_SLP->CM_SLP_CTRL2_REG = CMAC_TIMER_SLP_CM_SLP_CTRL2_REG_CMAC_WAKEUP_ON_SWD_STATE_Msk;
        g_mcu_wait_for_swd_start = cmac_timer_read_hi();
    }

skip_sleep:
    MCU_DIAG_SER('>');
}
