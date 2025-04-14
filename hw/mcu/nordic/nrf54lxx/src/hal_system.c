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

#if MYNEWT_VAL(MCU_DCDC_ENABLED)
    NRF_REGULATORS->VREGMAIN.DCDCEN = 1;
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
    uint32_t regmsk;
    uint32_t regval;
    uint32_t clksrc;

    regmsk = CLOCK_LFCLK_STAT_STATE_Msk | CLOCK_LFCLK_STAT_SRC_Msk;
    regval = CLOCK_LFCLK_STAT_STATE_Running << CLOCK_LFCLK_STAT_STATE_Pos;

#if MYNEWT_VAL_CHOICE(MCU_LFCLK_SOURCE, LFXO)
    regval |= CLOCK_LFCLK_STAT_SRC_LFXO << CLOCK_LFCLK_STAT_SRC_Pos;
    clksrc = CLOCK_LFCLK_STAT_SRC_LFXO;
#elif MYNEWT_VAL_CHOICE(MCU_LFCLK_SOURCE, LFSYNTH)
    regval |= CLOCK_LFCLK_STAT_SRC_LFSYNT << CLOCK_LFCLK_STAT_SRC_Pos;
    clksrc = CLOCK_LFCLK_STAT_SRC_LFSYNT;
#elif MYNEWT_VAL_CHOICE(MCU_LFCLK_SOURCE, LFRC)
    regval |= CLOCK_LFCLK_STAT_SRC_LFRC << CLOCK_LFCLK_STAT_SRC_Pos;
    clksrc = CLOCK_LFCLK_STAT_SRC_LFRC;
#else
    #error Unknown LFCLK source selected
#endif

    NRF_CLOCK_S->TASKS_XOSTOP = 1;

    /* Check if this clock source is already running */
    if ((NRF_CLOCK_S->LFCLK.STAT & regmsk) != regval) {
        NRF_CLOCK_S->TASKS_LFCLKSTOP = 1;
        NRF_CLOCK_S->EVENTS_LFCLKSTARTED = 0;
        NRF_CLOCK_S->LFCLK.SRC = clksrc;
        NRF_CLOCK_S->TASKS_LFCLKSTART = 1;

        /* Wait here till started! */
        while (1) {
            if (NRF_CLOCK_S->EVENTS_LFCLKSTARTED) {
                if ((NRF_CLOCK_S->LFCLK.STAT & regmsk) == regval) {
                    break;
                }
            }
        }
    }
#endif
}
