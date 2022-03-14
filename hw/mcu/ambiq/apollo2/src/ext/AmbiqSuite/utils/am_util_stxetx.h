//*****************************************************************************
//
//! @file am_util_stxetx.h
//!
//! @brief A packetization protocol for use with UART or I/O Slave.
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
#ifndef AM_UTIL_STXETX_H
#define AM_UTIL_STXETX_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @name STXETX byte code assignments.
//! @brief Byte code defines for the three special codes: STX, ETX and DLE.
//!
//! These macros are used in the byte stream.
//! @{
//
//*****************************************************************************
#define STXETX_STX (0x9A)
#define STXETX_ETX (0x9B)
#define STXETX_DLE (0x99)
//! @}


//*****************************************************************************
//
//! @name STXETX global data structure definition.
//! @brief Global data structure definitons for the STXETX UART protocol.
//!
//
//*****************************************************************************
typedef struct
{
    void (*rx_function)(int32_t, uint8_t *);
    bool bwait4stx_early_exit;
}g_am_util_stxetx_t;

extern g_am_util_stxetx_t g_am_util_stxetx;


//*****************************************************************************
//
// External function definitions
//
//*****************************************************************************
extern void am_util_stxetx_init( void (*rx_function)(int32_t MeasuredLength, uint8_t *pui8Payload));
extern uint32_t am_util_stxetx_tx(bool bFirst, bool bLast, uint32_t ui32Length, 
                                  uint8_t **ppui8Payload);
extern bool am_util_stxetx_rx_wait4start(void);
extern int32_t am_util_stxetx_rx(uint32_t ui32MaxPayloadSize, 
                                 uint8_t *pui8Payload);



#ifdef __cplusplus
}
#endif

#endif // AM_UTIL_STXETX_H

