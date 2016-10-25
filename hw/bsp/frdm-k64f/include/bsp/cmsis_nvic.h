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

/* mbed Microcontroller Library - cmsis_nvic
 * Copyright (c) 2009-2011 ARM Limited. All rights reserved.
 *
 * CMSIS-style functionality to support dynamic vectors
 */

#ifndef CMSIS_NVIC_H
#define CMSIS_NVIC_H

#include <stdint.h>

#define NVIC_NUM_VECTORS      (16 + 101)   // CORE + MCU Peripherals
#define NVIC_USER_IRQ_OFFSET  16

/*
 * include board definition file which includes: cmsis-core/core_cm4.h
 * this fixes missing CORTEX_M* definition in cmsis_nvic.c
 */
#include "MK64F12.h"

#ifdef __cplusplus
extern "C" {
#endif

void NVIC_Relocate(void);
void NVIC_SetVector(IRQn_Type IRQn, uint32_t vector);
uint32_t NVIC_GetVector(IRQn_Type IRQn);

#ifdef __cplusplus
}
#endif

#endif
