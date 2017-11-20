//*****************************************************************************
//
//! @file am_util_stxetx.c
//!
//! @brief Support for in channel packetization for UART and I/O Slave.
//!
//! Functions for providing packetization and depacketization for communication
//! over UART or I/O Slave.
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
#include <am_mcu_apollo.h>
#include <am_util.h>

#include "am_util_stxetx.h"

//*****************************************************************************
//
// Globals
//
//*****************************************************************************
g_am_util_stxetx_t g_am_util_stxetx;

//
// Define a UART instance number to use in the following macros that call
// HAL UART functions.
//
#define UART_INSTANCE       0

//
// This macro defines how we transmit a byte from am_util_stxetx_tx().
//
#ifndef AM_UTIL_STXETX_TX_XMIT
#define AM_UTIL_STXETX_TX_XMIT(BYTE)                                    \
        (am_hal_uart_char_transmit_buffered(UART_INSTANCE, BYTE))
#endif

//
// This macro defines how we wait for and receive a character.
//
#ifndef AM_UTIL_STXETX_RX_RCV
#define AM_UTIL_STXETX_RX_RCV(PTR)                                      \
        while ( (am_hal_uart_char_receive_buffered(UART_INSTANCE, (char *)PTR, 1) != 1) )
#endif


//*****************************************************************************
//
//! @brief Initialize the STXETX utility.
//!
//! @param rx_function - pointer to the rx packet cracker to be registered.
//!
//! This function initializes the STXETX protocol utilty and registers the
//! function that will be used to crack open received packets.
//!
//! @returns None
//
//*****************************************************************************
void
am_util_stxetx_init( void (*rx_function)(int32_t MeasuredLength, uint8_t *pui8Payload))
{
    //
    // Register the packet cracking call back function.
    //
    g_am_util_stxetx.rx_function = rx_function;

    //
    // Clear the early exit from wait4stx function.
    //
    g_am_util_stxetx.bwait4stx_early_exit = false;
}

//*****************************************************************************
//
//! @brief Formats and transmits an STXETX packetized version of the payload.
//!
//! @param bFirst - issue the STX as the first byte.
//! @param bLast  - issue the ETX as the Last byte.
//! @param ui32Length - number of payload bytes.
//! @param ppui8Payload - pointer to pointer to buffer to be transmitted.
//!
//! This function will transmit the contents of the payload buffer.
//!
//! NOTE: There may be more bytes in the output buffer than came from the input
//! buffer. Let N = length of pay load buffer, then the output byte string can
//! be up to 2N long in some pathological cases.  If both first and last are
//! true then the output can be up to 2N+2 bytes long.
//!
//! @returns Number of characters written to puiPayload.
//
//*****************************************************************************
uint32_t
am_util_stxetx_tx(bool bFirst, bool bLast, uint32_t ui32Length, uint8_t **ppui8Payload)
{
    int i;
    int Length = ui32Length;
    uint32_t ui32FinalLength = 0;


    //
    // Mark the start of a packet with an STX byte.
    //
    if ( bFirst )
    {
        AM_UTIL_STXETX_TX_XMIT(STXETX_STX);
        ui32FinalLength ++;
    }

    //
    // process the transmit Payload
    //
    for ( i = 0; i < Length; i++ )
    {
      if ( (**ppui8Payload == STXETX_STX)   ||
           (**ppui8Payload == STXETX_ETX)   ||
           (**ppui8Payload == STXETX_DLE) )
      {
          //
          // Insert a DLE before one of the protocol bytes.
          //
          AM_UTIL_STXETX_TX_XMIT(STXETX_DLE);
          ui32FinalLength ++;
      }

      //
      // Pass the payload byte along to the output.
      //
      AM_UTIL_STXETX_TX_XMIT(**ppui8Payload);
      *ppui8Payload += 1;

      //
      // Keep track of the number of bytes in the buffer.
      //
      ui32FinalLength ++;
    }


    //
    // Mark the end of a packet with an ETX byte.
    //
    if ( bLast )
    {
        AM_UTIL_STXETX_TX_XMIT(STXETX_ETX);
        ui32FinalLength ++;
    }

    return ui32FinalLength;
}



//*****************************************************************************
//
//! @brief Waits for STX marking start of packet.
//!
//!
//! This function will recieve bytes from the input stream and wait for a valid
//! STX marking the start of packet.
//!
//! The return code signals whether it found an STX or return do to an external,
//! request to exit, e.g. from an ISR.
//!
//! @returns True for valid STX found, i.e. start of packet and false for any
//!          other return cause.
//
//
//*****************************************************************************
bool
am_util_stxetx_rx_wait4start(void)
{
  uint8_t ui8Current = {0};

    //
    // Wait for STX to begin a packet.
    //
    while (1)
    {
        AM_UTIL_STXETX_RX_RCV(&ui8Current);
        am_util_stdio_printf(" 0X%2.2x", ui8Current);

        //
        // detect an STX in the open
        //
        if ( ui8Current == STXETX_STX )
        {
            return true;
        }

        //
        // if we get a DLE in the stream then discard any potential next byte
        // since it could be an invalid STX.
        //
        else if ( ui8Current == STXETX_DLE )
        {
            //
            // discard anything after DLE until we start a packet.
            //
            AM_UTIL_STXETX_RX_RCV(&ui8Current);

        }

        //
        // Check for an early exit request from an ISR or other task.
        //
        if ( g_am_util_stxetx.bwait4stx_early_exit )
        {
            //
            // Early exit before valid STX has been detected.
            //
            return false;
        }
    }
}

//*****************************************************************************
//
//! @brief Receives and extracts an STXETX formated packet.
//!
//! @param pui8Payload - pointer to buffer to load from the UART.
//!
//! This function will recieve the contents of an STXETX demarcated packet from
//! a buffer. Once the end of transmission byte (ETX) byte is received, it
//! passes the extracted payload to the registered packet cracker function.
//! If the registered function pointer is NULL, then it is up to the caller to
//! do any further cracking of the packet.
//!
//! @returns The length of the received packet.
//
//*****************************************************************************
int32_t
am_util_stxetx_rx(uint32_t ui32MaxPayloadSize, uint8_t *pui8Payload)
{
  uint8_t ui8Current = {0};
    uint32_t ui32MeasuredLength = 0;

    //
    // Now we are inside a packet, stay here until we get the whole packet.
    //
    while (1)
    {
        AM_UTIL_STXETX_RX_RCV(&ui8Current);
        am_util_stdio_printf(" 0X%2.2x", ui8Current);

        //
        // Give up if we have too many characters.
        //
        if ( ui32MeasuredLength >= ui32MaxPayloadSize )
        {
            return -1;
        }

        //
        // detect an ETX which will end the packet
        //
        if ( ui8Current == STXETX_ETX )
        {
            break;
        }

        //
        // Handle data link escape (DLE)
        //
        else if ( ui8Current == STXETX_DLE )
        {
            //
            // discard the DLE
            //
            AM_UTIL_STXETX_RX_RCV(&ui8Current);
            am_util_stdio_printf(" DLE: 0X%2.2x", ui8Current);

            //
            // Count this one and push it to the output.
            //
            ui32MeasuredLength++;
            *pui8Payload++ = ui8Current;
        }
        else
        {
            //
            // Push this byte to the rx buffer and count it.
            //
            ui32MeasuredLength++;
            *pui8Payload++ = ui8Current;
        }
    }

    if ( g_am_util_stxetx.rx_function )
    {
        g_am_util_stxetx.rx_function(ui32MeasuredLength, pui8Payload);
    }

    return ui32MeasuredLength;
}
