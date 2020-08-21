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

static uint32_t g_mcu_clock_rcx_freq;

static inline bool
da1469x_clock_is_xtal32m_settled(void)
{
    return ((*(uint32_t *)0x5001001c & 0xff00) == 0) &&
           ((*(uint32_t *)0x50010054 & 0x000f) != 0xb);
}

void
da1469x_clock_sys_xtal32m_init(void)
{
    uint32_t reg;
    int xtalrdy_cnt;

    /* Number of lp_clk cycles (~30.5us) */
    xtalrdy_cnt = MYNEWT_VAL(MCU_CLOCK_XTAL32M_SETTLE_TIME_US) * 10 / 305;

    reg = CRG_XTAL->XTALRDY_CTRL_REG;
    reg &= ~(CRG_XTAL_XTALRDY_CTRL_REG_XTALRDY_CLK_SEL_Msk |
             CRG_XTAL_XTALRDY_CTRL_REG_XTALRDY_CNT_Msk);
    reg |= xtalrdy_cnt;
    CRG_XTAL->XTALRDY_CTRL_REG = reg;
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
        CRG_TOP->CLK_CTRL_REG &= ~CRG_TOP_CLK_CTRL_REG_SYS_CLK_SEL_Msk;
    }

    while (!(CRG_TOP->CLK_CTRL_REG & CRG_TOP_CLK_CTRL_REG_RUNNING_AT_XTAL32M_Msk));
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
da1469x_clock_lp_xtal32k_enable(void)
{
    CRG_TOP->CLK_XTAL32K_REG |= CRG_TOP_CLK_XTAL32K_REG_XTAL32K_ENABLE_Msk;
}

void
da1469x_clock_lp_xtal32k_switch(void)
{
    CRG_TOP->CLK_CTRL_REG = (CRG_TOP->CLK_CTRL_REG &
                             ~CRG_TOP_CLK_CTRL_REG_LP_CLK_SEL_Msk) |
                            (2 << CRG_TOP_CLK_CTRL_REG_LP_CLK_SEL_Pos);
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
                            (1 << CRG_TOP_CLK_CTRL_REG_LP_CLK_SEL_Pos);
}

void
da1469x_clock_lp_rcx_calibrate(void)
{
    /*
     * XXX not sure what is a good value here, this is just some value that
     *     seems to work fine; we need to check how accurate this is and perhaps
     *     also make it configurable if necessary
     */
    const uint32_t ref_cnt = 100;
    uint32_t ref_val;

    da1469x_pd_acquire(MCU_PD_DOMAIN_PER);

    assert(CRG_TOP->CLK_CTRL_REG & CRG_TOP_CLK_CTRL_REG_RUNNING_AT_XTAL32M_Msk);
    assert(!(ANAMISC->CLK_REF_SEL_REG & ANAMISC_CLK_REF_SEL_REG_REF_CAL_START_Msk));

    ANAMISC->CLK_REF_CNT_REG = ref_cnt;
    ANAMISC->CLK_REF_SEL_REG = (3 << ANAMISC_CLK_REF_SEL_REG_REF_CLK_SEL_Pos) |
                               ANAMISC_CLK_REF_SEL_REG_REF_CAL_START_Msk;

    while (ANAMISC->CLK_REF_SEL_REG & ANAMISC_CLK_REF_SEL_REG_REF_CAL_START_Msk);

    ref_val = ANAMISC->CLK_REF_VAL_REG;

    da1469x_pd_release(MCU_PD_DOMAIN_PER);

    g_mcu_clock_rcx_freq = 32000000 * ref_cnt / ref_val;
}

uint32_t
da1469x_clock_lp_rcx_freq_get(void)
{
    assert(g_mcu_clock_rcx_freq);

    return g_mcu_clock_rcx_freq;
}

void
da1469x_clock_lp_rcx_disable(void)
{
    CRG_TOP->CLK_RCX_REG &= ~CRG_TOP_CLK_RCX_REG_RCX_ENABLE_Msk;
}

static void
da1469x_delay_us(uint32_t delay_us)
{
    /*
     * SysTick runs on ~32 MHz clock while PLL is not started.
     * so multiplying by 32 to convert from us to SysTicks.
     */
    SysTick->LOAD = delay_us * 32;
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
    /* CLK_SEL_Msk == 3 means PLL */
    CRG_TOP->CLK_CTRL_REG |= CRG_TOP_CLK_CTRL_REG_SYS_CLK_SEL_Msk;

    while (!(CRG_TOP->CLK_CTRL_REG & CRG_TOP_CLK_CTRL_REG_RUNNING_AT_PLL96M_Msk));
}
