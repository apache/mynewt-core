//*****************************************************************************
//
//  am_hal_global.c
//! @file
//!
//! @brief Locate global variables here.
//!
//! This module contains global variables that are used throughout the HAL.
//!
//! One use in particular is that it uses a global HAL flags variable that
//! contains flags used in various parts of the HAL.
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


//*****************************************************************************
//
// Global Variables
//
//*****************************************************************************
uint32_t volatile g_ui32HALflags =  0x00000000;

//*****************************************************************************
//
// Version information
//
//*****************************************************************************
const uint8_t  g_ui8HALcompiler[] = COMPILER_VERSION;
const am_hal_version_t g_ui32HALversion =
{
    .s.bAMREGS  = false,
    .s.Major    = AM_HAL_VERSION_MAJ,
    .s.Minor    = AM_HAL_VERSION_MIN,
    .s.Revision = AM_HAL_VERSION_REV
};

//*****************************************************************************
//
// Static function for reading the timer value.
//
//*****************************************************************************
#if (defined (__ARMCC_VERSION)) && (__ARMCC_VERSION < 6000000)
__asm void
am_hal_triple_read( uint32_t ui32TimerAddr, uint32_t ui32Data[])
{
    push    {r1, r4}                // Save r1=ui32Data, r4
    mrs     r4, PRIMASK             // Save current interrupt state
    cpsid   i                       // Disable INTs while reading the reg
    ldr     r1, [r0, #0]            // Read the designated register 3 times
    ldr     r2, [r0, #0]            //  "
    ldr     r3, [r0, #0]            //  "
    msr     PRIMASK, r4             // Restore interrupt state
    pop     {r0, r4}                // Get r0=ui32Data, restore r4
    str     r1, [r0, #0]            // Store 1st read value to array
    str     r2, [r0, #4]            // Store 2nd read value to array
    str     r3, [r0, #8]            // Store 3rd read value to array
    bx      lr                      // Return to caller
}
#elif (defined (__ARMCC_VERSION)) && (__ARMCC_VERSION >= 6000000)
void
am_hal_triple_read(uint32_t ui32TimerAddr, uint32_t ui32Data[])
{
  __asm (
    " push  {R1, R4}\n"
    " mrs   R4, PRIMASK\n"
    " cpsid i\n"
    " nop\n"
    " ldr   R1, [R0, #0]\n"
    " ldr   R2, [R0, #0]\n"
    " ldr   R3, [R0, #0]\n"
    " msr   PRIMASK, r4\n"
    " pop   {R0, R4}\n"
    " str   R1, [R0, #0]\n"
    " str   R2, [R0, #4]\n"
    " str   R3, [R0, #8]\n"
    :
    : [ui32TimerAddr] "r" (ui32TimerAddr),
      [ui32Data] "r" (&ui32Data[0])
    : "r0", "r1", "r2", "r3", "r4"
  );
}
#elif defined(__GNUC_STDC_INLINE__)
__attribute__((naked))
void
am_hal_triple_read(uint32_t ui32TimerAddr, uint32_t ui32Data[])
{
    __asm
    (
        "   push    {r1, r4}\n"                 // Save r1=ui32Data, r4
        "   mrs     r4, PRIMASK \n"             // Save current interrupt state
        "   cpsid   i           \n"             // Disable INTs while reading the reg
        "   ldr     r1, [r0, #0]\n"             // Read the designated register 3 times
        "   ldr     r2, [r0, #0]\n"             //  "
        "   ldr     r3, [r0, #0]\n"             //  "
        "   msr     PRIMASK, r4 \n"             // Restore interrupt state
        "   pop     {r0, r4}\n"                 // Get r0=ui32Data, restore r4
        "   str     r1, [r0, #0]\n"             // Store 1st read value to array
        "   str     r2, [r0, #4]\n"             // Store 2nd read value to array
        "   str     r3, [r0, #8]\n"             // Store 3rd read value to array
        "   bx      lr          \n"             // Return to caller
    );
}
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma diag_suppress = Pe940   // Suppress IAR compiler warning about missing
                                // return statement on a non-void function
__stackless void
am_hal_triple_read( uint32_t ui32TimerAddr, uint32_t ui32Data[])
{
    __asm(" push    {r1, r4}    ");         // Save r1=ui32Data, r4
    __asm(" mrs     r4, PRIMASK ");         // Save current interrupt state
    __asm(" cpsid   i           ");         // Disable INTs while reading the reg
    __asm(" ldr     r1, [r0, #0]");         // Read the designated register 3 times
    __asm(" ldr     r2, [r0, #0]");         //  "
    __asm(" ldr     r3, [r0, #0]");         //  "
    __asm(" msr     PRIMASK, r4 ");         // Restore interrupt state
    __asm(" pop     {r0, r4}    ");         // Get r0=ui32Data, restore r4
    __asm(" str     r1, [r0, #0]");         // Store 1st read value to array
    __asm(" str     r2, [r0, #4]");         // Store 2nd read value to array
    __asm(" str     r3, [r0, #8]");         // Store 3rd read value to array
    __asm(" bx      lr          ");         // Return to caller
}
#pragma diag_default = Pe940    // Restore IAR compiler warning
#else
#error Compiler is unknown, please contact Ambiq support team
#endif
