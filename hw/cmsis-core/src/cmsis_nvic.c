/* mbed Microcontroller Library
 * CMSIS-style functionality to support dynamic vectors
 *******************************************************************************
 * Copyright (c) 2011 ARM Limited. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of ARM Limited nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************
 */

#include <mynewt_nvic.h>

#ifndef __CORTEX_M
    #error "Macro __CORTEX_M not defined; must define cortex-m type!"
#endif

#if (__CORTEX_M == 0) 
    #ifndef __VTOR_PRESENT
    /* no VTOR is defined so we assume we can't relocate the vector table */
    #define __VTOR_PRESENT  (0)
#endif
#endif

extern char __isr_vector[];
extern char __vector_tbl_reloc__[];

void
NVIC_ConfigVector(IRQn_Type IRQn, void (*vector)(void), uint8_t priority, bool enable)
{
    NVIC_SetPriority(IRQn, priority);
    NVIC_SetVector(IRQn, (uint32_t)vector);
    if (enable) {
        NVIC_EnableIRQ(IRQn);
    }
}

void
NVIC_Relocate(void)
{
    uint32_t *current_location;
    uint32_t *new_location;
    int i;

    /*
     * Relocate the vector table from its current position to the position
     * designated in the linker script.
     */
    current_location = (uint32_t *)&__isr_vector;
    new_location = (uint32_t *)&__vector_tbl_reloc__;

    if (new_location != current_location) {
        for (i = 0; i < NVIC_NUM_VECTORS; i++) {
            new_location[i] = current_location[i];
        }
    }

    /* Set VTOR except for M0 */
#if ((__CORTEX_M == 0) && (__VTOR_PRESENT == 0))
#else
    SCB->VTOR = (uint32_t)&__vector_tbl_reloc__;
#endif
}
