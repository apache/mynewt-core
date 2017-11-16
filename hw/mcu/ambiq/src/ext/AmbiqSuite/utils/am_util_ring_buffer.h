//*****************************************************************************
//
//! @file am_util_ring_buffer.h
//!
//! @brief Some helper functions for implementing and managing a ring buffer.
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
#ifndef AM_UTIL_RING_BUFFER_H
#define AM_UTIL_RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif
//*****************************************************************************
//
// External variable definitions
//
//*****************************************************************************
typedef struct
{
    volatile uint8_t *pui8Data;
    volatile uint32_t ui32WriteIndex;
    volatile uint32_t ui32ReadIndex;
    volatile uint32_t ui32Length;
    volatile uint32_t ui32Capacity;
}
am_util_ring_buffer_t;

//*****************************************************************************
//
// Function-like macros.
//
//*****************************************************************************
#define am_util_ring_buffer_empty(psBuffer)                                   \
    ((psBuffer)->ui32Length == 0)

#define am_util_ring_buffer_full(psBuffer)                                    \
    ((psBuffer)->ui32Length == (psBuffer)->ui32Capacity)

#define am_util_ring_buffer_space_left(psBuffer)                              \
    ((psBuffer)->ui32Capacity - (psBuffer)->ui32Length)

#define am_util_ring_buffer_data_left(psBuffer)                               \
    ((psBuffer)->ui32Length)

//*****************************************************************************
//
// External function definitions
//
//*****************************************************************************
void am_util_ring_buffer_init(am_util_ring_buffer_t *psBuffer,
                              void *pvArray, uint32_t ui32Bytes);

bool am_util_ring_buffer_write(am_util_ring_buffer_t *psBuffer,
                               void *pvSource, uint32_t ui32Bytes);

bool am_util_ring_buffer_read(am_util_ring_buffer_t *psBuffer,
                              void *pvDest, uint32_t ui32Bytes);

#ifdef __cplusplus
}
#endif

#endif // AM_UTIL_RING_BUFFER_H

