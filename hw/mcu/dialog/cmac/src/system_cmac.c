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
#include "syscfg/syscfg.h"
#include "mcu/cmac_hal.h"
#include "mcu/cmac_pdc.h"
#include "mcu/cmac_timer.h"
#include "mcu/cmsis_nvic.h"
#include "mcu/mcu.h"
#include "hal/hal_system.h"
#include <ipc_cmac/diag.h>
#include "CMAC.h"

uint8_t g_mcu_chip_variant;

static inline uint32_t
get_reg32_bits(uint32_t addr, uint32_t mask)
{
    volatile uint32_t *reg = (volatile uint32_t *)addr;

    return (*reg & mask) >> __builtin_ctz(mask);
}

static inline void
set_reg32_bits(uint32_t addr, uint32_t mask, uint32_t val)
{
    volatile uint32_t *reg = (volatile uint32_t *)addr;

    *reg = (*reg & (~mask)) | (val << __builtin_ctz(mask));
}

static void
read_chip_variant(void)
{
    switch (get_reg32_bits(0x50040200, 0xff)) {
    default:
        /* use TSMC as default chip variant and hope it will work for unknown
         * chips */
    case '2':
        g_mcu_chip_variant = MCU_CHIP_VARIANT_TSMC;
        break;
    case '3':
        g_mcu_chip_variant = MCU_CHIP_VARIANT_GF;
        break;
    }
}

void
SystemInit(void)
{
#if MYNEWT_VAL(CMAC_DEBUG_DIAG_ENABLE)
    cmac_diag_setup_cmac();
#endif

#if MYNEWT_VAL(MCU_DEBUG_SWD_WAIT_FOR_ATTACH)
    while (!hal_debugger_connected());
    for (int i = 0; i < 1000000; i++);
#endif

    read_chip_variant();

    CMAC->CM_CTRL_REG &= ~CMAC_CM_CTRL_REG_CM_BS_RESET_N_Msk;
    CMAC->CM_CTRL_REG |= CMAC_CM_CTRL_REG_CM_BS_RESET_N_Msk;

    CMAC->CM_CTRL_REG = (CMAC->CM_CTRL_REG &
                         ~CMAC_CM_CTRL_REG_CM_CLK_FREQ_MHZ_D2M1_Msk) |
                        (15 << CMAC_CM_CTRL_REG_CM_CLK_FREQ_MHZ_D2M1_Pos);
    CMAC->CM_CTRL2_REG = CMAC_CM_CTRL2_REG_LL_TIMER1_9_0_LIMITED_N_Msk;

    CMAC->CM_CTRL_REG |= CMAC_CM_CTRL_REG_CM_BS_ENABLE_Msk;

    cmac_timer_init();

    set_reg32_bits(0x40000904, 0x0004, 1);
}
