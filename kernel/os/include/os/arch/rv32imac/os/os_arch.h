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

#ifndef _OS_ARCH_RV32IMAC_H
#define _OS_ARCH_RV32IMAC_H

#include <stdint.h>
#include "mcu/fe310.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CPU status register */
typedef uint32_t os_sr_t;

/* Stack element */
typedef uint32_t os_stack_t;

/* Stack sizes for common OS tasks */
#define OS_SANITY_STACK_SIZE (96)
#define OS_IDLE_STACK_SIZE (64)

void plic_default_isr(int num);

/* Include common arch definitions and APIs */
#include "os/arch/common.h"

static inline int
os_arch_in_isr(void)
{
    /*
     * TODO: Not implemented yet, add actual code to check it
     */
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _OS_ARCH_RV32IMAC_H */
