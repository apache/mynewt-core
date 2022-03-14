//*****************************************************************************
//
//! @file am_hal_entropy.c
//!
//! @brief Functions for generating true random numbers.
//!
//! @addtogroup entropy3p
//! @ingroup apollo3phal
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2021, Ambiq Micro, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision release_sdk_3_0_0-742e5ac27c of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>

#include "am_mcu_apollo.h"

//******************************************************************************
//
// Entropy configuration
//
//******************************************************************************

// Note: To change this, you will also need to change the timer configuration below.
#define LFRC_FREQ           512

#define MEASURE_PERIOD_MS   10

#define LFRC_TICKS          (((MEASURE_PERIOD_MS) * LFRC_FREQ) / 1000)
#define HFRC_FREQ           48000000

// Note: Done in this order to keep the numbers below 32-bit max integer. This
// should be okay, because HFRC_FREQ/LFRC_FREQ should be a whole number.
#define HFRC_TICKS_EXPECTED ((HFRC_FREQ / LFRC_FREQ) * LFRC_TICKS)

//******************************************************************************
//
// Entropy collector structure.
//
//******************************************************************************
typedef struct am_hal_entropy_collector_s
{
    uint8_t *pui8Data;
    uint32_t ui32Length;
    uint32_t ui32Index;
    am_hal_entropy_callback_t pfnCallback;
    void *pvContext;
}
am_hal_entropy_collector_t;

//******************************************************************************
//
// Globals
//
//******************************************************************************
uint32_t am_hal_entropy_timing_error_count = 0;

static volatile uint32_t g_ui32LastSysTick = 0xFFFFFF;

static am_hal_entropy_collector_t g_sEntropyCollector;
static uint32_t g_ui32LastSleepCount = 0;

//******************************************************************************
//
// CTimer configuration.
//
//******************************************************************************
#if AM_HAL_ENTROPY_CTIMER_TIMERX == AM_HAL_CTIMER_TIMERA
am_hal_ctimer_config_t sENTROPYTimerConfig =
{
    .ui32Link = 0,

    .ui32TimerAConfig = (AM_HAL_CTIMER_FN_REPEAT |
                         AM_HAL_CTIMER_INT_ENABLE |
                         AM_HAL_CTIMER_LFRC_512HZ),

    .ui32TimerBConfig = 0
};
#elif AM_HAL_ENTROPY_CTIMER_TIMERX == AM_HAL_CTIMER_TIMERB
am_hal_ctimer_config_t sENTROPYTimerConfig =
{
    .ui32Link = 0,

    .ui32TimerAConfig = 0,

    .ui32TimerBConfig = (AM_HAL_CTIMER_FN_REPEAT |
                         AM_HAL_CTIMER_INT_ENABLE |
                         AM_HAL_CTIMER_LFRC_512HZ)

};
#endif

//******************************************************************************
//
// CTIMER ISR
//
//******************************************************************************
static void
entropy_ctimer_isr(void)
{
    uint32_t ui32RandomValue;
    uint32_t ui32CurrentSysTick;
    uint32_t ui32ElapsedTicks;

    //
    // Read the current time first.
    //
    ui32CurrentSysTick = am_hal_systick_count();

    //
    // Calculate the elapsed time.
    //
    // We should be able to do modular subtraction here as long as our sample
    // window is less than (SYSTICK_MAX / HFRC_FREQUENCY). Sometimes the value
    // will wrap, but it will still give the right answer unless we wrap twice.
    //
    // For an HFRC frequency of 48 MHz and a Systick max of 0xFFFFFF, we should
    // be okay for ~350 ms.
    //
    ui32ElapsedTicks = (g_ui32LastSysTick - ui32CurrentSysTick) % 0x1000000;

    //
    // Update our "last seen" SysTick time.
    //
    g_ui32LastSysTick = ui32CurrentSysTick;

    am_hal_ctimer_int_clear(AM_HAL_CTIMER_INT_TIMERA0);

    //
    // If the core has gone to sleep since the last time we ran this interrupt
    // routine, we can't trust the random number we got. In that case, just
    // skip this interrupt and wait for the next one.
    //
    if (g_ui32LastSleepCount != g_am_hal_sysctrl_sleep_count)
    {
        g_ui32LastSleepCount = g_am_hal_sysctrl_sleep_count;
        return;
    }
    else
    {
        g_ui32LastSleepCount = g_am_hal_sysctrl_sleep_count;
    }

    //
    // For informational purposes only.
    //
    // This is a tracking variable to make sure our HFRC time interval is
    // reasonable. The variation in the HFRC should be far less than +/-50% in
    // normal circumstances. If this error count increments, it suggests that
    // something in the system is interfering with the timing of the entropy
    // system.
    //
    // In practice, this might happen during a debugger halt or if CTIMER
    // interrupt latency is too high.
    //
    if ((ui32ElapsedTicks > HFRC_TICKS_EXPECTED * 2) ||
        (ui32ElapsedTicks < HFRC_TICKS_EXPECTED / 2))
    {
        am_hal_entropy_timing_error_count++;
    }


    //
    // If we have an active Entropy context, we should start feeding random
    // numbers into it.
    //
    if (g_sEntropyCollector.pui8Data)
    {
        if (g_sEntropyCollector.ui32Index < g_sEntropyCollector.ui32Length)
        {
            //
            // Add the data to the buffer.
            //
            ui32RandomValue = ui32ElapsedTicks - (uint32_t) HFRC_TICKS_EXPECTED;
            g_sEntropyCollector.pui8Data[g_sEntropyCollector.ui32Index++] = (ui32RandomValue & 0xFF);
        }
        else
        {
            //
            // If we've captured all of the data we need, clear the global
            // variables and call the callback.
            //
            g_sEntropyCollector.pui8Data = 0;
            g_sEntropyCollector.ui32Length = 0;
            g_sEntropyCollector.ui32Index = 0;

            g_sEntropyCollector.pfnCallback(g_sEntropyCollector.pvContext);

            g_sEntropyCollector.pfnCallback = 0;
            g_sEntropyCollector.pvContext = 0;

        }
    }
}

//******************************************************************************
//
// CTIMER initialization.
//
//******************************************************************************
static void
entropy_ctimer_init(void)
{
    //
    // Enable the LFRC.
    //
    am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_LFRC_START, AM_HAL_ENTROPY_CTIMER);

    //
    // Set up timer A0.
    //
    am_hal_ctimer_clear(AM_HAL_ENTROPY_CTIMER, AM_HAL_ENTROPY_CTIMER_TIMERX);
    am_hal_ctimer_config(AM_HAL_ENTROPY_CTIMER, &sENTROPYTimerConfig);
    am_hal_ctimer_period_set(AM_HAL_ENTROPY_CTIMER, AM_HAL_ENTROPY_CTIMER_TIMERX, (LFRC_TICKS - 1), 0);
    am_hal_ctimer_int_clear(AM_HAL_ENTROPY_CTIMER_INT);
}

//******************************************************************************
//
// Initialize the ENTROPY
//
//******************************************************************************
void
am_hal_entropy_init(void)
{
    // Configure the timer
    entropy_ctimer_init();

    // Register our interrupt handler for the CTIMER interrupt.
    am_hal_ctimer_int_register(AM_HAL_ENTROPY_CTIMER_INT, entropy_ctimer_isr);

    // Enable interrupt for CTIMER
    am_hal_ctimer_int_enable(AM_HAL_ENTROPY_CTIMER_INT);
    NVIC_EnableIRQ(CTIMER_IRQn);
}

//******************************************************************************
//
// Start the entropy timers.
//
//******************************************************************************
void
am_hal_entropy_enable(void)
{
    //
    // Reset our global error count.
    //
    am_hal_entropy_timing_error_count = 0;

    //
    // Make all of our timers are starting from a known-good state.
    //
    g_ui32LastSysTick = 0x00FFFFFF;
    am_hal_systick_load(0x00FFFFFF);
    am_hal_ctimer_clear(AM_HAL_ENTROPY_CTIMER, AM_HAL_ENTROPY_CTIMER_TIMERX);

    //
    // Start both SysTick and the CTIMER.
    //
    am_hal_systick_start();
    am_hal_ctimer_start(AM_HAL_ENTROPY_CTIMER, AM_HAL_ENTROPY_CTIMER_TIMERX);
}

//******************************************************************************
//
// Stop the entropy timers.
//
//******************************************************************************
void
am_hal_entropy_disable(void)
{
    am_hal_systick_stop();
    am_hal_ctimer_stop(AM_HAL_ENTROPY_CTIMER, AM_HAL_ENTROPY_CTIMER_TIMERX);
}

//******************************************************************************
//
// Place the next N values from the entropy collector into the output location.
//
// - output: where to put the data.
// - length: how much data you want.
// - callback: function to call when the data is ready.
// - context: will be passed to the callback. can be used for anything.
//
//******************************************************************************
uint32_t
am_hal_entropy_get_values(uint8_t *pui8Output, uint32_t ui32Length,
                          am_hal_entropy_callback_t pfnCallback, void *pvContext)
{
    uint32_t ui32RetValue = AM_HAL_STATUS_SUCCESS;

    AM_CRITICAL_BEGIN;

    if (g_sEntropyCollector.pui8Data == 0)
    {
        g_sEntropyCollector.pui8Data = pui8Output;
        g_sEntropyCollector.ui32Length = ui32Length;
        g_sEntropyCollector.pfnCallback = pfnCallback;
        g_sEntropyCollector.pvContext = pvContext;
        g_sEntropyCollector.ui32Index = 0;
    }
    else
    {
        ui32RetValue = AM_HAL_STATUS_FAIL;
    }

    AM_CRITICAL_END;

    return ui32RetValue;
}

