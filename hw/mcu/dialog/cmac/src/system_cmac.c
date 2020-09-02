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
#include "cmac_driver/cmac_diag.h"
#include "CMAC.h"

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

    CMAC->CM_CTRL_REG &= ~CMAC_CM_CTRL_REG_CM_BS_RESET_N_Msk;
    CMAC->CM_CTRL_REG |= CMAC_CM_CTRL_REG_CM_BS_RESET_N_Msk;

    CMAC->CM_CTRL_REG = (CMAC->CM_CTRL_REG &
                         ~CMAC_CM_CTRL_REG_CM_CLK_FREQ_MHZ_D2M1_Msk) |
                        (15 << CMAC_CM_CTRL_REG_CM_CLK_FREQ_MHZ_D2M1_Pos);
    CMAC->CM_CTRL2_REG = CMAC_CM_CTRL2_REG_LL_TIMER1_9_0_LIMITED_N_Msk;

    CMAC->CM_CTRL_REG |= CMAC_CM_CTRL_REG_CM_BS_ENABLE_Msk;

    cmac_timer_init();
}
