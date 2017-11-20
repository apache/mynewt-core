//*****************************************************************************
//
//! @file am_util_ring_buffer.c
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

#include <stdint.h>
#include <stdbool.h>
#include "am_util_ring_buffer.h"

//*****************************************************************************
//
//! @brief Initializes a ring buffer structure.
//!
//! @param psBuffer is a pointer to the buffer structure to be initialized.
//!
//! @param pvArray is a pointer to the array that the new ringbuffer will use
//! for storage space
//!
//! @param ui32Bytes is the total number of bytes that the ringbuffer will be
//! allowed to use.
//!
//! This function should be called on a ring buffer structure before it is
//! used. If this function is called on a ring buffer that is already being
//! used, it will "erase" the buffer, effectively removing all of the buffer
//! contents from the perspective of the other ring buffer access functions.
//! The data will remain in memory, but it will be overwritten as the buffer is
//! used.
//!
//! @note This operation is not inherently thread-safe, so the caller must make
//! sure that it is appropriately guarded from interrupts and context switches.
//!
//! @return
//
//*****************************************************************************
void
am_util_ring_buffer_init(am_util_ring_buffer_t *psBuffer, void *pvArray,
                         uint32_t ui32Bytes)
{
    psBuffer->ui32WriteIndex = 0;
    psBuffer->ui32ReadIndex = 0;
    psBuffer->ui32Length = 0;
    psBuffer->ui32Capacity = ui32Bytes;
    psBuffer->pui8Data = (uint8_t *)pvArray;
}

//*****************************************************************************
//
//! @brief Write a single byte to the ring buffer.
//!
//! @param psBuffer is the address of the ring buffer structure to be written.
//! @param ui8Value is the byte to be added to the ring buffer.
//!
//! This function will write a single byte to the given ring buffer. Make sure
//! that the ring buffer is not already full when calling this function. If the
//! ring buffer is already full, this function will fail silently.
//!
//! @note This operation is not inherently thread-safe, so the caller must make
//! sure that it is appropriately guarded from interrupts and context switches.
//!
//! @return True if the data was written to the buffer. False for insufficient
//! space.
//
//*****************************************************************************
bool
am_util_ring_buffer_write(am_util_ring_buffer_t *psBuffer, void *pvSource,
                          uint32_t ui32Bytes)
{
    uint32_t i;
    uint8_t *pui8Source;

    pui8Source = (uint8_t *) pvSource;

    //
    // Check to make sure that the buffer isn't already full
    //
    if ( am_util_ring_buffer_space_left(psBuffer) >= ui32Bytes )
    {
        //
        // Loop over the bytes in the source array.
        //
        for ( i = 0; i < ui32Bytes; i++ )
        {
            //
            // Write the value to the buffer.
            //
            psBuffer->pui8Data[psBuffer->ui32WriteIndex] = pui8Source[i];

            //
            // Advance the write index, making sure to wrap if necessary.
            //
            psBuffer->ui32WriteIndex = ((psBuffer->ui32WriteIndex + 1) %
                                        psBuffer->ui32Capacity);
        }

        //
        // Update the length value appropriately.
        //
        psBuffer->ui32Length += ui32Bytes;

        //
        // Report a success.
        //
        return true;
    }
    else
    {
        //
        // The ring buffer can't fit the amount of data requested. Return a
        // failure.
        //
        return false;
    }
}

//*****************************************************************************
//
//! @brief Read a single byte from the ring buffer.
//!
//! @param psBuffer is the address of the ring buffer structure to be read.
//!
//! This function will write a single byte to the given ring buffer. Make sure
//! that the ring buffer is not already empty. If the ring buffer is empty,
//! this function will just return a NULL character.
//!
//! @note This operation is not inherently thread-safe, so the caller must make
//! sure that it is appropriately guarded from interrupts and context switches.
//!
//! @return The byte read from the buffer, or a NULL if the buffer was empty.
//
//*****************************************************************************
bool
am_util_ring_buffer_read(am_util_ring_buffer_t *psBuffer, void *pvDest,
                         uint32_t ui32Bytes)
{
    uint32_t i;
    uint8_t *pui8Dest;

    pui8Dest = (uint8_t *) pvDest;

    //
    // Check to make sure that the buffer isn't empty
    //
    if ( am_util_ring_buffer_data_left(psBuffer) >= ui32Bytes )
    {
        //
        // Loop over the bytes in the destination array.
        //
        for ( i = 0; i < ui32Bytes; i++ )
        {
            //
            // Grab the next value from the buffer.
            //
            pui8Dest[i] = psBuffer->pui8Data[psBuffer->ui32ReadIndex];

            //
            // Advance the read index, wrapping if needed.
            //
            psBuffer->ui32ReadIndex = ((psBuffer->ui32ReadIndex + 1) %
                                       psBuffer->ui32Capacity);
        }

        //
        // Adjust the length value to reflect the change.
        //
        psBuffer->ui32Length -= ui32Bytes;

        //
        // Report a success.
        //
        return true;
    }
    else
    {
        //
        // If the buffer didn't have enough data, just return a zero.
        //
        return false;
    }
}
