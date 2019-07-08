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

#include <stdint.h>
#include <unistd.h>

#include "os/mynewt.h"
#include "console/console.h"
#include "hal/hal_system.h"
#include "os_priv.h"

#if MYNEWT_VAL(OS_COREDUMP)
#include "coredump/coredump.h"
#endif

#if MYNEWT_VAL(OS_CRASH_LOG)
#include "reboot/log_reboot.h"
#endif

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

#if MYNEWT_VAL(OS_COREDUMP)
static void
trap_to_coredump(struct trap_frame *tf, struct coredump_regs *regs)
{
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
     * See ARMv8-M Architecture Ref Manual, section E2.1.236
     * If floating point registers were pushed to stack, SP is adjusted
     * by 0x68.
     * If FPPCR.TS is set, treat floating point registers as secure, and
     * SP is adjusted by 0xa8. XXX not dealing with secure mode yet.
     * Otherwise, SP is adjusted by 0x20.
     */
    if ((tf->lr & 0x10) == 0) {
        /*
         * Extended frame
         */
        regs->sp = ((uint32_t)tf->ef) + 0x68;
    } else {
        regs->sp = ((uint32_t)tf->ef) + 0x20;
    }
    regs->lr = tf->ef->lr;
    regs->pc = tf->ef->pc;
    regs->psr = tf->ef->psr;
}
#endif

void
__assert_func(const char *file, int line, const char *func, const char *e)
{
#if MYNEWT_VAL(OS_CRASH_LOG)
    struct log_reboot_info lri;
#endif
    int sr;

    OS_ENTER_CRITICAL(sr);
    (void)sr;
    console_blocking_mode();
    OS_PRINT_ASSERT(file, line, func, e);

#if MYNEWT_VAL(OS_CRASH_LOG)
    lri = (struct log_reboot_info) {
        .reason = HAL_RESET_SOFT,
        .file = file,
        .line = line,
        .pc = (uint32_t)__builtin_return_address(0),
    };
    log_reboot(&lri);
#endif


#if MYNEWT_VAL(OS_ASSERT_CB)
    os_assert_cb();
#endif
    if (hal_debugger_connected()) {
       /*
        * If debugger is attached, breakpoint before the trap.
        */
       asm("bkpt");
    }
    SCB->ICSR = SCB_ICSR_PENDNMISET_Msk;
    asm("isb");
    hal_system_reset();
}

void
os_default_irq(struct trap_frame *tf)
{
#if MYNEWT_VAL(OS_CRASH_LOG)
    struct log_reboot_info lri;
#endif
#if MYNEWT_VAL(OS_COREDUMP)
    struct coredump_regs regs;
#endif
#if MYNEWT_VAL(OS_CRASH_RESTORE_REGS)
    uint32_t orig_sp;
#endif

    console_blocking_mode();
    console_printf("Unhandled interrupt (%ld), exception sp 0x%08lx\n",
      SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk, (uint32_t)tf->ef);
    console_printf(" r0:0x%08lx  r1:0x%08lx  r2:0x%08lx  r3:0x%08lx\n",
      tf->ef->r0, tf->ef->r1, tf->ef->r2, tf->ef->r3);
    console_printf(" r4:0x%08lx  r5:0x%08lx  r6:0x%08lx  r7:0x%08lx\n",
      tf->r4, tf->r5, tf->r6, tf->r7);
    console_printf(" r8:0x%08lx  r9:0x%08lx r10:0x%08lx r11:0x%08lx\n",
      tf->r8, tf->r9, tf->r10, tf->r11);
    console_printf("r12:0x%08lx  lr:0x%08lx  pc:0x%08lx psr:0x%08lx\n",
      tf->ef->r12, tf->ef->lr, tf->ef->pc, tf->ef->psr);
    console_printf("ICSR:0x%08lx HFSR:0x%08lx CFSR:0x%08lx\n",
      SCB->ICSR, SCB->HFSR, SCB->CFSR);
    console_printf("BFAR:0x%08lx MMFAR:0x%08lx\n", SCB->BFAR, SCB->MMFAR);

    os_stacktrace((uintptr_t)(tf->ef + 1));

#if MYNEWT_VAL(OS_CRASH_LOG)
    lri = (struct log_reboot_info) {
        .reason = HAL_RESET_SOFT,
        .file = NULL,
        .line = 0,
        .pc = tf->ef->pc,
    };
    log_reboot(&lri);
#endif

#if MYNEWT_VAL(OS_COREDUMP)
    trap_to_coredump(tf, &regs);
    coredump_dump(&regs, sizeof(regs));
#endif

#if MYNEWT_VAL(OS_CRASH_RESTORE_REGS)
    if (((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) < 16) &&
        hal_debugger_connected()) {

        if ((tf->lr & 0x10) == 0) {
            /*
             * Extended frame
             */
            orig_sp = ((uint32_t)tf->ef) + 0x68;
        } else {
            orig_sp = ((uint32_t)tf->ef) + 0x20;
        }

        console_printf("Use 'set $pc = 0x%08lx' to restore PC in gdb\n",
                       tf->ef->pc);

        __asm volatile (
            "mov sp, %[stack_ptr]\n"
            "mov r0, %[regs1]\n"
            "ldm r0, {r4-r11}\n"
            "mov r0, %[regs2]\n"
            "ldm r0, {r0-r3,r12,lr}\n"
            "bkpt"
            :
            : [regs1] "r" (&tf->r4),
              [regs2] "r" (tf->ef),
              [stack_ptr] "r" (orig_sp)
            : "r0"
        );
    }
#endif

    hal_system_reset();
}
