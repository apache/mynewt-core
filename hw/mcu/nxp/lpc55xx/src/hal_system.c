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

#include <os/mynewt.h>
#include <mcu/cortex_m33.h>
#include <hal/hal_system.h>
#include <clock_config.h>

int hal_debugger_connected(void)
{
    return CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk;
}

void hal_system_reset(void)
{

#if MYNEWT_VAL(HAL_SYSTEM_RESET_CB)
    hal_system_reset_cb();
#endif

    while (1) {
        HAL_DEBUG_BREAK();
        NVIC_SystemReset();
    }
}

void
SystemInitHook(void)
{
    if (MYNEWT_VAL_CHOICE(LPC55XX_BOOT_CLOCK, FRO12M)) {
        BOARD_BootClockFRO12M();
    } else if (MYNEWT_VAL_CHOICE(LPC55XX_BOOT_CLOCK, FROHF96M)) {
        BOARD_BootClockFROHF96M();
    } else if (MYNEWT_VAL_CHOICE(LPC55XX_BOOT_CLOCK, PLL100M)) {
        BOARD_BootClockPLL100M();
    } else if (MYNEWT_VAL_CHOICE(LPC55XX_BOOT_CLOCK, PLL150M)) {
        BOARD_BootClockPLL150M();
    } else if (MYNEWT_VAL_CHOICE(LPC55XX_BOOT_CLOCK, PLL1_150M)) {
        BOARD_BootClockPLL1_150M();
    }
}
