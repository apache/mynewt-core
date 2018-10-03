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

#ifndef _OS_ARCH_MIPS_H
#define _OS_ARCH_MIPS_H

#include <stdint.h>
#include <mips/cpu.h>
#include <mips/m32c0.h>
#include "mcu/mips.h"

/* CPU status register */
typedef uint32_t os_sr_t;

/* Stack element */
typedef uint32_t os_stack_t;

#define OS_ALIGNMENT        (4)
#define OS_STACK_ALIGNMENT  (8)

/* Stack sizes for common OS tasks */
#define OS_SANITY_STACK_SIZE (64)
#define OS_IDLE_STACK_SIZE (64)

#define OS_ENTER_CRITICAL(__os_sr)          \
        do {                                \
            __os_sr = _mips_intdisable();   \
        } while(0)
#define OS_EXIT_CRITICAL(__os_sr)           _mips_intrestore(__os_sr)
#define OS_IS_CRITICAL()                    ((mips_getsr() & 1) == 0)
#define OS_ASSERT_CRITICAL()                assert(OS_IS_CRITICAL())

static inline int
os_arch_in_isr(void)
{
    /* check the EXL bit */
    return (mips_getsr() & (1 << 1)) ? 1 : 0;
}

/* Include common arch definitions and APIs */
#include "os/arch/common.h"

#endif /* _OS_ARCH_MIPS_H */
