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
#include "mcu/da1469x_clock.h"
#include "mcu/da1469x_pdc.h"
#include "hal/hal_system.h"
#include "DA1469xAB.h"

#if !MYNEWT_VAL(BOOT_LOADER)
static enum hal_reset_reason g_hal_reset_reason;
#endif

void
hal_system_init(void)
{
    /*
     * RESET_STAT_REG has to be cleared to allow HW set bits during next reset
     * so we should read it now and keep result for application to check at any
     * time. This does not happen for bootloader since reading reset reason in
     * bootloader would prevent application from reading it.
     */

#if !MYNEWT_VAL(BOOT_LOADER)
    uint32_t reg;

    reg = CRG_TOP->RESET_STAT_REG;
    CRG_TOP->RESET_STAT_REG = 0;

    if (reg & CRG_TOP_RESET_STAT_REG_PORESET_STAT_Msk) {
        g_hal_reset_reason = HAL_RESET_POR;
    } else if (reg & CRG_TOP_RESET_STAT_REG_WDOGRESET_STAT_Msk) {
        g_hal_reset_reason = HAL_RESET_WATCHDOG;
    } else if (reg & CRG_TOP_RESET_STAT_REG_SWRESET_STAT_Msk) {
        g_hal_reset_reason = HAL_RESET_SOFT;
    } else if (reg & CRG_TOP_RESET_STAT_REG_HWRESET_STAT_Msk) {
        g_hal_reset_reason = HAL_RESET_PIN;
    } else {
        g_hal_reset_reason = 0;
    }
#endif
    /* Disable pad latches */
    CRG_TOP->P0_RESET_PAD_LATCH_REG = CRG_TOP_P0_PAD_LATCH_REG_P0_LATCH_EN_Msk;
    CRG_TOP->P1_RESET_PAD_LATCH_REG = CRG_TOP_P1_PAD_LATCH_REG_P1_LATCH_EN_Msk;

}

void
hal_system_reset(void)
{
    while (1) {
        if (hal_debugger_connected()) {
            asm("bkpt");
        }
        NVIC_SystemReset();
    }
}

int
hal_debugger_connected(void)
{
    return CRG_TOP->SYS_STAT_REG & CRG_TOP_SYS_STAT_REG_DBG_IS_ACTIVE_Msk;
}

void
hal_system_clock_start(void)
{
    /* Reset clock dividers to 0 */
    CRG_TOP->CLK_AMBA_REG &= ~(CRG_TOP_CLK_AMBA_REG_HCLK_DIV_Msk | CRG_TOP_CLK_AMBA_REG_PCLK_DIV_Msk);

    /* Select XTAL32K as LP clock */
    CRG_TOP->CLK_XTAL32K_REG |= CRG_TOP_CLK_XTAL32K_REG_XTAL32K_ENABLE_Msk;
    CRG_TOP->CLK_CTRL_REG = ((CRG_TOP->CLK_CTRL_REG & ~CRG_TOP_CLK_CTRL_REG_LP_CLK_SEL_Msk) |
                                                (2U << CRG_TOP_CLK_CTRL_REG_LP_CLK_SEL_Pos));

    /* Switch to XTAL32M and disable RC32M */
    da1469x_clock_sys_xtal32m_init();
    da1469x_clock_sys_xtal32m_enable();
    da1469x_clock_sys_xtal32m_switch_safe();
    da1469x_clock_sys_rc32m_disable();
}

enum hal_reset_reason
hal_reset_cause(void)
{
#if MYNEWT_VAL(BOOT_LOADER)
    return 0;
#else
    return g_hal_reset_reason;
#endif
}
