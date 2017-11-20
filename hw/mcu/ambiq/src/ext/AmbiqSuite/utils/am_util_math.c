//*****************************************************************************
//
//! @file am_util_math.c
//!
//! @brief A few useful math functions.
//!
//! Functions for performing some number manipulation.
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
#include "am_util_math.h"

//*****************************************************************************
//
//! @brief Converts a Binary Coded Decimal (BCD) byte to its Decimal form.
//!
//! @param ui8BCDByte is a BCD byte.
//!
//! This function is useful when working with RTC's as they tend to be in BCD 
//! format.
//!
//! @returns the BCD byte converted to Decimal
//
//*****************************************************************************
uint8_t
am_util_math_bcd_to_dec(uint8_t ui8BCDByte)
{
  return (((ui8BCDByte & 0xF0) >> 4) * 10) + (ui8BCDByte & 0x0F);
}

//*****************************************************************************
//
//! @brief Converts a Decimal byte to its Binary Coded Decimal (BCD) form.
//!
//! @param ui8DecimalByte is a Decimal byte.
//!
//! This function is useful when working with RTC's as they tend to be in BCD 
//! format.
//!
//! @returns the Decimal byte converted to BCD.
//
//*****************************************************************************
uint8_t
am_util_math_dec_to_bcd(uint8_t ui8DecimalByte)
{
  return (((ui8DecimalByte / 10) << 4) | (ui8DecimalByte % 10));
}

