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
#include <stdint.h>

#include <os/mynewt.h>

#include <hal/hal_bsp.h>
#include <bsp/bsp.h>

static inline int PLL_ODIV(int n)
{
    if (n == 2) {
        return 1;
    } else if (n == 4) {
        return 2;
    } else if (n == 8) {
        return 3;
    } else if (n == 16) {
        return 4;
    } else {
        return 5;
    }
}

uint32_t SystemCoreClock;

#if __PIC32_HAS_L1CACHE
void
dcache_flush_area(void *addr, int size)
{
    uint32_t a = (uint32_t)(addr) & ~3;

    for (; size > 0; --size, a += 4) {
        _cache(17, (void *)a);
    }
}

void
dcache_flush(void)
{
    int i;
    int j;

    for (i = 0; i < 64; ++i) {
        for (j = 0; j < 4; ++j) {
            _cache(1, (void *)((i + j * 64) * 16));
        }
    }
}
#endif

void
SystemClock_Config(void)
{
    /* unlock system for clock configuration */
    SYSKEY = 0x00000000;
    SYSKEY = 0xAA996655;
    SYSKEY = 0x556699AA;

#if (MYNEWT_VAL(SYSTEM_CLOCK_FPLLIDIV))
    fpllidiv = MYNEWT_VAL(SYSTEM_CLOCK_FPLLIDIV);
#endif

#if MYNEWT_VAL_CHOICE(SYSTEM_CLOCK_SRC, POSC) ||\
    MYNEWT_VAL_CHOICE(SYSTEM_CLOCK_SRC, POSC_PLL)
#if MYNEWT_VAL(SYSTEM_CLOCK_OSC_FREQ)
    OSCCON = (OSCCON & ~_OSCCON_NOSC_MASK) | (2 << _OSCCON_NOSC_POSITION) | _OSCCON_OSWEN_MASK;
    SystemCoreClock = MYNEWT_VAL(SYSTEM_CLOCK_OSC_FREQ);
#else
#error When POSC is selected OSC_FREQ must also be specified
#endif
#endif

#if MYNEWT_VAL_CHOICE(SYSTEM_CLOCK_SRC, FRC_DIV)
    OSCCON = (OSCCON & ~(_OSCCON_NOSC_MASK | _OSCCON_FRCDIV_MASK)) | _OSCCON_OSWEN_MASK |
        ((MYNEWT_VAL(SYSTEM_CLOCK_FRC_DIV) - 1) << _OSCCON_FRCDIV_POSITION);
    SystemCoreClock = 8000000 / MYNEWT_VAL(SYSTEM_CLOCK_FRC_DIV);
#elif MYNEWT_VAL_CHOICE(SYSTEM_CLOCK_SRC, POSC)
    OSCCON = (OSCCON & ~_OSCCON_NOSC_MASK) | (2 << _OSCCON_NOSC_POSITION) | _OSCCON_OSWEN_MASK;
    SystemCoreClock = MYNEWT_VAL(SYSTEM_CLOCK_OSC_FREQ);
#elif MYNEWT_VAL(SYSTEM_CLOCK_SRC) == MYNEWT_VAL_CHOICE(SYSTEM_CLOCK_SRC, FRC_PLL)
    /* Running on PLL now, switch to FRC */
    if ((OSCCON & _OSCCON_COSC_MASK) == (1 << _OSCCON_COSC_POSITION)) {
        OSCCON = (OSCCON & ~(_OSCCON_CLKLOCK_MASK | _OSCCON_NOSC_MASK | _OSCCON_FRCDIV_MASK)) | _OSCCON_OSWEN_MASK;
        while (OSCCON & _OSCCON_COSC_MASK) ;
    }
    SPLLCON = (PLL_ODIV(MYNEWT_VAL(SYSTEM_CLOCK_PLLODIV))) << _SPLLCON_PLLODIV_POSITION |
        (MYNEWT_VAL(SYSTEM_CLOCK_PLLMULT) - 1) << _SPLLCON_PLLMULT_POSITION |
        (MYNEWT_VAL(SYSTEM_CLOCK_PLLIDIV) - 1) << _SPLLCON_PLLIDIV_POSITION |
        _SPLLCON_PLLICLK_MASK |
        (MYNEWT_VAL(SYSTEM_CLOCK_PLLRANGE)) << _SPLLCON_PLLRANGE_POSITION;
    OSCCON = (OSCCON & ~_OSCCON_NOSC_MASK) | (1 << _OSCCON_NOSC_POSITION) | _OSCCON_OSWEN_MASK;
    SystemCoreClock = 8000000 / MYNEWT_VAL(SYSTEM_CLOCK_PLLIDIV) *
        MYNEWT_VAL(SYSTEM_CLOCK_PLLMULT) / MYNEWT_VAL(SYSTEM_CLOCK_PLLODIV);

#elif MYNEWT_VAL(SYSTEM_CLOCK_SRC) == MYNEWT_VAL_CHOICE(SYSTEM_CLOCK_SRC, POSC_PLL)
    /* Running on PLL now, switch to FRC */
    if ((OSCCON & _OSCCON_COSC_MASK) == (1 << _OSCCON_COSC_POSITION)) {
        OSCCON = (OSCCON & ~(_OSCCON_CLKLOCK_MASK | _OSCCON_NOSC_MASK | _OSCCON_FRCDIV_MASK)) | _OSCCON_OSWEN_MASK;
        while (OSCCON & _OSCCON_COSC_MASK) ;
    }
    SPLLCON = (PLL_ODIV(MYNEWT_VAL(SYSTEM_CLOCK_PLLODIV))) << _SPLLCON_PLLODIV_POSITION |
        (MYNEWT_VAL(SYSTEM_CLOCK_PLLMULT) - 1) << _SPLLCON_PLLMULT_POSITION |
        (MYNEWT_VAL(SYSTEM_CLOCK_PLLIDIV) - 1) << _SPLLCON_PLLIDIV_POSITION |
        (MYNEWT_VAL(SYSTEM_CLOCK_PLLRANGE)) << _SPLLCON_PLLRANGE_POSITION;
    OSCCON = (OSCCON & ~_OSCCON_NOSC_MASK) | (1 << _OSCCON_NOSC_POSITION) | _OSCCON_OSWEN_MASK;
    SystemCoreClock = MYNEWT_VAL(SYSTEM_CLOCK_OSC_FREQ) / MYNEWT_VAL(SYSTEM_CLOCK_PLLIDIV) *
        MYNEWT_VAL(SYSTEM_CLOCK_PLLMULT) / MYNEWT_VAL(SYSTEM_CLOCK_PLLODIV);
#endif
    /* Lock system since done with clock configuration */
    SYSKEY = 0x33333333;
}

void
SystemInit(void)
{
    /* Configure System Clock */
    SystemClock_Config();
}
