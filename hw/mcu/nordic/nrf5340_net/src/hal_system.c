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
#include <mcu/nrf5340_net_clock.h>
#include <hal/hal_system.h>
#include <hal/hal_debug.h>
#include <mynewt_cm.h>
#include <nrf.h>

/**
 * Function called at startup. Called after BSS and .data initialized but
 * prior to the _start function.
 *
 * NOTE: this function is called by both the bootloader and the application.
 * If you add code here that you do not want executed in either case you need
 * to conditionally compile it using the config variable BOOT_LOADER (will
 * be set to 1 in case of bootloader build)
 *
 */
void
hal_system_init(void)
{
    NVIC_Relocate();

#if MYNEWT_VAL(MCU_ICACHE_ENABLED)
    NRF_NVMC_NS->ICACHECNF = 1;
#endif
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
hal_debugger_connected(void)
{
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
#if MYNEWT_VAL(MCU_LFCLK_SOURCE)

#if MYNEWT_VAL_CHOICE(MCU_LFCLK_SOURCE, LFXO)
    nrf5340_net_set_lf_clock_source(CLOCK_LFCLKSTAT_SRC_LFXO);
#elif MYNEWT_VAL_CHOICE(MCU_LFCLK_SOURCE, LFSYNTH)
    nrf5340_net_set_lf_clock_source(CLOCK_LFCLKSTAT_SRC_LFSYNT);
#elif MYNEWT_VAL_CHOICE(MCU_LFCLK_SOURCE, LFRC)
    nrf5340_net_set_lf_clock_source(CLOCK_LFCLKSTAT_SRC_LFRC);
#else
    #error Unknown LFCLK source selected
#endif

#endif
}
