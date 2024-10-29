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
#include <stdbool.h>
#include <stdint.h>
#include "mcu/cmac_hal.h"
#include "mcu/cmac_timer.h"
#include "mcu/mcu.h"
#include <ipc_cmac/shm.h>
#include "os/os.h"
#include "CMAC.h"

#define COMP_TICK_HAS_PASSED(_num) \
    (CMAC->CM_EV_LATCHED_REG & \
     (CMAC_CM_EV_LATCHED_REG_EV1C_CLK_1US_X1_Msk << ((_num) - 1)))

struct cmac_timer_slp {
    uint32_t freq;
#if !MYNEWT_VAL(MCU_SLP_TIMER_32K_ONLY)
    uint32_t conv;
    uint32_t tick_ns;
#endif
};

static struct cmac_timer_slp g_cmac_timer_slp;

static cmac_timer_int_func_t *cmac_timer_int_hal_timer;
static cmac_timer_int_func_t *cmac_timer_int_hal_os_tick;

volatile uint32_t cm_ll_int_stat_reg;

void
LL_TIMER2LLC_IRQHandler(void)
{
    uint32_t int_stat;

    /* Clear interrupt now since callback may set comparators again */
    int_stat = CMAC->CM_LL_INT_STAT_REG | cm_ll_int_stat_reg;
    CMAC->CM_LL_INT_STAT_REG = int_stat;
    CMAC->CM_EXC_STAT_REG = CMAC_CM_EXC_STAT_REG_EXC_LL_TIMER2LLC_Msk;
    cm_ll_int_stat_reg = 0;

    if (int_stat & CMAC_CM_LL_INT_STAT_REG_LL_TIMER1_EQ_X_SEL_Msk) {
        cmac_timer_int_hal_timer();
    }

    if (int_stat & CMAC_CM_LL_INT_STAT_REG_LL_TIMER1_EQ_Y_SEL_Msk) {
        cmac_timer_int_hal_os_tick();
    }
}

static inline void
lpclk_wait_pos(void)
{
    uint32_t reg;

    reg = CMAC->CM_CTRL2_REG;
    CMAC->CM_CTRL2_REG = reg | CMAC_CM_CTRL2_REG_RXEV_ON_LPCLK_POSEDGE_Msk;

    __SEV();
    __WFE();
    __WFE();

    CMAC->CM_CTRL2_REG = reg;
}

static inline void
lpclk_wait_neg(void)
{
    uint32_t reg;

    reg = CMAC->CM_CTRL2_REG;
    CMAC->CM_CTRL2_REG = reg | CMAC_CM_CTRL2_REG_RXEV_ON_LPCLK_NEGEDGE_Msk;

    __SEV();
    __WFE();
    __WFE();

    CMAC->CM_CTRL2_REG = reg;
}

static void
slp_switch_to_lpclk(void)
{
    uint32_t reg;

    reg = CMAC_TIMER_SLP->CM_SLP_CTRL_REG;
    reg |= CMAC_TIMER_SLP_CM_SLP_CTRL_REG_TCLK_FROM_LPCLK_Msk;
    reg &= ~CMAC_TIMER_SLP_CM_SLP_CTRL_REG_TCLK_FROM_PCLK_Msk;

    lpclk_wait_neg();

    CMAC_TIMER_SLP->CM_SLP_CTRL_REG = reg;
}

static void
slp_switch_to_pclk(void)
{
    uint32_t reg;

    reg = CMAC_TIMER_SLP->CM_SLP_CTRL_REG;
    reg |= CMAC_TIMER_SLP_CM_SLP_CTRL_REG_TCLK_FROM_PCLK_Msk;
    reg &= ~CMAC_TIMER_SLP_CM_SLP_CTRL_REG_TCLK_FROM_LPCLK_Msk;

    lpclk_wait_neg();

    CMAC_TIMER_SLP->CM_SLP_CTRL_REG = reg;
}

static int32_t
slp_read(void)
{
    assert(CMAC_TIMER_SLP->CM_SLP_CTRL_REG & CMAC_TIMER_SLP_CM_SLP_CTRL_REG_TCLK_FROM_PCLK_Msk);

    return (int32_t)CMAC_TIMER_SLP->CM_SLP_TIMER_REG;
}

static void
slp_write(uint32_t val)
{
    assert(!(val & 0x80000000));
    assert(CMAC_TIMER_SLP->CM_SLP_CTRL_REG & CMAC_TIMER_SLP_CM_SLP_CTRL_REG_TCLK_FROM_PCLK_Msk);

    CMAC_TIMER_SLP->CM_SLP_TIMER_REG = val;
}

static void
switch_to_slp(void)
{
    slp_switch_to_lpclk();

    /* We are already synchronized with neg edge after switch to lpclk */
    CMAC_TIMER_SLP->CM_SLP_CTRL_REG |= CMAC_TIMER_SLP_CM_SLP_CTRL_REG_SLP_TIMER_SW_Msk;

    lpclk_wait_pos();
    while (!(CMAC_TIMER_SLP->CM_SLP_CTRL_REG & CMAC_TIMER_SLP_CM_SLP_CTRL_REG_SLP_TIMER_ACTIVE_Msk));
}

static void
switch_to_llt(void)
{
    lpclk_wait_neg();
    CMAC_TIMER_SLP->CM_SLP_CTRL_REG &= ~CMAC_TIMER_SLP_CM_SLP_CTRL_REG_SLP_TIMER_SW_Msk;

    lpclk_wait_pos();
    while (CMAC_TIMER_SLP->CM_SLP_CTRL_REG & CMAC_TIMER_SLP_CM_SLP_CTRL_REG_SLP_TIMER_ACTIVE_Msk);

    slp_switch_to_pclk();
}

static void
compensate_1mhz_clock(uint64_t slept_ns)
{
    uint32_t slept_ns_rem;
    uint32_t clk_freq_mhz_d2m1;
    uint32_t clk_freq_mhz;
    uint32_t comp_timer;
    uint32_t comp_1mhz;
    uint32_t comp_2mhz;

    slept_ns_rem = slept_ns % 1000;

    clk_freq_mhz_d2m1 = (CMAC->CM_CTRL_REG &
                         CMAC_CM_CTRL_REG_CM_CLK_FREQ_MHZ_D2M1_Msk) >>
                        CMAC_CM_CTRL_REG_CM_CLK_FREQ_MHZ_D2M1_Pos;
    clk_freq_mhz = 2 * (clk_freq_mhz_d2m1 + 1);
    comp_timer = slept_ns_rem * clk_freq_mhz / 1000;

    if (comp_timer > clk_freq_mhz_d2m1) {
        comp_1mhz = 1;
        comp_2mhz = comp_timer - clk_freq_mhz_d2m1 - 1;
    } else {
        comp_1mhz = 0;
        comp_2mhz = comp_timer;
    }

    CMAC->CM_CLK_COMP_REG = (comp_1mhz << CMAC_CM_CLK_COMP_REG_CLK1MHZ_COMP_Pos) |
                            (comp_2mhz << CMAC_CM_CLK_COMP_REG_CLK2MHZ_COMP_Pos);
}

static void
compensate_ll_timer(uint32_t slept_us)
{
    uint32_t comp_ll_timer_36;
    uint32_t comp_ll_timer_09;
    uint32_t new_ll_timer_36;
    uint32_t new_ll_timer_09;

    /*
     * Calculate compensation values. These values are applied 2 ticks after
     * reading timer value so adjust it here as well.
     */
    comp_ll_timer_36 = slept_us / 1024;
    comp_ll_timer_09 = slept_us % 1024;
    comp_ll_timer_09 += 2;

    /*
     * Compiler barrier to make sure calculations are already done prior to
     * this line since code below has strict time constraints.
     */
    asm volatile(""
                 :
                 :"r" (comp_ll_timer_36), "r" (comp_ll_timer_09)
                 : "memory");

    /*
     * Normally we should only wait for next 1MHz tick but since prior to
     * calling this function we run 1MHz clock compensation it may happen that
     * the very first tick will be shorter and we won't be able to read LL Timer
     * value during this tick. We just need to wait for next one to make sure
     * it's a proper one.
     */
    CMAC->CM_CTRL2_REG |= CMAC_CM_CTRL2_REG_RXEV_ON_1MHZ_Msk;
    __SEV();
    __WFE();
    __WFE();
    __WFE();
    CMAC->CM_EV_LATCHED_REG = 1;

    /*
     * Code below has strict time constraints: we use 2 ticks to read and then
     * calculate compensated value of LL Timer which we apply in 3rd tick. If
     * we fail to do any of these steps timely, LL Timer will be set incorrectly.
     */

    /* 1st tick - read current LL Timer value */
    new_ll_timer_36 = CMAC->CM_LL_TIMER1_36_10_REG;
    new_ll_timer_09 = CMAC->CM_LL_TIMER1_9_0_REG;
    __WFE();

    /* 2nd tick - calculate new LL Timer value */
    new_ll_timer_09 += comp_ll_timer_09;
    new_ll_timer_36 += comp_ll_timer_36 + new_ll_timer_09 / 1024;
    new_ll_timer_09 %= 1024;
    __WFE();

    /* 3rd tick - write compensated value */
    CMAC->CM_LL_TIMER1_9_0_REG = new_ll_timer_09;
    CMAC->CM_LL_TIMER1_36_10_REG = new_ll_timer_36;

#ifndef NDEBUG
    __WFE();
    assert(!COMP_TICK_HAS_PASSED(4));
    assert(COMP_TICK_HAS_PASSED(3));
#endif

    CMAC->CM_CTRL2_REG &= ~CMAC_CM_CTRL2_REG_RXEV_ON_1MHZ_Msk;
}

uint32_t
cmac_timer_get_hal_os_tick(void)
{
    return cmac_timer_read32_msb();
}

void
cmac_timer_init(void)
{
    /* Make sure LL Timer does not use limited range */
    assert(CMAC->CM_CTRL2_REG & CMAC_CM_CTRL2_REG_LL_TIMER1_9_0_LIMITED_N_Msk);

    /*
     * Set EQ_X and EQ_Y comparators to trigger LL_TIMER2LLC interrupt.
     * They are used for hal_timer and os_tick respectively.
     * Set EQ_Y to match on all 37 bits.
     */
    CMAC->CM_LL_INT_SEL_REG |= CMAC_CM_LL_INT_SEL_REG_LL_TIMER1_EQ_X_SEL_Msk |
                               CMAC_CM_LL_INT_SEL_REG_LL_TIMER1_EQ_Y_SEL_Msk;
    CMAC->CM_LL_TIMER1_EQ_Y_CTRL_REG = 0x7f;

    switch_to_llt();

    NVIC_DisableIRQ(LL_TIMER2LLC_IRQn);
    CMAC->CM_LL_INT_MSK_CLR_REG = UINT32_MAX;
    CMAC->CM_LL_INT_STAT_REG = UINT32_MAX;
    NVIC_ClearPendingIRQ(LL_TIMER2LLC_IRQn);
    NVIC_SetPriority(LL_TIMER2LLC_IRQn, 2);
    NVIC_EnableIRQ(LL_TIMER2LLC_IRQn);
}

void
cmac_timer_slp_enable(uint32_t ticks)
{
    slp_write(ticks);
    switch_to_slp();
}

void
cmac_timer_slp_disable(uint32_t exp_ticks)
{
    uint32_t slept_ticks;
    uint32_t slept_us;
    uint64_t slept_ns;

    assert(CMAC->CM_LL_INT_STAT_REG == 0);

    switch_to_llt();

    slept_ticks = exp_ticks - slp_read();

    /* XXX optimize this since Cortex-M0+ does not do integer divisions */
#if MYNEWT_VAL(MCU_SLP_TIMER_32K_ONLY)
    slept_ns = (uint64_t)slept_ticks * 30518;
#else
    slept_ns = (uint64_t)slept_ticks * g_cmac_timer_slp.tick_ns;
#endif
    slept_us = slept_ns / 1000;

    __disable_irq();
    compensate_1mhz_clock(slept_ns);
    compensate_ll_timer(slept_us);
    __enable_irq();

    CMAC_TIMER_SLP->CM_SLP_TIMER_REG = 0;
    CMAC_TIMER_SLP->CM_SLP_CTRL2_REG = CMAC_TIMER_SLP_CM_SLP_CTRL2_REG_SLP_TIMER_IRQ_CLR_Msk;

    assert(CMAC->CM_LL_INT_STAT_REG == 0);
}

void
cmac_timer_slp_update(uint16_t lp_clock_freq)
{
    if (lp_clock_freq == g_cmac_timer_slp.freq) {
        return;
    }

    g_cmac_timer_slp.freq = lp_clock_freq;

#if !MYNEWT_VAL(MCU_SLP_TIMER_32K_ONLY)
    if (g_cmac_timer_slp.freq) {
        g_cmac_timer_slp.conv = g_cmac_timer_slp.freq * 32768 / 1000000;
        g_cmac_timer_slp.tick_ns = 1000000000 / g_cmac_timer_slp.freq;
    }
#endif
}

bool
cmac_timer_slp_is_ready(void)
{
#if MYNEWT_VAL(MCU_SLP_TIMER_32K_ONLY)
    return g_cmac_timer_slp.freq == 32768;
#else
    return g_cmac_timer_slp.freq != 0;
#endif
}

#if !MYNEWT_VAL(MCU_SLP_TIMER_32K_ONLY)
uint32_t
cmac_timer_slp_tick_us(void)
{
    /* Better round tick length up so we wake up earlier than too late */
    return g_cmac_timer_slp.tick_ns / 1000 + 1;
}
#endif

void
cmac_timer_int_hal_timer_register(cmac_timer_int_func_t func)
{
    assert(!cmac_timer_int_hal_timer);
    cmac_timer_int_hal_timer = func;
}

void
cmac_timer_int_os_tick_register(cmac_timer_int_func_t func)
{
    assert(!cmac_timer_int_hal_os_tick);
    cmac_timer_int_hal_os_tick = func;
}

void
cmac_timer_int_os_tick_clear(void)
{
    CMAC->CM_LL_INT_STAT_REG = CMAC_CM_LL_INT_STAT_REG_LL_TIMER1_EQ_Y_SEL_Msk;
}

uint32_t
cmac_timer_next_at(void)
{
    uint32_t mask;
    uint32_t val32;
    uint32_t reg32;
    uint32_t to_next;

    mask = CMAC->CM_LL_INT_MSK_SET_REG;

#if MYNEWT_VAL(MCU_SLP_TIMER_32K_ONLY)
    /* Max sleep time is 130s (see below) */
    to_next = 130000000;
#else
    to_next = 4000000;
#endif

    val32 = cmac_timer_read32();

    if (mask & CMAC_CM_LL_INT_MSK_SET_REG_LL_TIMER1_EQ_X_SEL_Msk) {
        reg32 = (CMAC->CM_LL_TIMER1_EQ_X_HI_REG << 10) |
                CMAC->CM_LL_TIMER1_EQ_X_LO_REG;
        to_next = min(to_next, reg32 - val32);
    }

    if (mask & CMAC_CM_LL_INT_MSK_SET_REG_LL_TIMER1_EQ_Y_SEL_Msk) {
        reg32 = (CMAC->CM_LL_TIMER1_EQ_Y_HI_REG << 10) |
                CMAC->CM_LL_TIMER1_EQ_Y_LO_REG;
        to_next = min(to_next, reg32 - val32);
    }

    /* XXX add handling if any other comparator is used */

    return val32 + to_next;
}

uint32_t
cmac_timer_usecs_to_lp_ticks(uint32_t usecs)
{
#if MYNEWT_VAL(MCU_SLP_TIMER_32K_ONLY)
    /*
     * To speed up calculations we use only 32 lsb of timer value and thus have
     * limited range for sleep time we can handle. To provide best accuracy
     * calculations are done with different precision depending on target sleep
     * time:
     * - for sleep time <4s result is off by no more than 2.8ms
     * - for sleep time <60s result is off by no more than 97.4ms
     * - for sleep time <130s result is off by no more than 2148.1ms
     * Calculated lp_ticks sleep time is always shorter or equal to exact value
     * so in worst case we'll wake up a bit too early and go to sleep once more.
     */
    if (usecs < 4000000) {
        return (usecs * 1073) >> 15;
    } else if (usecs < 60000000) {
        return (usecs * 67) >> 11;
    } else {
        return (usecs * 33) >> 10;
    }
#else
    return (usecs * g_cmac_timer_slp.conv) >> 15;
#endif
}
