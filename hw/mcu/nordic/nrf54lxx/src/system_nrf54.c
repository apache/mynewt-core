/*

Copyright (c) 2009-2024 ARM Limited. All rights reserved.

    SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the License); you may
not use this file except in compliance with the License.
You may obtain a copy of the License at

    www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an AS IS BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

NOTICE: This file has been modified by Nordic Semiconductor ASA.

*/

/* NOTE: Template files (including this one) are application specific and therefore expected to
   be copied into the application project folder prior to its use! */

#include <stdint.h>
#include <stdbool.h>
#include "nrf.h"
#include "nrf54l_erratas.h"
#include "system_nrf54l.h"
#include "system_nrf54l_approtect.h"
#include "system_config_sau.h"

/*lint ++flb "Enter library region" */

#define __SYSTEM_CLOCK_DEFAULT      (64000000ul)

/* Trace configuration */
#define TRACE_TRACECLK_PIN          (6ul)
#define TRACE_TRACEDATA0_PIN        (7ul)
#define TRACE_TRACEDATA1_PIN        (8ul)
#define TRACE_TRACEDATA2_PIN        (9ul)
#define TRACE_TRACEDATA3_PIN        (10ul)

#define TRACE_PIN_CLEAR             (~(GPIO_PIN_CNF_CTRLSEL_Msk | GPIO_PIN_CNF_DRIVE0_Msk | GPIO_PIN_CNF_DRIVE1_Msk))

#define TRACE_PIN_CONFIG            ((GPIO_PIN_CNF_DRIVE0_E0 << GPIO_PIN_CNF_DRIVE0_Pos) \
                                    | (GPIO_PIN_CNF_DRIVE1_E1 << GPIO_PIN_CNF_DRIVE1_Pos))


#if defined ( __CC_ARM ) || defined ( __GNUC__ )
uint32_t SystemCoreClock __attribute__((used)) = __SYSTEM_CLOCK_DEFAULT;
#elif defined ( __ICCARM__ )
__root uint32_t SystemCoreClock = __SYSTEM_CLOCK_DEFAULT;
#endif

void SystemCoreClockUpdate(void)
{
    switch(NRF_OSCILLATORS->PLL.CURRENTFREQ)
    {
    case OSCILLATORS_PLL_CURRENTFREQ_CURRENTFREQ_CK64M:
        SystemCoreClock = 64000000ul;
        break;
    case OSCILLATORS_PLL_CURRENTFREQ_CURRENTFREQ_CK128M:
        SystemCoreClock = 128000000ul;
        break;
    }
}

void SystemInit(void)
{
#ifdef __CORTEX_M
#ifndef NRF_SKIP_CLOCK_CONFIGURATION
#if defined(NRF_CONFIG_CPU_FREQ_MHZ) && (NRF_CONFIG_CPU_FREQ_MHZ==64)
    NRF_OSCILLATORS->PLL.FREQ = OSCILLATORS_PLL_FREQ_FREQ_CK64M;
#elif defined(NRF_CONFIG_CPU_FREQ_MHZ) && (NRF_CONFIG_CPU_FREQ_MHZ==128)
    NRF_OSCILLATORS->PLL.FREQ = OSCILLATORS_PLL_FREQ_FREQ_CK128M;
#elif defined(NRF_CONFIG_CPU_FREQ_MHZ)
#error "Illegal CPU frequency set"
#else
    NRF_OSCILLATORS->PLL.FREQ = OSCILLATORS_PLL_FREQ_FREQ_CK128M;
#endif
#endif

#if !defined(NRF_TRUSTZONE_NONSECURE) && defined(__ARM_FEATURE_CMSE)
#ifndef NRF_SKIP_TAMPC_SETUP
    nrf54l_handle_approtect();
#endif
#if defined(__FPU_PRESENT) && __FPU_PRESENT
    /* Allow Non-Secure code to run FPU instructions.
    * If only the secure code should control FPU power state these registers should be configured accordingly in the secure application code. */
    SCB->NSACR |= (3UL << 10ul);
#endif

#ifndef NRF_SKIP_SAU_CONFIGURATION
    configure_default_sau();
#endif

#if !defined (NRF_DISABLE_FICR_TRIMCNF)
    /* Trimming of the device. Copy all the trimming values from FICR into the target addresses. Trim
    until one ADDR is not initialized. */
    uint32_t index = 0ul;
    for (index = 0ul; index < FICR_TRIMCNF_MaxCount && NRF_FICR_NS->TRIMCNF[index].ADDR != 0xFFFFFFFFul && NRF_FICR_NS->TRIMCNF[index].ADDR != 0x00000000ul; index++) {
#if defined ( __ICCARM__ )
        /* IAR will complain about the order of volatile pointer accesses. */
                    #pragma diag_suppress=Pa082
#endif
        * ((volatile uint32_t*)NRF_FICR_NS->TRIMCNF[index].ADDR) = NRF_FICR_NS->TRIMCNF[index].DATA;
#if defined ( __ICCARM__ )
#pragma diag_default=Pa082
#endif
    }
#endif

    /* Device configuration for ES PDK */
#if defined (NRF54L15_XXAA)
    if (*((volatile uint32_t *)0x50120440) == 0x00ul) {
        *((volatile uint32_t *)0x50120440) = 0xC8ul;
    }
#endif

#if NRF54L_ERRATA_32_ENABLE_WORKAROUND
    /* Workaround for Errata 32 */
    if (nrf54l_errata_32())
    {
        if (*((volatile uint32_t *)0x00FFC334ul) <= 0x180A1D00ul){
            *((volatile uint32_t *)0x50120640ul) = 0x1EA9E040ul;
        }
    }
#endif

#endif

    /* Enable the FPU if the compiler used floating point unit instructions. __FPU_USED is a MACRO defined by the
    * compiler. Since the FPU consumes energy, remember to disable FPU use in the compiler if floating point unit
    * operations are not used in your code. */

    /* Allow Non-Secure code to run FPU instructions.
     * If only the secure code should control FPU power state these registers should be configured accordingly in the secure application code. */
    SCB->NSACR |= (3UL << 10ul);

#if (__FPU_USED == 1ul)
    SCB->CPACR |= (3UL << 20ul) | (3UL << 22ul);
    __DSB();
    __ISB();
#endif

#if !defined(NRF_TRUSTZONE_NONSECURE) && defined(__ARM_FEATURE_CMSE)
#if defined(NRF_CONFIG_NFCT_PINS_AS_GPIOS)
    NRF_NFCT_S->PADCONFIG = (NFCT_PADCONFIG_ENABLE_Disabled << NFCT_PADCONFIG_ENABLE_Pos);
#endif

    /* Enable SWO trace functionality. If ENABLE_SWO is not defined, SWO pin will be used as GPIO (see Product
    Specification to see which one). */
#if defined (ENABLE_SWO)
    // Enable trace and debug
                NRF_TAD_S->ENABLE = TAD_ENABLE_ENABLE_Msk;

                // Configure trace port pads
                NRF_P2_S->PIN_CNF[TRACE_TRACECLK_PIN] &= TRACE_PIN_CLEAR;
                NRF_P2_S->PIN_CNF[TRACE_TRACEDATA0_PIN] &= TRACE_PIN_CLEAR;

                NRF_P2_S->PIN_CNF[TRACE_TRACECLK_PIN] |= TRACE_PIN_CONFIG;
                NRF_P2_S->PIN_CNF[TRACE_TRACEDATA0_PIN] |= TRACE_PIN_CONFIG;

                // Configure trace port speed
                NRF_TAD_S->TRACEPORTSPEED = TAD_TRACEPORTSPEED_TRACEPORTSPEED_DIV2;
#endif

    /* Enable Trace functionality. If ENABLE_TRACE is not defined, TRACE pins will be used as GPIOs (see Product
    Specification to see which ones). */
#if defined (ENABLE_TRACE)
    // Enable trace and debug
                NRF_TAD_S->ENABLE = TAD_ENABLE_ENABLE_Msk;

                // Configure trace port pads
                NRF_P2_S->PIN_CNF[TRACE_TRACECLK_PIN] &= TRACE_PIN_CLEAR;
                NRF_P2_S->PIN_CNF[TRACE_TRACEDATA0_PIN] &= TRACE_PIN_CLEAR;
                NRF_P2_S->PIN_CNF[TRACE_TRACEDATA1_PIN] &= TRACE_PIN_CLEAR;
                NRF_P2_S->PIN_CNF[TRACE_TRACEDATA2_PIN] &= TRACE_PIN_CLEAR;
                NRF_P2_S->PIN_CNF[TRACE_TRACEDATA3_PIN] &= TRACE_PIN_CLEAR;

                NRF_P2_S->PIN_CNF[TRACE_TRACECLK_PIN] |= TRACE_PIN_CONFIG;
                NRF_P2_S->PIN_CNF[TRACE_TRACEDATA0_PIN] |= TRACE_PIN_CONFIG;
                NRF_P2_S->PIN_CNF[TRACE_TRACEDATA1_PIN] |= TRACE_PIN_CONFIG;
                NRF_P2_S->PIN_CNF[TRACE_TRACEDATA2_PIN] |= TRACE_PIN_CONFIG;
                NRF_P2_S->PIN_CNF[TRACE_TRACEDATA3_PIN] |= TRACE_PIN_CONFIG;

                // Configure trace port speed
                NRF_TAD_S->TRACEPORTSPEED = TAD_TRACEPORTSPEED_TRACEPORTSPEED_DIV2;
#endif
#endif

#if !defined(NRF_TRUSTZONE_NONSECURE) && !defined (NRF_SKIP_GLITCHDETECTOR_DISABLE)
    /* Disable glitch detector */
#if defined (GLITCHDET_GLITCHDETECTORS)
    NRF_GLITCHDET_S->GLITCHDETECTOR.CONFIG = (GLITCHDET_GLITCHDETECTOR_CONFIG_ENABLE_Disable << GLITCHDET_GLITCHDETECTOR_CONFIG_ENABLE_Pos);
#else
    NRF_GLITCHDET_S->CONFIG = (GLITCHDET_CONFIG_ENABLE_Disable << GLITCHDET_CONFIG_ENABLE_Pos);
#endif
#endif
#endif
}

/*lint --flb "Leave library region" */
