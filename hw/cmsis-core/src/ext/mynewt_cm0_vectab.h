/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef MYNEWT_CM0_VECTAB_H
#define MYNEWT_CM0_VECTAB_H

#define NVIC_USER_IRQ_OFFSET          16

extern char __vector_tbl_reloc__[];

__STATIC_INLINE void
NVIC_SetVector(IRQn_Type IRQn, uint32_t vector)
{
    uint32_t *vectors;
#ifndef __VTOR_PRESENT
    vectors = (uint32_t *)&__vector_tbl_reloc__;
#else
    vectors = (uint32_t *)SCB->VTOR;
#endif
    vectors[IRQn + NVIC_USER_IRQ_OFFSET] = vector;
    __DMB();
}

__STATIC_INLINE uint32_t
NVIC_GetVector(IRQn_Type IRQn)
{
    uint32_t *vectors = (uint32_t *)&__vector_tbl_reloc__;
    return vectors[IRQn + NVIC_USER_IRQ_OFFSET];
}

#endif
