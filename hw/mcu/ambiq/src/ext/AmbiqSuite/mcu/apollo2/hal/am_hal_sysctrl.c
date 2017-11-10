//*****************************************************************************
//
//! @file am_hal_sysctrl.c
//!
//! @brief Functions for interfacing with the M4F system control registers
//!
//! @addtogroup hal Hardware Abstraction Layer (HAL)
//! @addtogroup sysctrl System Control (SYSCTRL)
//! @ingroup hal
//! @{
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
// This is part of revision 1.2.8 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include "am_mcu_apollo.h"

//*****************************************************************************
//
//  Determine if Apollo2 A revision.
//
//  Note - this function is intended to be temporary until Apollo2 revA chips
//  are no longer relevant.
//
//*****************************************************************************
static bool
isRevA(void)
{
    return AM_BFM(MCUCTRL, CHIPREV, REVMAJ) == AM_REG_MCUCTRL_CHIPREV_REVMAJ_A ?
            true : false;
}

//*****************************************************************************
//
//! @brief Place the core into sleep or deepsleep.
//!
//! @param bSleepDeep - False for Normal or True Deep sleep.
//!
//! This function puts the MCU to sleep or deepsleep depending on bSleepDeep.
//!
//! Valid values for bSleepDeep are:
//!
//!     AM_HAL_SYSCTRL_SLEEP_NORMAL
//!     AM_HAL_SYSCTRL_SLEEP_DEEP
//!
//! @return None.
//
//*****************************************************************************
void
am_hal_sysctrl_sleep(bool bSleepDeep)
{
    uint32_t ui32Critical;
    uint32_t ui32CoreBuckEn, ui32MemBuckEn;

    //
    // Disable interrupts and save the previous interrupt state.
    //
    ui32Critical = am_hal_interrupt_master_disable();

    //
    // If the user selected DEEPSLEEP and the TPIU is off, attempt to enter
    // DEEP SLEEP.
    //
    if ((bSleepDeep == AM_HAL_SYSCTRL_SLEEP_DEEP) &&
        (AM_BFM(MCUCTRL, TPIUCTRL, ENABLE) == AM_REG_MCUCTRL_TPIUCTRL_ENABLE_DIS))
    {
        //
        // Prepare the core for deepsleep (write 1 to the DEEPSLEEP bit).
        //
        AM_BFW(SYSCTRL, SCR, SLEEPDEEP, 1);

        if (isRevA())
        {
            //
            // Workaround for an issue with RevA buck-converter sleep behavior.
            // Save the buck enable registers, and then turn them off.
            //
            ui32CoreBuckEn = AM_BFR(PWRCTRL, SUPPLYSRC, COREBUCKEN);
            ui32MemBuckEn = AM_BFR(PWRCTRL, SUPPLYSRC, MEMBUCKEN);
            am_hal_mcuctrl_bucks_disable();
        }
        else
        {
            ui32CoreBuckEn = ui32MemBuckEn = 0;
        }

        //
        // Execute the sleep instruction.
        //
        AM_ASM_WFI;

        //
        // Return from sleep
        //
        if (isRevA())
        {
            //
            // Workaround for an issue with RevA buck-converter sleep behavior.
            // If the bucks were on when this function was called, turn them
            // back on now.
            //
            if (ui32CoreBuckEn || ui32MemBuckEn)
            {
                am_hal_mcuctrl_bucks_enable();
            }
        }
    }
    else
    {
        //
        // Prepare the core for normal sleep (write 0 to the DEEPSLEEP bit).
        //
        AM_BFW(SYSCTRL, SCR, SLEEPDEEP, 0);

        //
        // Go to sleep.
        //
        AM_ASM_WFI;
    }

    //
    // Restore the interrupt state.
    //
    am_hal_interrupt_master_set(ui32Critical);
}

//*****************************************************************************
//
//! @brief Enable the floating point module.
//!
//! Call this function to enable the ARM hardware floating point module.
//!
//! @return None.
//
//*****************************************************************************
void
am_hal_sysctrl_fpu_enable(void)
{
    //
    // Enable access to the FPU in both privileged and user modes.
    // NOTE: Write 0s to all reserved fields in this register.
    //
    AM_REG(SYSCTRL, CPACR) = (AM_REG_SYSCTRL_CPACR_CP11(0x3) |
                             AM_REG_SYSCTRL_CPACR_CP10(0x3));
}

//*****************************************************************************
//
//! @brief Disable the floating point module.
//!
//! Call this function to disable the ARM hardware floating point module.
//!
//! @return None.
//
//*****************************************************************************
void
am_hal_sysctrl_fpu_disable(void)
{
    //
    // Disable access to the FPU in both privileged and user modes.
    // NOTE: Write 0s to all reserved fields in this register.
    //
    AM_REG(SYSCTRL, CPACR) = 0x00000000                     &
                          ~(AM_REG_SYSCTRL_CPACR_CP11(0x3) |
                            AM_REG_SYSCTRL_CPACR_CP10(0x3));
}

//*****************************************************************************
//
//! @brief Enable stacking of FPU registers on exception entry.
//!
//! @param bLazy - Set to "true" to enable "lazy stacking".
//!
//! This function allows the core to save floating-point information to the
//! stack on exception entry. Setting the bLazy option enables "lazy stacking"
//! for interrupt handlers.  Normally, mixing floating-point code and interrupt
//! driven routines causes increased interrupt latency, because the core must
//! save extra information to the stack upon exception entry. With the lazy
//! stacking option enabled, the core will skip the saving of floating-point
//! registers when possible, reducing average interrupt latency.
//!
//! @note This function should be called before the floating-point module is
//! used in interrupt-driven code. If it is not called, the core will not have
//! any way to save context information for floating-point variables on
//! exception entry.
//!
//! @return None.
//
//*****************************************************************************
void
am_hal_sysctrl_fpu_stacking_enable(bool bLazy)
{
    if ( bLazy )
    {
        //
        // Enable automatic saving of FPU registers on exception entry, using lazy
        // context saving.
        //
        AM_REG(SYSCTRL, FPCCR) |= (AM_REG_SYSCTRL_FPCCR_ASPEN(0x1) |
                                   AM_REG_SYSCTRL_FPCCR_LSPEN(0x1));
    }
    else
    {
        //
        // Enable automatic saving of FPU registers on exception entry.
        //
        AM_REG(SYSCTRL, FPCCR) |= AM_REG_SYSCTRL_FPCCR_ASPEN(0x1);
    }
}

//*****************************************************************************
//
//! @brief Disable FPU register stacking on exception entry.
//!
//! This function disables all stacking of floating point registers for
//! interrupt handlers.
//!
//! @return None.
//
//*****************************************************************************
void
am_hal_sysctrl_fpu_stacking_disable(void)
{
    //
    // Enable automatic saving of FPU registers on exception entry, using lazy
    // context saving.
    //
    AM_REG(SYSCTRL, FPCCR) &= ~(AM_REG_SYSCTRL_FPCCR_ASPEN(0x1) |
                                AM_REG_SYSCTRL_FPCCR_LSPEN(0x1));
}

//*****************************************************************************
//
//! @brief Issue a system wide reset using the AIRCR bit in the M4 system ctrl.
//!
//! This function issues a system wide reset (Apollo POR level reset).
//!
//! @return None.
//
//*****************************************************************************
void
am_hal_sysctrl_aircr_reset(void)
{
    //
    // Set the system reset bit in the AIRCR register
    //
    AM_REG(SYSCTRL, AIRCR) = AM_REG_SYSCTRL_AIRCR_VECTKEY(0x5FA) |
                             AM_REG_SYSCTRL_AIRCR_SYSRESETREQ(1);
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
