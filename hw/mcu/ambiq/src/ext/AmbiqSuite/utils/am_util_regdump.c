//*****************************************************************************
//
//! @file am_util_regdump.c
//!
//! @brief Dump specified registers for debug purposes.
//!
//! This module contains functions for real time (debug) printing of registers
//! from peripherals specified in a given bitmask.
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

#include "am_util_regdump.h"
#include "am_util_stdio.h"


//*****************************************************************************
//
// Register dump structure.
//
//*****************************************************************************
typedef struct
{
    //
    //! Set to 1 to operate this timer as a 32-bit timer instead of two 16-bit
    //! timers.
    //
    uint32_t ui32Offset;

    //
    //! Configuration options for TIMERA
    //
    char *pi8RegName;
    //uint8_t *puiRegName;
}
am_util_regdump_t;


//*****************************************************************************
//
// Globals.
//
//*****************************************************************************
static uint8_t g_ui8Fmt[64];

am_util_regdump_t g_sRegdumpADC[] =
{
    {0x000, "CFG"},
    {0x004, "STAT"},
    {0x008, "SWT"},
    {0x00C, "SL0CFG"},
    {0x010, "SL1CFG"},
    {0x014, "SL2CFG"},
    {0x018, "SL3CFG"},
    {0x01C, "SL4CFG"},
    {0x020, "SL5CFG"},
    {0x024, "SL6CFG"},
    {0x028, "SL7CFG"},
    {0x02C, "WULIM"},
    {0x030, "WLLIM"},
#if INCLUDE_REGS_WITH_SIDE_EFFECT
    {0x038, "FIFO"},
#endif // INCLUDE_REGS_WITH_SIDE_EFFECT
    {0x200, "INTEN"},
    {0x204, "INTSTAT"},
    {0x208, "INTCLR"},
    {0x20C, "INTSET"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpCACHECTRL[] =
{
    {0x000, "CACHECFG"},
    {0x004, "FLASHCFG"},
    {0x008, "CACHECTRL"},
    {0x010, "NCR0START"},
    {0x014, "NCR0END"},
    {0x018, "NCR1START"},
    {0x01C, "NCR1END"},
    {0x030, "CACHEMODE"},
    {0x040, "DMON0"},
    {0x044, "DMON1"},
    {0x048, "DMON2"},
    {0x04C, "DMON3"},
    {0x050, "IMON0"},
    {0x054, "IMON1"},
    {0x058, "IMON2"},
    {0x05C, "IMON3"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpCLKGEN[] =
{
    {0x000, "CALXT"},
    {0x004, "CALRC"},
    {0x008, "ACALCTR"},
    {0x00C, "OCTRL"},
    {0x010, "CLKOUT"},
    {0x014, "CLKKEY"},
    {0x018, "CCTRL"},
    {0x01C, "STATUS"},
    {0x020, "HFADJ"},
    {0x024, "HFVAL"},
    {0x028, "CLOCKEN"},
    {0x02C, "CLOCKEN2"},
    {0x030, "CLOCKEN3"},
    {0x034, "UARTEN"},
    {0x100, "INTEN"},
    {0x104, "INTSTAT"},
    {0x108, "INTCLR"},
    {0x10C, "INTSET"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpCTIMER[] =
{
    {0x000, "TMR0"},
    {0x004, "CMPRA0"},
    {0x008, "CMPRB0"},
    {0x00C, "CTRL0"},
    {0x010, "TMR1"},
    {0x014, "CMPRA1"},
    {0x018, "CMPRB1"},
    {0x01C, "CTRL1"},
    {0x020, "TMR2"},
    {0x024, "CMPRA2"},
    {0x028, "CMPRB2"},
    {0x02C, "CTRL2"},
    {0x030, "TMR3"},
    {0x034, "CMPRA3"},
    {0x038, "CMPRB3"},
    {0x03C, "CTRL3"},
    {0x100, "STCFG"},
    {0x104, "STTMR"},
    {0x108, "CAPTURE_CONTROL"},
    {0x110, "SCMPR0"},
    {0x114, "SCMPR1"},
    {0x118, "SCMPR2"},
    {0x11C, "SCMPR3"},
    {0x120, "SCMPR4"},
    {0x124, "SCMPR5"},
    {0x128, "SCMPR6"},
    {0x12C, "SCMPR7"},
    {0x1E0, "SCAPT0"},
    {0x1E4, "SCAPT1"},
    {0x1E8, "SCAPT2"},
    {0x1EC, "SCAPT3"},
    {0x1F0, "SNVR0"},
    {0x1F4, "SNVR1"},
    {0x1F8, "SNVR2"},
    {0x200, "INTEN"},
    {0x204, "INTSTAT"},
    {0x208, "INTCLR"},
    {0x20C, "INTSET"},
    {0x300, "STMINTEN"},
    {0x304, "STMINTSTAT"},
    {0x308, "STMINTCLR"},
    {0x30C, "STMINTSET"},
    {0xFFFFFFFF, NULL}
};


am_util_regdump_t g_sRegdumpGPIO[] =
{
    {0x000, "PADREGA"},
    {0x004, "PADREGB"},
    {0x008, "PADREGC"},
    {0x00C, "PADREGD"},
    {0x010, "PADREGE"},
    {0x014, "PADREGF"},
    {0x018, "PADREGG"},
    {0x01C, "PADREGH"},
    {0x020, "PADREGI"},
    {0x024, "PADREGJ"},
    {0x028, "PADREGK"},
    {0x02C, "PADREGL"},
    {0x030, "PADREGM"},
    {0x040, "CFGA"},
    {0x044, "CFGB"},
    {0x048, "CFGC"},
    {0x04C, "CFGD"},
    {0x050, "CFGE"},
    {0x054, "CFGF"},
    {0x058, "CFGG"},
    {0x060, "PADKEY"},
    {0x080, "RDA"},
    {0x084, "RDB"},
    {0x088, "WTA"},
    {0x08C, "WTB"},
    {0x090, "WTSA"},
    {0x094, "WTSB"},
    {0x098, "WTCA"},
    {0x09C, "WTCB"},
    {0x0A0, "ENA"},
    {0x0A4, "ENB"},
    {0x0A8, "ENSA"},
    {0x0Ac, "ENSB"},
    {0x0B4, "ENCA"},
    {0x0B8, "ENCB"},
    {0x0BC, "STMRCAP"},
    {0x0C0, "IOM0IRQ"},
    {0x0C4, "IOM1IRQ"},
    {0x0C8, "IOM2IRQ"},
    {0x0CC, "IOM3IRQ"},
    {0x0D0, "IOM4IRQ"},
    {0x0D4, "IOM5IRQ"},
    {0x0D8, "LOOPBACK"},
    {0x0DC, "OBS"},
    {0x0E0, "ALTPADCFGA"},
    {0x0E4, "ALTPADCFGB"},
    {0x0E8, "ALTPADCFGC"},
    {0x0EC, "ALTPADCFGD"},
    {0x0F0, "ALTPADCFGE"},
    {0x0F4, "ALTPADCFGF"},
    {0x0F8, "ALTPADCFGG"},
    {0x0FC, "ALTPADCFGH"},
    {0x100, "ALTPADCFGI"},
    {0x104, "ALTPADCFGJ"},
    {0x108, "ALTPADCFGK"},
    {0x10C, "ALTPADCFGL"},
    {0x110, "ALTPADCFGM"},
    {0x200, "INT0EN"},
    {0x204, "INT0STAT"},
    {0x208, "INT0CLR"},
    {0x20C, "INT0SET"},
    {0x210, "INT1EN"},
    {0x214, "INT1STAT"},
    {0x218, "INT1CLR"},
    {0x21C, "INT1SET"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpIOM[] =
{
#if INCLUDE_REGS_WITH_SIDE_EFFECT
    {0x000, "FIFO"},
#endif // INCLUDE_REGS_WITH_SIDE_EFFECT
    {0x100, "FIFOPTR"},
    {0x104, "TLNGTH"},
    {0x108, "FIFOTHR"},
    {0x10C, "CLKCFG"},
    {0x110, "CMD"},
    {0x114, "CMDRPT"},
    {0x118, "STATUS"},
    {0x11C, "CFG"},
    {0x200, "INTEN"},
    {0x204, "INTSTAT"},
    {0x208, "INTCLR"},
    {0x20C, "INTSET"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpIOS[] =
{
#if INCLUDE_REGS_WITH_SIDE_EFFECT
    {0x000, "FIFO"},
#endif // INCLUDE_REGS_WITH_SIDE_EFFECT
    {0x100, "FIFOPTR"},
    {0x104, "FIFOCFG"},
    {0x108, "FIFOTHR"},
    {0x10C, "FUPD"},
    {0x110, "FIFOCTR"},
    {0x114, "FIFOINC"},
    {0x118, "CFG"},
    {0x11C, "PRENC"},
    {0x120, "INTCTLC"},
    {0x124, "GENADD"},
    {0x200, "INTEN"},
    {0x204, "INTSTAT"},
    {0x208, "INTCLR"},
    {0x20C, "INTSET"},
    {0x210, "REGACCINTEN"},
    {0x214, "REGACCINTSTAT"},
    {0x218, "REGACCINTCLR"},
    {0x21C, "REGACCINTSET"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpPDM[] =
{
    {0x000, "PCFG"},
    {0x004, "VCFG"},
    {0x008, "FR"},
#if INCLUDE_REGS_WITH_SIDE_EFFECT
    {0x00C, "FRD"},
#endif // INCLUDE_REGS_WITH_SIDE_EFFECT
    {0x010, "FLUSH"},
    {0x014, "FTHR"},
    {0x200, "INTEN"},
    {0x204, "INTSTAT"},
    {0x208, "INTCLR"},
    {0x20C, "INTSET"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpPWRCTRL[] =
{
    {0x000, "SUPPLYSRC"},
    {0x004, "POWERSTATUS"},
    {0x008, "DEVICEEN"},
    {0x00C, "SRAMPWDINSLEEP"},
    {0x010, "MEMEN"},
    {0x014, "PWRONSTATUS"},
    {0x018, "SRAMCTRL"},
    {0x01C, "ADCSTATUS"},
    {0x020, "MISCOPT"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpRSTGEN[] =
{
    {0x000, "CFG"},
    {0x004, "SWPOI"},
    {0x008, "SWPOR"},
    {0x00C, "STAT"},
    {0x010, "CLRSTAT"},
    {0x014, "TPIURST"},
    {0x200, "INTEN"},
    {0x204, "INTSTAT"},
    {0x208, "INTCLR"},
    {0x20C, "INTSET"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpRTC[] =
{
    {0x040, "CTRLOW"},
    {0x044, "CTRUP"},
    {0x048, "ALMLOW"},
    {0x04C, "ALMUP"},
    {0x050, "RTCCTL"},
    //
    // The interrupt regs are actually duplicates of CLKGEN
    //
    {0x100, "INTEN"},
    {0x104, "INTSTAT"},
    {0x108, "INTCLR"},
    {0x10C, "INTSET"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpUART[] =
{
#if INCLUDE_REGS_WITH_SIDE_EFFECT
    {0x000, "DR (data)"},
#endif // INCLUDE_REGS_WITH_SIDE_EFFECT
    {0x004, "RSR (status)"},
    {0x018, "FR (flag)"},
    {0x020, "ILPR (IrDA Counter)"},
    {0x024, "IBRD (Baud Div)"},
    {0x028, "FBRD (Frac Baud Div)"},
    {0x02C, "LCRH (Line Ctrl)"},
    {0x030, "CR (Ctrl)"},
    {0x034, "IFLS"},
    {0x038, "IER"},
    {0x03C, "IES"},
    {0x040, "MIS"},
    {0x044, "IEC"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpVCOMP[] =
{
    {0x000, "CFG"},
    {0x004, "STAT"},
    {0x008, "PWDKEY"},
    {0x200, "INTEN"},
    {0x204, "INTSTAT"},
    {0x208, "INTCLR"},
    {0x20C, "INTSET"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpWDT[] =
{
    {0x000, "CFG"},
    {0x004, "RSTRT"},
    {0x008, "LOCK"},
    {0x00C, "COUNT"},
    {0x200, "INTEN"},
    {0x204, "INTSTAT"},
    {0x208, "INTCLR"},
    {0x20C, "INTSET"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpITM[] =
{
    {0xE0000000, "STIM0"},
    {0xE0000004, "STIM1"},
    {0xE0000008, "STIM2"},
    {0xE000000C, "STIM3"},
    {0xE0000010, "STIM4"},
    {0xE0000014, "STIM5"},
    {0xE0000018, "STIM6"},
    {0xE000001C, "STIM7"},
    {0xE0000020, "STIM8"},
    {0xE0000024, "STIM9"},
    {0xE0000028, "STIM10"},
    {0xE000002C, "STIM11"},
    {0xE0000030, "STIM12"},
    {0xE0000034, "STIM13"},
    {0xE0000038, "STIM14"},
    {0xE000003C, "STIM15"},
    {0xE0000040, "STIM16"},
    {0xE0000044, "STIM17"},
    {0xE0000048, "STIM18"},
    {0xE000004C, "STIM19"},
    {0xE0000050, "STIM20"},
    {0xE0000054, "STIM21"},
    {0xE0000058, "STIM22"},
    {0xE000005C, "STIM23"},
    {0xE0000060, "STIM24"},
    {0xE0000064, "STIM25"},
    {0xE0000068, "STIM26"},
    {0xE000006C, "STIM27"},
    {0xE0000070, "STIM28"},
    {0xE0000074, "STIM29"},
    {0xE0000078, "STIM30"},
    {0xE000007C, "STIM31"},
    {0xE0000E00, "TER"},
    {0xE0000E40, "TPR"},
    {0xE0000E80, "TCR"},
    {0xE0000FB4, "LOCKSREG"},
    {0xE0000FD0, "PID4"},
    {0xE0000FD4, "PID5"},
    {0xE0000FD8, "PID6"},
    {0xE0000FDC, "PID7"},
    {0xE0000FE0, "PID0"},
    {0xE0000FE4, "PID1"},
    {0xE0000FE8, "PID2"},
    {0xE0000FEC, "PID3"},
    {0xE0000FF0, "CID0"},
    {0xE0000FF4, "CID1"},
    {0xE0000FF8, "CID2"},
    {0xE0000FFC, "CID3"},
    {0xE0000FB0, "LOCKAREG"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpNVIC[] =
{
    {0xE000E100, "ISER0"},
    {0xE000E180, "ICER0"},
    {0xE000E200, "ISPR0"},
    {0xE000E280, "ICPR0"},
    {0xE000E300, "IABR0"},
    {0xE000E400, "IPR0"},
    {0xE000E404, "IPR1"},
    {0xE000E408, "IPR2"},
    {0xE000E40C, "IPR3"},
    {0xE000E410, "IPR4"},
    {0xE000E414, "IPR5"},
    {0xE000E418, "IPR6"},
    {0xE000E41C, "IPR7"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpSYSCTRL[] =
{
    {0xE000ED04, "ICSR"},
    {0xE000ED08, "VTOR"},
    {0xE000ED0C, "AIRCR"},
    {0xE000ED10, "SCR"},
    {0xE000ED14, "CCR"},
    {0xE000ED18, "SHPR1"},
    {0xE000ED1C, "SHPR2"},
    {0xE000ED20, "SHPR3"},
    {0xE000ED24, "SHCSR"},
    {0xE000ED28, "CFSR"},
    {0xE000ED2C, "HFSR"},
    {0xE000ED34, "MMFAR"},
    {0xE000ED38, "BFAR"},
    {0xE000ED88, "CPACR"},
    {0xE000EDFC, "DEMCR"},
    {0xE000EF00, "STIR"},
    {0xE000EF34, "FPCCR"},
    {0xE000EF38, "FPCAR"},
    {0xE000EF3C, "FPDSCR"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpSYSTICK[] =
{
    {0xE000E010, "SYSTCSR"},
    {0xE000E014, "SYSTRVR"},
    {0xE000E018, "SYSTCVR"},
    {0xE000E01C, "SYSTCALIB"},
    {0xFFFFFFFF, NULL}
};

am_util_regdump_t g_sRegdumpTPIU[] =
{
    {0xE0040000, "SSPSR"},
    {0xE0040004, "CSPSR"},
    {0xE0040010, "ACPR"},
    {0xE00400F0, "SPPR"},
    {0xE0040304, "FFCR"},
    {0xE0040F00, "ITCTRL"},
    {0xE0040FC8, "TYPE"},
    {0xFFFFFFFF, NULL}
};

//*****************************************************************************
//
// Support functions.
//
//*****************************************************************************
static void
regdump_newline_print(uint32_t ui32Num)
{
    while ( ui32Num-- )
    {
        am_util_stdio_printf("\n");
    }
}


static uint32_t
regdump_strlen(char *pcStr)
{
    char *pcS;

    //
    // Loop through the string.
    //
    for ( pcS = pcStr; *pcS; ++pcS );

    //
    // Return the length.
    //
    return (pcS - pcStr);
}

static void
block_print(am_util_regdump_t *psDump, uint32_t ui32BaseAddr)
{
    uint32_t ui32RegAddr;
    uint32_t ux, ui32Len, ui32MaxLen;

    //
    // First, get the maximum register name length.
    //
    ui32MaxLen = ux = 0;
    while ( psDump[ux].ui32Offset != 0xFFFFFFFF )
    {
        ui32Len = regdump_strlen(psDump[ux].pi8RegName);
        if ( ui32Len > ui32MaxLen )
        {
            ui32MaxLen = ui32Len;
        }
        ux++;
    }

    //
    // Create the format string
    //
    am_util_stdio_sprintf((char*)g_ui8Fmt, "  %%-%ds (0x%%08X) = 0x%%08X\n", ui32MaxLen + 1);
//  am_util_stdio_printf("g_ui8Fmt: '%s'\n\n", (char*)g_ui8Fmt);

    //
    // Now, get the value of each register and print it.
    //
    ux = 0;
    while ( psDump[ux].ui32Offset != 0xFFFFFFFF )
    {
        //
        // Format string is of the form: "  %8s (0x%08X) = 0x%08X\n"
        //
        ui32RegAddr = ui32BaseAddr + psDump[ux].ui32Offset;
        am_util_stdio_printf((char*)g_ui8Fmt, psDump[ux].pi8RegName, ui32RegAddr, AM_REGVAL(ui32RegAddr));
        ux++;
    }
}

//*****************************************************************************
//
// printDump() - Print the registers for a given block.
//
// ui32NumModules    = Number of modules in this block
//                     e.g. AM_REG_xxx_NUM_MODULES.
// ui32BlockBaseAddr = Base address of this block.
// ui32ModuleOffset  = Offset, in bytes, between modules in the block
//                     e.g. AM_REG_IOMSTRn(1) - AM_REG_IOMSTRn(0).
// ui32ModuleMask    = Mask of the desired block modules to be printed.
//                     Each lower bit indicates a module.
// pui8BlockName     = Name of the block (e.g "IOM").
// psDump            = ptr to regdump structure for this block.
//
//*****************************************************************************
static void
dump_reg(uint32_t ui32NumModules,
         uint32_t ui32BlockBaseAddr,
         uint32_t ui32ModuleOffset,
         uint32_t ui32ModuleMask,
         char *pui8BlockName,
         am_util_regdump_t *psDump)
{
    uint32_t ui32Module;

    am_util_stdio_printf("%s registers:\n", pui8BlockName);

    ui32Module = 0;
    while ( ui32Module < ui32NumModules )
    {
        if ( (ui32NumModules > 1)  &&
             !(ui32ModuleMask & (1 << ui32Module)) )
        {
            ui32Module++;
            continue;
        }
        else
        {
            if ( ui32NumModules > 1 )
            {
                am_util_stdio_printf(" %s Module %d\n", pui8BlockName, ui32Module);
            }
        }

        block_print(psDump, ui32BlockBaseAddr + (ui32ModuleOffset * ui32Module));

        ui32Module++;
        regdump_newline_print(1);
    }
    regdump_newline_print(1);
}


//*****************************************************************************
//
//! @brief Register dumping for debug purposes.
//!
//! This function dumps register values to the print port for debug purposes.
//!
//! @param ui32PeriphMask = an OR of the mask values to be printed.  e.g.
//! AM_UTIL_REGDUMP_IOM | AM_UTIL_REGDUMP_GPIO
//!
//! @param ui32ModuleMask = A mask representing the modules (for a multi-module
//! block such as IOM) to be dumped.  Bit0 represents module 0, etc.
//! This parameter is ignored for single-module blocks such as GPIO.
//! Pre-defined macros can be used to generate this mask, e.g.
//!     REGDUMP_MOD0 | REGDUMP_MOD1 | REGDUMP_MOD2
//!     or equivalently
//!     REGDUMP_MOD_MAS(0,2)
//!
//*****************************************************************************

void
am_util_regdump_print(uint32_t ui32PeriphMask, uint32_t ui32ModuleMask)
{
    if ( ui32PeriphMask & AM_UTIL_REGDUMP_ADC )
    {
        dump_reg(AM_REG_ADC_NUM_MODULES, AM_REG_ADCn(0),
                 AM_REG_ADCn(1) - AM_REG_ADCn(0),
                 ui32ModuleMask, "ADC", &g_sRegdumpADC[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_CLKGEN )
    {
        dump_reg(AM_REG_CLKGEN_NUM_MODULES, AM_REG_CLKGENn(0),
                 AM_REG_CLKGENn(1) - AM_REG_CLKGENn(0),
                 ui32ModuleMask, "CLKGEN", &g_sRegdumpCLKGEN[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_CTIMER )
    {
        dump_reg(AM_REG_CTIMER_NUM_MODULES, AM_REG_CTIMERn(0),
                 AM_REG_CTIMERn(1) - AM_REG_CTIMERn(0),
                 ui32ModuleMask, "CTIMER", &g_sRegdumpCTIMER[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_GPIO )
    {
        dump_reg(AM_REG_GPIO_NUM_MODULES, AM_REG_GPIOn(0),
                 AM_REG_GPIOn(1) - AM_REG_GPIOn(0),
                 ui32ModuleMask, "GPIO", &g_sRegdumpGPIO[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_IOM )
    {
        dump_reg(AM_REG_IOMSTR_NUM_MODULES, AM_REG_IOMSTRn(0),
                 AM_REG_IOMSTRn(1) - AM_REG_IOMSTRn(0),
                 ui32ModuleMask, "IOM", &g_sRegdumpIOM[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_IOS )
    {
        dump_reg(AM_REG_IOSLAVE_NUM_MODULES, AM_REG_IOSLAVEn(0),
                 AM_REG_IOSLAVEn(1) - AM_REG_IOSLAVEn(0),
                 ui32ModuleMask, "IOS", &g_sRegdumpIOS[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_RSTGEN )
    {
        dump_reg(AM_REG_RSTGEN_NUM_MODULES, AM_REG_RSTGENn(0),
                 AM_REG_RSTGENn(1) - AM_REG_RSTGENn(0),
                 ui32ModuleMask, "RSTGEN", &g_sRegdumpRSTGEN[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_RTC )
    {
        dump_reg(AM_REG_RTC_NUM_MODULES, AM_REG_RTCn(0),
                 AM_REG_RTCn(1) - AM_REG_RTCn(0),
                 ui32ModuleMask, "RTC", &g_sRegdumpRTC[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_UART )
    {
        dump_reg(AM_REG_UART_NUM_MODULES, AM_REG_UARTn(0),
                 AM_REG_UARTn(1) - AM_REG_UARTn(0),
                 ui32ModuleMask, "UART", &g_sRegdumpUART[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_VCOMP )
    {
        dump_reg(AM_REG_VCOMP_NUM_MODULES, AM_REG_VCOMPn(0),
                 AM_REG_VCOMPn(1) - AM_REG_VCOMPn(0),
                 ui32ModuleMask, "VCOMP", &g_sRegdumpVCOMP[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_WDT )
    {
        dump_reg(AM_REG_WDT_NUM_MODULES, AM_REG_WDTn(0),
                 AM_REG_WDTn(1) - AM_REG_WDTn(0),
                 ui32ModuleMask, "WDT", &g_sRegdumpWDT[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_ITM )
    {
        dump_reg(AM_REG_ITM_NUM_MODULES, AM_REG_ITMn(0),
                 AM_REG_ITMn(1) - AM_REG_ITMn(0),
                 ui32ModuleMask, "ITM", &g_sRegdumpITM[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_NVIC )
    {
        dump_reg(AM_REG_NVIC_NUM_MODULES, AM_REG_NVICn(0),
                 AM_REG_NVICn(1) - AM_REG_NVICn(0),
                 ui32ModuleMask, "NVIC", &g_sRegdumpNVIC[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_SYSCTRL )
    {
        dump_reg(AM_REG_SYSCTRL_NUM_MODULES, AM_REG_SYSCTRLn(0),
                 AM_REG_SYSCTRLn(1) - AM_REG_SYSCTRLn(0),
                 ui32ModuleMask, "SYSCTRL", &g_sRegdumpSYSCTRL[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_SYSTICK )
    {
        dump_reg(AM_REG_SYSTICK_NUM_MODULES, AM_REG_SYSTICKn(0),
                 AM_REG_SYSTICKn(1) - AM_REG_SYSTICKn(0),
                 ui32ModuleMask, "SYSTICK", &g_sRegdumpSYSTICK[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_TPIU )
    {
        dump_reg(AM_REG_TPIU_NUM_MODULES, AM_REG_TPIUn(0),
                 AM_REG_TPIUn(1) - AM_REG_TPIUn(0),
                 ui32ModuleMask, "TPIU", &g_sRegdumpTPIU[0]);
    }

#if AM_PART_APOLLO2
    am_util_stdio_printf("Apollo2 specific registers:\n\n");

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_CACHE )
    {
        dump_reg(AM_REG_CACHECTRL_NUM_MODULES, AM_REG_CACHECTRLn(0),
                 AM_REG_CACHECTRLn(1) - AM_REG_CACHECTRLn(0),
                 ui32ModuleMask, "CACHE", &g_sRegdumpCACHECTRL[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_PDM )
    {
        dump_reg(AM_REG_PDM_NUM_MODULES, AM_REG_PDMn(0),
                 AM_REG_PDMn(1) - AM_REG_PDMn(0),
                 ui32ModuleMask, "PDM", &g_sRegdumpPDM[0]);
    }

    if ( ui32PeriphMask & AM_UTIL_REGDUMP_PWRCTRL )
    {
        dump_reg(AM_REG_PWRCTRL_NUM_MODULES, AM_REG_PWRCTRLn(0),
                 AM_REG_PWRCTRLn(1) - AM_REG_PWRCTRLn(0),
                 ui32ModuleMask, "PWRCTRL", &g_sRegdumpPWRCTRL[0]);
    }
#endif // AM_PART_APOLLO2





} // am_util_regdump_print()
