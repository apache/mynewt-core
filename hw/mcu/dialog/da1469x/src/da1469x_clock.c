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
#include "syscfg/syscfg.h"
#include "mcu/cmsis_nvic.h"
#include "mcu/da1469x_hal.h"
#include "mcu/da1469x_pd.h"
#include "mcu/da1469x_pdc.h"
#include "mcu/da1469x_clock.h"

#define XTAL32M_FREQ    32000000
#define RC32M_FREQ      32000000
#define RC32M_FREQ_MIN  30600000
#define RC32M_FREQ_MAX  32600000
#define DIVN_FREQ       32000000    /* For CLK_CTRL_REG::SYS_CLK_SEL != 2 (LP clock) */
#define CAL_REF_FREQ    DIVN_FREQ   /* For CLK_REF_SEL_REG::CAL_CLK_SEL = 0 (DivN) */
#define PLL_FREQ        96000000
#define XTAL32K_FREQ       32768
#define XTALRDY_IRQ_FREQ  256000    /* For XTALRDY_CTRL_REG::XTALRDY_CLK_SEL = 1 (256 kHz) */

#define XTALRDY_IRQ_DIV         (RC32M_FREQ / XTALRDY_IRQ_FREQ)
#define XTALRDY_IRQ_FREQ_MAX    (RC32M_FREQ_MAX / XTALRDY_IRQ_DIV)

#define RTC_IN_FREQ_HZ       100
#define RTC_DIV_FRAC_ADJ      10    /* For CLK_RTCDIV_REG::RTC_DIV_DENOM = 0 (1000) */

enum da1469x_sys_clk_sel {
    DA1469X_SYS_XTAL32M = 0,
    DA1469X_SYS_RC32M,
    DA1469X_SYS_LP_CLK,
    DA1469X_SYS_PLL96M,
};

enum da1469x_xtalrdy_clk_sel {
    DA1469X_XTALRDY_CLK_32K = 0,
    DA1469X_XTALRDY_CLK_256K,
};
#define DA1469X_XTALRDY_CLK_SEL DA1469X_XTALRDY_CLK_256K

enum da1469x_lp_clk_sel {
    DA1469X_LP_RC32K = 0,
    DA1469X_LP_RCX,
    DA1469X_LP_XTAL32K,
    DA1469X_LP_EXT,
};

enum da1469x_ref_sel {
    DA1469X_REF_DIVN = 0,
    DA1469X_REF_RC32M,
    DA1469X_REF_RC32K,
    DA1469X_REF_XTAL32K,
    DA1469X_REF_RCOSC,
};
#define DA1469X_REF_SEL DA1469X_REF_DIVN

enum da1469x_calib_sel {
    DA1469X_CALIB_RC32K = 0,
    DA1469X_CALIB_RC32M,
    DA1469X_CALIB_XTAL32K,
    DA1469X_CALIB_RCX,
    DA1469X_CALIB_RCOSC,
};

enum da1469x_rtc_div_denom_sel {
    DA1469X_RTC_DIV_DENOM_1000 = 0,
    DA1469X_RTC_DIV_DENOM_1024,
};
#define DA1469X_RTC_DIV_DENOM_SEL   DA1469X_RTC_DIV_DENOM_1000

static uint32_t g_mcu_clock_rcx_freq;
static uint32_t g_mcu_clock_rc32k_freq;
static uint32_t g_mcu_clock_rc32m_freq = RC32M_FREQ;
static uint32_t g_mcu_clock_xtal32k_freq;

uint32_t SystemCoreClock = RC32M_FREQ;

static inline bool
da1469x_clock_is_xtal32m_settled(void)
{
    return ((CRG_XTAL->XTALRDY_STAT_REG & CRG_XTAL_XTALRDY_STAT_REG_XTALRDY_COUNT_Msk) == 0) &&
           ((CRG_XTAL->XTAL32M_STAT1_REG & CRG_XTAL_XTAL32M_STAT1_REG_XTAL32M_STATE_Msk) != 0xb);
}

void
da1469x_clock_sys_xtal32m_init(void)
{
    int xtalrdy_cnt;

    /*
     * Number of XTALRDY IRQ timer clock cycles making up the desired xtal settling time,
     * sufficient even at maximum RC32M frequency.
     */
    xtalrdy_cnt = MYNEWT_VAL(MCU_CLOCK_XTAL32M_SETTLE_TIME_US) * XTALRDY_IRQ_FREQ_MAX / 1000000;

    CRG_XTAL->XTALRDY_CTRL_REG = (xtalrdy_cnt << CRG_XTAL_XTALRDY_CTRL_REG_XTALRDY_CNT_Pos) |
                                 (DA1469X_XTALRDY_CLK_SEL << CRG_XTAL_XTALRDY_CTRL_REG_XTALRDY_CLK_SEL_Pos);
}

void
da1469x_clock_sys_xtal32m_enable(void)
{
    int idx;

    idx = da1469x_pdc_find(MCU_PDC_TRIGGER_SW_TRIGGER, MCU_PDC_MASTER_M33,
                           MCU_PDC_EN_XTAL);
    if (idx < 0) {
        idx = da1469x_pdc_add(MCU_PDC_TRIGGER_SW_TRIGGER, MCU_PDC_MASTER_M33,
                              MCU_PDC_EN_XTAL);
    }
    assert(idx >= 0);

    da1469x_pdc_set(idx);
    da1469x_pdc_ack(idx);
}

void
da1469x_clock_sys_xtal32m_switch(void)
{
    if (CRG_TOP->CLK_CTRL_REG & CRG_TOP_CLK_CTRL_REG_RUNNING_AT_RC32M_Msk) {
        CRG_TOP->CLK_SWITCH2XTAL_REG = CRG_TOP_CLK_SWITCH2XTAL_REG_SWITCH2XTAL_Msk;
    } else {
        CRG_TOP->CLK_CTRL_REG = (CRG_TOP->CLK_CTRL_REG &
                                 ~CRG_TOP_CLK_CTRL_REG_SYS_CLK_SEL_Msk) |
                                (DA1469X_SYS_XTAL32M << CRG_TOP_CLK_CTRL_REG_SYS_CLK_SEL_Pos);
    }

    while (!(CRG_TOP->CLK_CTRL_REG & CRG_TOP_CLK_CTRL_REG_RUNNING_AT_XTAL32M_Msk));

    SystemCoreClock = XTAL32M_FREQ;
}

void
da1469x_clock_sys_xtal32m_wait_to_settle(void)
{
    uint32_t primask;

    __HAL_DISABLE_INTERRUPTS(primask);

    NVIC_ClearPendingIRQ(XTAL32M_RDY_IRQn);

    if (!da1469x_clock_is_xtal32m_settled()) {
        NVIC_EnableIRQ(XTAL32M_RDY_IRQn);
        while (!NVIC_GetPendingIRQ(XTAL32M_RDY_IRQn)) {
            __WFI();
        }
        NVIC_DisableIRQ(XTAL32M_RDY_IRQn);
    }

    __HAL_ENABLE_INTERRUPTS(primask);
}

void
da1469x_clock_sys_xtal32m_switch_safe(void)
{
    da1469x_clock_sys_xtal32m_wait_to_settle();

    da1469x_clock_sys_xtal32m_switch();
}

void
da1469x_clock_sys_rc32m_disable(void)
{
    CRG_TOP->CLK_RC32M_REG &= ~CRG_TOP_CLK_RC32M_REG_RC32M_ENABLE_Msk;
}

void
da1469x_clock_sys_rc32m_enable(void)
{
    CRG_TOP->CLK_RC32M_REG |= CRG_TOP_CLK_RC32M_REG_RC32M_ENABLE_Msk;
}

void
da1469x_clock_sys_rc32m_switch(void)
{
    CRG_TOP->CLK_CTRL_REG = (CRG_TOP->CLK_CTRL_REG &
                             ~CRG_TOP_CLK_CTRL_REG_SYS_CLK_SEL_Msk) |
                            (DA1469X_SYS_RC32M << CRG_TOP_CLK_CTRL_REG_SYS_CLK_SEL_Pos);

    while (!(CRG_TOP->CLK_CTRL_REG & CRG_TOP_CLK_CTRL_REG_RUNNING_AT_RC32M_Msk));

    SystemCoreClock = g_mcu_clock_rc32m_freq;
}

void
da1469x_clock_lp_xtal32k_disable(void)
{
    CRG_TOP->CLK_XTAL32K_REG &= ~CRG_TOP_CLK_XTAL32K_REG_XTAL32K_ENABLE_Msk;
}

void
da1469x_clock_lp_xtal32k_enable(void)
{
    CRG_TOP->CLK_XTAL32K_REG |= CRG_TOP_CLK_XTAL32K_REG_XTAL32K_ENABLE_Msk;
}

void
da1469x_clock_lp_xtal32k_switch(void)
{
    CRG_TOP->CLK_CTRL_REG = (CRG_TOP->CLK_CTRL_REG &
                             ~CRG_TOP_CLK_CTRL_REG_LP_CLK_SEL_Msk) |
                            (DA1469X_LP_XTAL32K << CRG_TOP_CLK_CTRL_REG_LP_CLK_SEL_Pos);
    /* If system is running on LP clock update SystemCoreClock */
    if (CRG_TOP->CLK_CTRL_REG & CRG_TOP_CLK_CTRL_REG_RUNNING_AT_LP_CLK_Msk) {
        SystemCoreClock = XTAL32K_FREQ;
    }
}

void
da1469x_clock_lp_rc32k_disable(void)
{
    CRG_TOP->CLK_RC32K_REG &= ~CRG_TOP_CLK_RC32K_REG_RC32K_ENABLE_Msk;
}

void
da1469x_clock_lp_rc32k_enable(void)
{
    CRG_TOP->CLK_RC32K_REG |= CRG_TOP_CLK_RC32K_REG_RC32K_ENABLE_Msk;
}

void
da1469x_clock_lp_rc32k_switch(void)
{
    CRG_TOP->CLK_CTRL_REG = (CRG_TOP->CLK_CTRL_REG &
                             ~CRG_TOP_CLK_CTRL_REG_LP_CLK_SEL_Msk) |
                            (DA1469X_LP_RC32K << CRG_TOP_CLK_CTRL_REG_LP_CLK_SEL_Pos);

    /* If system is running on LP clock update SystemCoreClock */
    if (CRG_TOP->CLK_CTRL_REG & CRG_TOP_CLK_CTRL_REG_RUNNING_AT_LP_CLK_Msk) {
        SystemCoreClock = g_mcu_clock_rc32k_freq;
    }
}

void
da1469x_clock_lp_rcx_enable(void)
{
    CRG_TOP->CLK_RCX_REG  |= CRG_TOP_CLK_RCX_REG_RCX_ENABLE_Msk;
}

void
da1469x_clock_lp_rcx_switch(void)
{
    CRG_TOP->CLK_CTRL_REG = (CRG_TOP->CLK_CTRL_REG &
                             ~CRG_TOP_CLK_CTRL_REG_LP_CLK_SEL_Msk) |
                            (DA1469X_LP_RCX << CRG_TOP_CLK_CTRL_REG_LP_CLK_SEL_Pos);

    /* If system is running on LP clock update SystemCoreClock */
    if (CRG_TOP->CLK_CTRL_REG & CRG_TOP_CLK_CTRL_REG_RUNNING_AT_LP_CLK_Msk) {
        SystemCoreClock = g_mcu_clock_rcx_freq;
    }
}

/**
 * Measure clock frequency
 *
 * @param clock_sel - clock to measure
 *                  0 - RC32K
 *                  1 - RC32M
 *                  2 - XTAL32K
 *                  3 - RCX
 *                  4 - RCOSC
 * @param ref_cnt - number of cycles used for measurement.
 * @return  clock frequency
 */
static uint32_t
da1469x_clock_calibrate(uint8_t clock_sel, uint16_t ref_cnt)
{
    uint32_t ref_val;

    da1469x_pd_acquire(MCU_PD_DOMAIN_PER);

    assert(!(ANAMISC_BIF->CLK_REF_SEL_REG & ANAMISC_BIF_CLK_REF_SEL_REG_REF_CAL_START_Msk));

    ANAMISC_BIF->CLK_REF_CNT_REG = ref_cnt;

    /* Select reference clock & calibrated clock */
    ANAMISC_BIF->CLK_REF_SEL_REG =
        (DA1469X_REF_SEL << ANAMISC_BIF_CLK_REF_SEL_REG_CAL_CLK_SEL_Pos) |
        (clock_sel << ANAMISC_BIF_CLK_REF_SEL_REG_REF_CLK_SEL_Pos);

    /* Start measurement */
    ANAMISC_BIF->CLK_REF_SEL_REG |= ANAMISC_BIF_CLK_REF_SEL_REG_REF_CAL_START_Msk;

    /* Wait for meaurement to complete */
    while (ANAMISC_BIF->CLK_REF_SEL_REG & ANAMISC_BIF_CLK_REF_SEL_REG_REF_CAL_START_Msk);

    ref_val = ANAMISC_BIF->CLK_REF_VAL_REG;

    da1469x_pd_release(MCU_PD_DOMAIN_PER);

    return CAL_REF_FREQ * ref_cnt / ref_val;
}

void
da1469x_clock_lp_rcx_calibrate(void)
{
    g_mcu_clock_rcx_freq =
        da1469x_clock_calibrate(DA1469X_CALIB_RCX, MYNEWT_VAL(MCU_CLOCK_RCX_CAL_REF_CNT));
}

#define RC32K_TARGET_FREQ   32000
#define RC32K_TRIM_MIN      0
#define RC32K_TRIM_MAX      15

static inline uint32_t
rc32k_trim_get(void)
{
    return (CRG_TOP->CLK_RC32K_REG & CRG_TOP_CLK_RC32K_REG_RC32K_TRIM_Msk) >>
           CRG_TOP_CLK_RC32K_REG_RC32K_TRIM_Pos;
}

static inline void
rc32k_trim_set(uint32_t trim)
{
    CRG_TOP->CLK_RC32K_REG =
        (CRG_TOP->CLK_RC32K_REG & ~CRG_TOP_CLK_RC32K_REG_RC32K_TRIM_Msk) |
        (trim << CRG_TOP_CLK_RC32K_REG_RC32K_TRIM_Pos);
}

void
da1469x_clock_lp_rc32k_calibrate(void)
{
    uint32_t trim;
    uint32_t trim_prev;
    uint32_t freq;
    uint32_t freq_prev;
    uint32_t freq_delta;
    uint32_t freq_delta_prev;
    bool trim_ok;

    if (!(CRG_TOP->CLK_RC32K_REG & CRG_TOP_CLK_RC32K_REG_RC32K_ENABLE_Msk)) {
        return;
    }

    freq = 0;
    freq_delta = INT32_MAX;

    trim = rc32k_trim_get();
    trim_prev = trim;
    trim_ok = false;

    do {
        freq_prev = freq;
        freq_delta_prev = freq_delta;

        freq = da1469x_clock_calibrate(DA1469X_CALIB_RC32K, MYNEWT_VAL(MCU_CLOCK_RC32K_CAL_REF_CNT));

        freq_delta = freq - RC32K_TARGET_FREQ;
        freq_delta = (int32_t)freq_delta < 0 ? -freq_delta : freq_delta;

        if (freq_delta > freq_delta_prev) {
            /* Previous trim value was closer to target frequency, use it */
            freq = freq_prev;
            rc32k_trim_set(trim_prev);
            trim_ok = true;
        } else if (freq > RC32K_TARGET_FREQ) {
            /* Decrease trim value if possible */
            if (trim > RC32K_TRIM_MIN) {
                trim_prev = trim;
                rc32k_trim_set(--trim);
            } else {
                trim_ok = true;
            }
        } else if (freq < RC32K_TARGET_FREQ) {
            /* Increase trim value if possible */
            if (trim < RC32K_TRIM_MAX) {
                trim_prev = trim;
                rc32k_trim_set(++trim);
            } else {
                trim_ok = true;
            }
        } else {
            trim_ok = true;
        }
    } while (!trim_ok);

    g_mcu_clock_rc32k_freq = freq;
}

void
da1469x_clock_lp_xtal32k_calibrate(void)
{
#if MYNEWT_VAL(MCU_CLOCK_XTAL32K_ALLOW_CALIB)
    g_mcu_clock_xtal32k_freq =
        da1469x_clock_calibrate(DA1469X_CALIB_XTAL32K, MYNEWT_VAL(MCU_CLOCK_XTAL32K_CAL_REF_CNT));
#else
    g_mcu_clock_xtal32k_freq = XTAL32K_FREQ;
#endif
}

void
da1469x_clock_lp_calibrate(void)
{
#if MYNEWT_VAL_CHOICE(MCU_LPCLK_SOURCE, RCX)
    da1469x_clock_lp_rcx_calibrate();
#elif MYNEWT_VAL_CHOICE(MCU_LPCLK_SOURCE, RC32K)
    da1469x_clock_lp_rc32k_calibrate();
#elif MYNEWT_VAL_CHOICE(MCU_LPCLK_SOURCE, XTAL32K)
    da1469x_clock_lp_xtal32k_calibrate();
#endif
}

void
da1469x_clock_sys_rc32m_calibrate(void)
{
    g_mcu_clock_rc32m_freq = da1469x_clock_calibrate(DA1469X_CALIB_RC32M,
                                                     MYNEWT_VAL(MCU_CLOCK_RC32M_CAL_REF_CNT));
}

uint32_t
da1469x_clock_lp_rcx_freq_get(void)
{
    assert(g_mcu_clock_rcx_freq);

    return g_mcu_clock_rcx_freq;
}

uint32_t
da1469x_clock_lp_rc32k_freq_get(void)
{
    assert(g_mcu_clock_rc32k_freq);

    return g_mcu_clock_rc32k_freq;
}

uint32_t
da1469x_clock_lp_xtal32k_freq_get(void)
{
    assert(g_mcu_clock_xtal32k_freq);

    return g_mcu_clock_xtal32k_freq;
}

uint32_t
da1469x_clock_lp_freq_get(void)
{
#if MYNEWT_VAL_CHOICE(MCU_LPCLK_SOURCE, RCX)
    return da1469x_clock_lp_rcx_freq_get();
#elif MYNEWT_VAL_CHOICE(MCU_LPCLK_SOURCE, RC32K)
    return da1469x_clock_lp_rc32k_freq_get();
#elif MYNEWT_VAL_CHOICE(MCU_LPCLK_SOURCE, XTAL32K)
    return da1469x_clock_lp_xtal32k_freq_get();
#endif
}

uint32_t
da1469x_clock_sys_rc32m_freq_get(void)
{
    assert(g_mcu_clock_rc32m_freq);

    return g_mcu_clock_rc32m_freq;
}

void
da1469x_clock_lp_rcx_disable(void)
{
    CRG_TOP->CLK_RCX_REG &= ~CRG_TOP_CLK_RCX_REG_RCX_ENABLE_Msk;
}

void
da1469x_clock_lp_set_rtc_divs(uint32_t rtc_clock_freq)
{
    /* Please see the DA1469x Datasheet section 34.3 for details */
    uint32_t reg;

    reg = ((rtc_clock_freq % RTC_IN_FREQ_HZ) * RTC_DIV_FRAC_ADJ) <<
          CRG_TOP_CLK_RTCDIV_REG_RTC_DIV_FRAC_Pos;
    reg |= ((rtc_clock_freq / RTC_IN_FREQ_HZ)) <<
           CRG_TOP_CLK_RTCDIV_REG_RTC_DIV_INT_Pos;
    reg |= DA1469X_RTC_DIV_DENOM_SEL << CRG_TOP_CLK_RTCDIV_REG_RTC_DIV_DENOM_Pos;
    reg |= CRG_TOP_CLK_RTCDIV_REG_RTC_DIV_ENABLE_Msk;
    CRG_TOP->CLK_RTCDIV_REG = reg;
}

static void
da1469x_delay_us(uint32_t delay_us)
{
    /* Convert from us to SysTicks (PLL is not started yet) */
    SysTick->LOAD = delay_us * XTAL32M_FREQ / 1000000;
    SysTick->VAL = 0UL;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while (0 == (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
    SysTick->CTRL = 0;
}

/*
 * The following definition are taken from newer version of SDK file DA1469xAB.h.
 * File was not updated due to other registers definitions that went missing.
 * This should be removed once Dialog SDK header has all definitions.
 */
#ifndef CRG_XTAL_PLL_SYS_CTRL1_REG_PLL_SEL_MIN_CUR_INT_Msk
#define CRG_XTAL_PLL_SYS_CTRL1_REG_PLL_SEL_MIN_CUR_INT_Pos (14UL)   /*!< PLL_SEL_MIN_CUR_INT (Bit 14)                          */
#define CRG_XTAL_PLL_SYS_CTRL1_REG_PLL_SEL_MIN_CUR_INT_Msk (0x4000UL) /*!< PLL_SEL_MIN_CUR_INT (Bitfield-Mask: 0x01)           */
#endif

/**
 * Enable PLL96
 */
void
da1469x_clock_sys_pll_enable(void)
{
    /* Start PLL LDO if not done yet */
    if ((CRG_XTAL->PLL_SYS_STATUS_REG & CRG_XTAL_PLL_SYS_STATUS_REG_LDO_PLL_OK_Msk) == 0) {
        CRG_XTAL->PLL_SYS_CTRL1_REG |= CRG_XTAL_PLL_SYS_CTRL1_REG_LDO_PLL_ENABLE_Msk;
        /* Wait for XTAL LDO to settle */
        da1469x_delay_us(20);
    }
    if ((CRG_XTAL->PLL_SYS_STATUS_REG & CRG_XTAL_PLL_SYS_STATUS_REG_PLL_LOCK_FINE_Msk) == 0) {
        /* Enables DXTAL for the system PLL */
        CRG_XTAL->XTAL32M_CTRL0_REG |= CRG_XTAL_XTAL32M_CTRL0_REG_XTAL32M_DXTAL_SYSPLL_ENABLE_Msk;
        /* Use internal VCO current setting to enable precharge */
        CRG_XTAL->PLL_SYS_CTRL1_REG |= CRG_XTAL_PLL_SYS_CTRL1_REG_PLL_SEL_MIN_CUR_INT_Msk;
        /* Enable precharge */
        CRG_XTAL->PLL_SYS_CTRL2_REG |= CRG_XTAL_PLL_SYS_CTRL2_REG_PLL_RECALIB_Msk;
        /* Start the SYSPLL */
        CRG_XTAL->PLL_SYS_CTRL1_REG |= CRG_XTAL_PLL_SYS_CTRL1_REG_PLL_EN_Msk;
        /* Precharge loopfilter (Vtune) */
        da1469x_delay_us(10);
        /* Disable precharge */
        CRG_XTAL->PLL_SYS_CTRL2_REG &= ~CRG_XTAL_PLL_SYS_CTRL2_REG_PLL_RECALIB_Msk;
        /* Extra wait time */
        da1469x_delay_us(5);
        /* Take external VCO current setting */
        CRG_XTAL->PLL_SYS_CTRL1_REG &= ~CRG_XTAL_PLL_SYS_CTRL1_REG_PLL_SEL_MIN_CUR_INT_Msk;
    }
}

void
da1469x_clock_pll_disable(void)
{
    while (CRG_TOP->CLK_CTRL_REG & CRG_TOP_CLK_CTRL_REG_RUNNING_AT_PLL96M_Msk) {
        CRG_TOP->CLK_SWITCH2XTAL_REG = CRG_TOP_CLK_SWITCH2XTAL_REG_SWITCH2XTAL_Msk;

        SystemCoreClock = XTAL32M_FREQ;
    }

    CRG_XTAL->PLL_SYS_CTRL1_REG &= ~(CRG_XTAL_PLL_SYS_CTRL1_REG_PLL_EN_Msk |
                                     CRG_XTAL_PLL_SYS_CTRL1_REG_LDO_PLL_ENABLE_Msk);
}

void
da1469x_clock_pll_wait_to_lock(void)
{
    uint32_t primask;

    __HAL_DISABLE_INTERRUPTS(primask);

    NVIC_ClearPendingIRQ(PLL_LOCK_IRQn);

    if (!da1469x_clock_is_pll_locked()) {
        NVIC_EnableIRQ(PLL_LOCK_IRQn);
        while (!NVIC_GetPendingIRQ(PLL_LOCK_IRQn)) {
            __WFI();
        }
        NVIC_DisableIRQ(PLL_LOCK_IRQn);
    }

    __HAL_ENABLE_INTERRUPTS(primask);
}

void
da1469x_clock_sys_pll_switch(void)
{
    CRG_TOP->CLK_CTRL_REG = (CRG_TOP->CLK_CTRL_REG &
                             ~CRG_TOP_CLK_CTRL_REG_SYS_CLK_SEL_Msk) |
                            (DA1469X_SYS_PLL96M << CRG_TOP_CLK_CTRL_REG_SYS_CLK_SEL_Pos);

    while (!(CRG_TOP->CLK_CTRL_REG & CRG_TOP_CLK_CTRL_REG_RUNNING_AT_PLL96M_Msk));

    SystemCoreClock = PLL_FREQ;
}
