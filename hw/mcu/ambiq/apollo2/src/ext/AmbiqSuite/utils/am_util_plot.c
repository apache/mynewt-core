//*****************************************************************************
//
//! @file am_util_plot.c
//!
//! @brief A few useful plot functions to be used with AM Flash.
//!
//! Functions for providing AM Flash with the correct data enabling real-time
//! plotting.
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
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "hal/am_hal_itm.h"
#include "am_util_stdio.h"
#include "am_util_plot.h"

//*****************************************************************************
//
// Globals
//
//*****************************************************************************
// variable to keep track if we need to send a sync packet.
uint32_t g_ui32Sync = 0;

//*****************************************************************************
//
//! @brief Initializes the plot interface (ITM)
//!
//! This function initializes the ITM to allow for plotting.
//!
//! @returns None
//
//*****************************************************************************
void
am_util_plot_init(void)
{
    //
    // Enable the ITM.
    //
    am_hal_itm_enable();

    //
    // Initialize the printf interface for ITM/SWO output
    //
    am_util_stdio_printf_init((am_util_stdio_print_char_t) am_hal_itm_print);
}

//*****************************************************************************
//
//! @brief Plots an integer using AM Flash as the viewer.
//!
//! @param ui32Trace - trace number.
//! @param i32Value - value to plot.
//!
//! This function will plot a value to the desired trace. Valid values for
//! ui32Trace are:
//!
//! AM_UTIL_PLOT_0
//! AM_UTIL_PLOT_1
//! AM_UTIL_PLOT_2
//! AM_UTIL_PLOT_3
//!
//! @returns None
//
//*****************************************************************************
void
am_util_plot_int(uint32_t ui32Trace, int32_t i32Value)
{
    if (g_ui32Sync == 0)
    {
        //
        // Send Sync.
        //
        am_hal_itm_sync_send();

        //
        // Reset sync count.
        //
        g_ui32Sync = AM_UTIL_PLOT_SYNC_SEND;
    }
    else
    {
        g_ui32Sync--;
    }

    //
    // Write to the stimulus register.
    //
    am_hal_itm_stimulus_reg_word_write(ui32Trace, i32Value);
}

//*****************************************************************************
//
//! @brief Plots an byte using AM Flash as the veiwer.
//!
//! @param ui32Trace - trace number.
//! @param ui8Value - byte value to plot.
//!
//! This function will plot a byte value to the desired trace. If your plot
//! value fits into a byte, use this function as the ITM traffic can be reduced
//! by a factor of 4 over am_util_plot_int().  Valid values for ui32Trace
//! are:
//!
//! AM_UTIL_PLOT_0
//! AM_UTIL_PLOT_1
//! AM_UTIL_PLOT_2
//! AM_UTIL_PLOT_3
//!
//! @returns None
//
//*****************************************************************************
void
am_util_plot_byte(uint32_t ui32Trace, uint8_t ui8Value)
{
    if (g_ui32Sync == 0)
    {
        //
        // Send Sync.
        //
        am_hal_itm_sync_send();

        //
        // Reset sync count.
        //
        g_ui32Sync = AM_UTIL_PLOT_SYNC_SEND;
    }
    else
    {
        g_ui32Sync--;
    }

    //
    // Write to the stimulus register.
    //
    am_hal_itm_stimulus_reg_byte_write(ui32Trace, ui8Value);
}
