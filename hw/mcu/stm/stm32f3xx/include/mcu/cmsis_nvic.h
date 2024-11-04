/**
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

extern uint32_t __isr_vector_start[];
extern uint32_t __isr_vector_end[];

/* Extract number of vectors from .interrupt section size */
#define NVIC_NUM_VECTORS      (__isr_vector_end - __isr_vector_start)
#define NVIC_USER_IRQ_OFFSET  16

#include "stm32f3xx.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
