/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* mbed Microcontroller Library - cmsis_nvic for STM32F4
 * Copyright (c) 2009-2011 ARM Limited. All rights reserved.
 *
 * CMSIS-style functionality to support dynamic vectors
 */
#include "bsp/cmsis_nvic.h"

extern char __isr_vector[];
extern char __vector_tbl_reloc__[];

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

    /* Set VTOR */
    SCB->VTOR = (uint32_t)&__vector_tbl_reloc__;
}

void
NVIC_SetVector(IRQn_Type IRQn, uint32_t vector)
{
    uint32_t *vectors;

    vectors = (uint32_t *)SCB->VTOR;
    vectors[IRQn + NVIC_USER_IRQ_OFFSET] = vector;
    __DMB();
}

uint32_t
NVIC_GetVector(IRQn_Type IRQn)
{
    uint32_t *vectors;

    vectors = (uint32_t*)SCB->VTOR;
    return vectors[IRQn + NVIC_USER_IRQ_OFFSET];
}

