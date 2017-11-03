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

#include <hal/hal_system.h>
#ifdef COREDUMP_PRESENT
#include <coredump/coredump.h>
#endif
#include "os/os.h"

#include <stdint.h>
#include <unistd.h>

struct exception_frame {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
};

struct trap_frame {
    struct exception_frame *ef;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t lr;    /* this LR holds EXC_RETURN */
};

struct coredump_regs {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t r12;
    uint32_t sp;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
};

#ifdef COREDUMP_PRESENT
static void
trap_to_coredump(struct trap_frame *tf, struct coredump_regs *regs)
{
}
#endif

void
__assert_func(const char *file, int line, const char *func, const char *e)
{
    int sr;
    OS_ENTER_CRITICAL(sr);
    (void)sr;
    hal_system_reset();
}

void
os_default_irq(struct trap_frame *tf)
{
#ifdef COREDUMP_PRESENT
    struct coredump_regs regs;
#endif

#ifdef COREDUMP_PRESENT
    trap_to_coredump(tf, &regs);
    coredump_dump(&regs, sizeof(regs));
#endif
    hal_system_reset();
}
