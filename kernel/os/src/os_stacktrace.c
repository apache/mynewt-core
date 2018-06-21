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

#include "os/mynewt.h"

#if MYNEWT_VAL(OS_CRASH_STACKTRACE)
#include <hal/hal_bsp.h>
#include <console/console.h>

#define OS_STACK_DEPTH_MAX      1024

/*
 * Is address within text region?
 */
static int
os_addr_is_text(uintptr_t addr)
{
    extern void *__text;
    extern void *__etext;
    uintptr_t start = (uintptr_t)&__text;
    uintptr_t end = (uintptr_t)&__etext;

    /*
     * Assumes all text is contiguous. XXX split images and architectures where
     * this is not the case.
     */
    if (addr >= start && addr < end) {
        return 1;
    }
    return 0;
}

/*
 * Is address within area where stack could be?
 */
static int
os_addr_is_ram(uintptr_t addr)
{
    const struct hal_bsp_mem_dump *mem;
    const struct hal_bsp_mem_dump *cur;
    int mem_cnt;
    int i;

    mem = hal_bsp_core_dump(&mem_cnt); /* this returns ram regions as array */
    for (i = 0; i < mem_cnt; i++) {
        cur = &mem[i];
        if (addr >= (uintptr_t)cur->hbmd_start &&
            addr < (uintptr_t)cur->hbmd_start + cur->hbmd_size) {
            return 1;
        }
    }
    return 0;
}

/**
 * Print addresses from stack which look like they might be instruction
 * pointers.
 */
void
os_stacktrace(uintptr_t sp)
{
    uintptr_t addr;
    uintptr_t end;
    struct os_task *t;

    sp &= ~(sizeof(uintptr_t) - 1);
    end = sp + OS_STACK_DEPTH_MAX;

    if (g_os_started && g_current_task) {
        t = g_current_task;
        if (sp > (uintptr_t)t->t_stacktop && end > (uintptr_t)t->t_stacktop) {
            end = (uintptr_t)t->t_stacktop;
        }
    } else {
        t = NULL;
    }
    console_printf("task:%s\n", t ? t->t_name : "NA");
    for ( ; sp < end; sp += sizeof(uintptr_t)) {
        if (os_addr_is_ram(sp)) {
            addr = *(uintptr_t *)sp;
            if (os_addr_is_text(addr)) {
                console_printf(" 0x%08lx: 0x%08lx\n",
                               (unsigned long)sp, (unsigned long)addr);
            }
        }
    }
}
#endif


