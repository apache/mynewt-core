//*****************************************************************************
//
//! @file am_hal_entropy.h
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
#ifndef AM_HAL_ENTROPY_H
#define AM_HAL_ENTROPY_H

#include "stdint.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C"
{
#endif

//******************************************************************************
//
// ENTROPY configuration
//
// Use these macros to set the timer number and timer segment to be used for the
// ENTROPY.
//
//******************************************************************************
#define AM_HAL_ENTROPY_CTIMER                  0
#define AM_HAL_ENTROPY_CTIMER_SEGMENT          A

//******************************************************************************
//
// Helper macros for ENTROPY.
//
//******************************************************************************
#define AM_HAL_ENTROPY_CTIMER_SEG2(X) AM_HAL_CTIMER_TIMER ## X
#define AM_HAL_ENTROPY_CTIMER_SEG1(X) AM_HAL_ENTROPY_CTIMER_SEG2(X)
#define AM_HAL_ENTROPY_CTIMER_TIMERX AM_HAL_ENTROPY_CTIMER_SEG1(AM_HAL_ENTROPY_CTIMER_SEGMENT)

#define AM_HAL_ENTROPY_CTIMER_INT2(SEG, NUM) AM_HAL_CTIMER_INT_TIMER ## SEG ## NUM
#define AM_HAL_ENTROPY_CTIMER_INT1(SEG, NUM) AM_HAL_ENTROPY_CTIMER_INT2(SEG, NUM)
#define AM_HAL_ENTROPY_CTIMER_INT \
    AM_HAL_ENTROPY_CTIMER_INT1(AM_HAL_ENTROPY_CTIMER_SEGMENT, AM_HAL_ENTROPY_CTIMER)

//******************************************************************************
//
// Types.
//
//******************************************************************************
typedef void (*am_hal_entropy_callback_t)(void *context);

//******************************************************************************
//
// External Globals.
//
//******************************************************************************
extern uint32_t am_hal_entropy_timing_error_count;

//******************************************************************************
//
// Prototypes.
//
//******************************************************************************
extern void am_hal_entropy_init(void);
extern void am_hal_entropy_enable(void);
extern void am_hal_entropy_disable(void);
extern uint32_t am_hal_entropy_get_values(uint8_t *pui8Output, uint32_t ui32Length,
                                          am_hal_entropy_callback_t pfnCallback,
                                          void *pvContext);


#ifdef __cplusplus
}
#endif

#endif // AM_HAL_ENTROPY_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
