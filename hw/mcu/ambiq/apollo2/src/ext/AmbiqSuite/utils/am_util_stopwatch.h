//*****************************************************************************
//
//! @file am_util_stopwatch.h
//!
//! @brief Provides functionality to measure elapsed time.
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2017, Ambiq Micro
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
// This is part of revision v1.2.10-2-gea660ad-hotfix2 of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_UTIL_STOPWATCH_H
#define AM_UTIL_STOPWATCH_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "hal/am_hal_rtc.h"

//*****************************************************************************
//
// A data structure for holding the formatted time.
//
//*****************************************************************************
typedef struct am_util_stopwatch_elapsed_t
{
    uint32_t ui32MS;
    uint32_t ui32Second;
    uint32_t ui32Minute;
    uint32_t ui32Hour;
    uint32_t ui32Day;
    uint32_t ui32Month;
    uint32_t ui32Year;
} am_util_stopwatch_elapsed_t;

//*****************************************************************************
//
// A data structure for tracking the stopwatch state.
//
//*****************************************************************************
typedef struct am_util_stopwatch_t
{
    uint64_t ui64ElapsedTime;          // Total elapsed time in ms.
    uint64_t ui64PausedTime;           // Total paused time in ms.
    bool bStarted;                     // Stopwatch started state.
    bool bPaused;                      // Stopwatch paused state.
    am_hal_rtc_time_t sStartTime;      // Start time to determine elapsed time.
    am_hal_rtc_time_t sPauseTime;      // Pause time to determine elapsed time.
} am_util_stopwatch_t;

//*****************************************************************************
//
//! @name Resolution for Elapsed Time
//! @brief Defines to pass to am_util_stopwatch_elapsed_get()
//!
//! These macros should be used to specify what resolution to return the
//! elapsed time in.
//! @{
//
//*****************************************************************************
#define AM_UTIL_STOPWATCH_MS            0x0
#define AM_UTIL_STOPWATCH_SECOND        0x1
#define AM_UTIL_STOPWATCH_MINUTE        0x2
#define AM_UTIL_STOPWATCH_HOUR          0x4
#define AM_UTIL_STOPWATCH_DAY           0x8
#define AM_UTIL_STOPWATCH_MONTH         0x10
#define AM_UTIL_STOPWATCH_YEAR          0x20
//! @}

//*****************************************************************************
//
// External function definitions
//
//*****************************************************************************
extern void am_util_stopwatch_init(am_util_stopwatch_t *pStopwatch);
extern void am_util_stopwatch_start(am_util_stopwatch_t *pStopwatch);
extern void am_util_stopwatch_stop(am_util_stopwatch_t *pStopwatch);
extern void am_util_stopwatch_restart(am_util_stopwatch_t *pStopwatch);
extern void am_util_stopwatch_clear(am_util_stopwatch_t *pStopwatch);
extern uint64_t am_util_stopwatch_elapsed_get(am_util_stopwatch_t *pStopwatch,
                                                       uint32_t ui32Resolution);
extern void am_util_stopwatch_elapsed_get_formatted(am_util_stopwatch_t *pStopwatch,
                                                    am_util_stopwatch_elapsed_t *pTime);

#ifdef __cplusplus
}
#endif

#endif // AM_UTIL_STOPWATCH_H
