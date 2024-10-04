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
#include <mcu/nrf5340_clock.h>
#include <hal/hal_system.h>
#include <hal/hal_debug.h>
#include "mynewt_cm.h"
#include <nrf.h>
#include <nrfx_config.h>
#include <hal/nrf_oscillators.h>
#include <tfm/tfm.h>

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

#if MYNEWT_VAL(MCU_CACHE_ENABLED)
#if MYNEWT_VAL(MCU_APP_SECURE) || MYNEWT_VAL(BOOT_LOADER)
    NRF_CACHE_S->ENABLE = 1;
#endif
#endif

#if MYNEWT_VAL(MCU_DCDC_ENABLED)
    NRF_REGULATORS->VREGMAIN.DCDCEN = 1;
    if (NRF_REGULATORS->MAINREGSTATUS & REGULATORS_MAINREGSTATUS_VREGH_Msk) {
        NRF_REGULATORS->VREGH.DCDCEN = 1;
    }

#if MYNEWT_VAL(BSP_NRF5340_NET_ENABLE)
    NRF_REGULATORS->VREGRADIO.DCDCEN = 1;
#endif
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

#if MYNEWT_VAL(MCU_HFXO_INTCAP) > 0
static void
hfxo_int_cap_set(void)
{
    uint32_t xosc32mtrim;
    uint32_t slope_field;
    uint32_t slope_mask;
    uint32_t slope_sign;
    int32_t slope;
    uint32_t offset;
    uint32_t capvalue;

    if (tfm_ficr_xosc32mtrim_read(&xosc32mtrim) != 0) {
        /* TODO assert? */
        return;
    }

    /* The SLOPE field is in the two's complement form, hence this special
     * handling. Ideally, it would result in just one SBFX instruction for
     * extracting the slope value, at least gcc is capable of producing such
     * output, but since the compiler apparently tries first to optimize
     * additions and subtractions, it generates slightly less than optimal
     * code.
     */
    slope_field = (xosc32mtrim & FICR_XOSC32MTRIM_SLOPE_Msk) >> FICR_XOSC32MTRIM_SLOPE_Pos;
    slope_mask = FICR_XOSC32MTRIM_SLOPE_Msk >> FICR_XOSC32MTRIM_SLOPE_Pos;
    slope_sign = (slope_mask - (slope_mask >> 1));
    slope = (int32_t)(slope_field ^ slope_sign) - (int32_t)slope_sign;
    offset = (xosc32mtrim & FICR_XOSC32MTRIM_OFFSET_Msk) >> FICR_XOSC32MTRIM_OFFSET_Pos;

    /* As specified in the nRF5340 PS:
     * CAPVALUE = (((FICR->XOSC32MTRIM.SLOPE+56)*(CAPACITANCE*2-14))
     *            +((FICR->XOSC32MTRIM.OFFSET-8)<<4)+32)>>6;
     * where CAPACITANCE is the desired capacitor value in pF, holding any
     * value between 7.0 pF and 20.0 pF in 0.5 pF steps.
     */
    capvalue = ((slope + 56) * ((unsigned int)(MYNEWT_VAL(MCU_HFXO_INTCAP) * 2) - 14)
                + ((offset - 8) << 4) + 32) >> 6;

    nrf_oscillators_hfxo_cap_set(NRF_OSCILLATORS, true, capvalue);
}
#endif

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

    regmsk = CLOCK_LFCLKSTAT_STATE_Msk | CLOCK_LFCLKSTAT_SRC_Msk;
    regval = CLOCK_LFCLKSTAT_STATE_Running << CLOCK_LFCLKSTAT_STATE_Pos;

#if MYNEWT_VAL_CHOICE(MCU_LFCLK_SOURCE, LFXO)

#if MYNEWT_VAL_CHOICE(MCU_LFCLK_XO_INTCAP, external)
    NRF_OSCILLATORS->XOSC32KI.INTCAP = OSCILLATORS_XOSC32KI_INTCAP_INTCAP_External;
#elif MYNEWT_VAL_CHOICE(MCU_LFCLK_XO_INTCAP, c6pf)
    NRF_OSCILLATORS->XOSC32KI.INTCAP = OSCILLATORS_XOSC32KI_INTCAP_INTCAP_C6PF;
#elif MYNEWT_VAL_CHOICE(MCU_LFCLK_XO_INTCAP, c7pf)
    NRF_OSCILLATORS->XOSC32KI.INTCAP = OSCILLATORS_XOSC32KI_INTCAP_INTCAP_C7PF;
#elif MYNEWT_VAL_CHOICE(MCU_LFCLK_XO_INTCAP, c9pf)
    NRF_OSCILLATORS->XOSC32KI.INTCAP = OSCILLATORS_XOSC32KI_INTCAP_INTCAP_C9PF;
#endif

#if !defined(NRF_TRUSTZONE_NONSECURE)
    NRF_P0->PIN_CNF[0] = GPIO_PIN_CNF_MCUSEL_Peripheral << GPIO_PIN_CNF_MCUSEL_Pos;
    NRF_P0->PIN_CNF[1] = GPIO_PIN_CNF_MCUSEL_Peripheral << GPIO_PIN_CNF_MCUSEL_Pos;
#endif
    regval |= CLOCK_LFCLKSTAT_SRC_LFXO << CLOCK_LFCLKSTAT_SRC_Pos;
    clksrc = CLOCK_LFCLKSTAT_SRC_LFXO;
#elif MYNEWT_VAL_CHOICE(MCU_LFCLK_SOURCE, LFSYNTH)
    regval |= CLOCK_LFCLKSTAT_SRC_LFSYNT << CLOCK_LFCLKSTAT_SRC_Pos;
    clksrc = CLOCK_LFCLKSTAT_SRC_LFSYNT;
#elif MYNEWT_VAL_CHOICE(MCU_LFCLK_SOURCE, LFRC)
    regval |= CLOCK_LFCLKSTAT_SRC_LFRC << CLOCK_LFCLKSTAT_SRC_Pos;
    clksrc = CLOCK_LFCLKSTAT_SRC_LFRC;
#else
    #error Unknown LFCLK source selected
#endif

#if MYNEWT_VAL_CHOICE(MCU_LFCLK_SOURCE, LFSYNTH)
    /* Must turn on HFLCK for synthesized 32768 crystal */
    if ((NRF_CLOCK->HFCLKSTAT & CLOCK_HFCLKSTAT_STATE_Msk) !=
        (CLOCK_HFCLKSTAT_STATE_Running << CLOCK_HFCLKSTAT_STATE_Pos)) {
        NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
        nrf5340_clock_hfxo_request();
        while (1) {
            if ((NRF_CLOCK->EVENTS_HFCLKSTARTED) != 0) {
                break;
            }
        }
    } else {
        nrf5340_clock_hfxo_request();
    }
#endif

    /* Check if this clock source is already running */
    if ((NRF_CLOCK->LFCLKSTAT & regmsk) != regval) {
        NRF_CLOCK->TASKS_LFCLKSTOP = 1;
        NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
        NRF_CLOCK->LFCLKSRC = clksrc;
        NRF_CLOCK->TASKS_LFCLKSTART = 1;

        /* Wait here till started! */
        while (1) {
            if (NRF_CLOCK->EVENTS_LFCLKSTARTED) {
                if ((NRF_CLOCK->LFCLKSTAT & regmsk) == regval) {
                    break;
                }
            }
        }
    }
#endif

#if MYNEWT_VAL(MCU_HFXO_INTCAP) > 0
    hfxo_int_cap_set();
#endif

    if (MYNEWT_VAL(MCU_HFCLCK192_DIV) == 1) {
        NRF_CLOCK->HFCLK192MCTRL = 0;
    } else if (MYNEWT_VAL(MCU_HFCLCK192_DIV) == 2) {
        NRF_CLOCK->HFCLK192MCTRL = 1;
    } else if (MYNEWT_VAL(MCU_HFCLCK192_DIV) == 4) {
        NRF_CLOCK->HFCLK192MCTRL = 2;
    }
}
