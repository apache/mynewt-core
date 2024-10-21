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

#include <syscfg/syscfg.h>
#include <mcu/cortex_m33.h>
#include <mcu/nrf54h20_rad_clock.h>
#include <hal/hal_system.h>
#include <hal/hal_debug.h>
#include <mcu/cmsis_nvic.h>
#include <nrf.h>
#include <assert.h>

/* stack limit provided by linker script */
extern uint32_t __StackLimit[];
extern int nrf54h20_rad_clock_grtc_init(void);

void
hal_system_init(void)
{
    uint32_t rc;

    NVIC_Relocate();

//    assert(nrf54h20_rad_clock_grtc_init() == 0);

    /* Setup Cortex-M33 stack limiter to detect stack overflow in interrupts and bootloader code */
    __set_MSPLIM((uint32_t)__StackLimit);
}

void
hal_system_reset(void)
{
 #if MYNEWT_VAL(HAL_SYSTEM_RESET_CB)
     hal_system_reset_cb();
 #endif

     while (1) {
         HAL_DEBUG_BREAK();
         NVIC_SystemReset();
     }
}

int
hal_debugger_connected(void) {
    return CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk;
}

/**
 * hal system clock start
 *
 * Makes sure the LFCLK and/or HFCLK is started.
 */
void
hal_system_clock_start(void)
{
    /* 1. Start local RTC (radiocore only)
     * 2. Get value of GRTC (available for both cores)
     * 3. Get ticks difference between RTC and GRTC
     * 4. Configure abstract timers over the Higher-Speed and High-Speed timers.
     */
}
