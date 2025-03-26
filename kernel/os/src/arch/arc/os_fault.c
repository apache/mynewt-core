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
#include "console/console.h"
#include "hal/hal_system.h"
#include "os_priv.h"

#if MYNEWT_VAL(OS_COREDUMP)
#include "coredump/coredump.h"
#endif

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

#if MYNEWT_VAL(OS_COREDUMP) && !MYNEWT_VAL(OS_COREDUMP_CB)
static void
trap_to_coredump(struct trap_frame *tf, struct coredump_regs *regs)
{
/* TODO: handle coredumps */
#if 0
    regs->r0 = tf->ef->r0;
    regs->r1 = tf->ef->r1;
    regs->r2 = tf->ef->r2;
    regs->r3 = tf->ef->r3;
    regs->r4 = tf->r4;
    regs->r5 = tf->r5;
    regs->r6 = tf->r6;
    regs->r7 = tf->r7;
    regs->r8 = tf->r8;
    regs->r9 = tf->r9;
    regs->r10 = tf->r10;
    regs->r11 = tf->r11;
    regs->r12 = tf->ef->r12;
    /*
     * SP just before exception for the coredump.
     * See ARMv7-M Architecture Ref Manual, sections B1.5.6 - B1.5.8
     * If floating point registers were pushed to stack, SP is adjusted
     * by 0x68.
     * Otherwise, SP is adjusted by 0x20.
     * If SCB->CCR.STKALIGN is set, or fpu is active, SP is aligned to
     * 8-byte boundary on exception entry.
     * If this alignment adjustment happened, xPSR will have bit 9 set.
     */
    if ((tf->lr & 0x10) == 0) {
        /*
         * Extended frame
         */
        regs->sp = ((uint32_t)tf->ef) + 0x68;
        if (tf->ef->psr & (1 << 9)) {
            regs->sp += 4;
        }
    } else {
        regs->sp = ((uint32_t)tf->ef) + 0x20;
        if ((SCB->CCR & SCB_CCR_STKALIGN_Msk) & tf->ef->psr & (1 << 9)) {
            regs->sp += 4;
        }
    }
    regs->lr = tf->ef->lr;
    regs->pc = tf->ef->pc;
    regs->psr = tf->ef->psr;
#else
#endif
}
#endif

void
__assert_func(const char *file, int line, const char *func, const char *e)
{
    int sr;

    OS_ENTER_CRITICAL(sr);
    (void)sr;
    console_blocking_mode();
    OS_PRINT_ASSERT(file, line, func, e);

#if MYNEWT_VAL(OS_ASSERT_CB)
    os_assert_cb(file, line, func, e);
#endif
    if (hal_debugger_connected()) {
       /*
        * If debugger is attached, breakpoint before the trap.
        */
       asm("BRK_S");
    }
/* TODO: Determine how this will be done */
#if 0
    SCB->ICSR = SCB_ICSR_NMIPENDSET_Msk;
    asm("isb");
#endif
    hal_system_reset();
}

void
os_default_irq(struct trap_frame *tf)
{
#if MYNEWT_VAL(OS_COREDUMP) && !MYNEWT_VAL(OS_COREDUMP_CB)
    struct coredump_regs regs;
#endif

    console_blocking_mode();
    console_printf("Unhandled interrupt , exception sp 0x%08lx\n",
                   (uint32_t)tf->ef);
    console_printf(" r0:0x%08lx  r1:0x%08lx  r2:0x%08lx  r3:0x%08lx\n",
      tf->ef->r0, tf->ef->r1, tf->ef->r2, tf->ef->r3);
    console_printf(" r4:0x%08lx  r5:0x%08lx  r6:0x%08lx  r7:0x%08lx\n",
      tf->r4, tf->r5, tf->r6, tf->r7);
    console_printf(" r8:0x%08lx  r9:0x%08lx r10:0x%08lx r11:0x%08lx\n",
      tf->r8, tf->r9, tf->r10, tf->r11);
    console_printf("r12:0x%08lx  lr:0x%08lx  pc:0x%08lx psr:0x%08lx\n",
      tf->ef->r12, tf->ef->lr, tf->ef->pc, tf->ef->psr);

#if MYNEWT_VAL(OS_COREDUMP)
#if MYNEWT_VAL(OS_COREDUMP_CB)
    os_coredump_cb(tf);
#else
    trap_to_coredump(tf, &regs);
    coredump_dump(&regs, sizeof(regs));
#endif
#endif
    hal_system_reset();
}
