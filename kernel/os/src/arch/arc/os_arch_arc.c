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

#include <string.h>
#include "os/mynewt.h"
#include "inc/arc/arc_builtin.h"
#include "inc/arc/arc_exception.h"
#include <hal/hal_bsp.h>
#include <hal/hal_os_tick.h>
#include "os_priv.h"

extern uint32_t _f_sdata;
extern uint32_t _e_stack;

#define INITIAL_STATUS32    (0x80000000 | STATUS32_RESET_VALUE |            \
                             (((INT_PRI_MAX - INT_PRI_MIN) << 1) & 0x1e))

/* Stack frame structure */
struct stack_frame {
#ifndef ARC_FEATURE_RF16
    uint32_t    r25;
    uint32_t    r24;
    uint32_t    r23;
    uint32_t    r22;
    uint32_t    r21;
    uint32_t    r20;
    uint32_t    r19;
    uint32_t    r18;
    uint32_t    r17;
    uint32_t    r16;
#endif
    uint32_t    r15;
    uint32_t    r14;
    uint32_t    r13;
    uint32_t    bta;
    uint32_t    r30;
    uint32_t    ilink;
    uint32_t    fp;
    uint32_t    gp;
    uint32_t    r12;

    uint32_t    r0;
    uint32_t    r1;
    uint32_t    r2;
    uint32_t    r3;
#ifndef ARC_FEATURE_RF16
    uint32_t    r4;
    uint32_t    r5;
    uint32_t    r6;
    uint32_t    r7;
    uint32_t    r8;
    uint32_t    r9;
#endif
    uint32_t    r10;
    uint32_t    r11;
    uint32_t    blink;              /* R31 */
    uint32_t    lp_end;
    uint32_t    lp_start;
    uint32_t    lp_count;
#ifdef ARC_FEATURE_CODE_DENSITY
    uint32_t    ei_base;
    uint32_t    ldi_base;
    uint32_t    jli_base;
#endif
    uint32_t    pc;
    uint32_t    status32;
};

/* TODO: determine how we will deal with running un-privileged */
//uint32_t os_flags = OS_RUN_KERNEL;

void
timer_handler(void)
{
    os_time_advance(1);
}

void
os_arch_ctx_sw(struct os_task *t)
{
    os_sched_ctx_sw_hook(t);

    if (!exc_sense()) {
        asm("TRAP_S 1");
    }
}

os_sr_t
os_arch_save_sr(void)
{
    os_sr_t sreg;

    sreg = cpu_lock_save();

    return sreg;
}

void
os_arch_restore_sr(os_sr_t isr_ctx)
{
    cpu_unlock_restore(isr_ctx);
}

int
os_arch_in_critical(void)
{
    int status;

    status = (_arc_aux_read(AUX_STATUS32) & AUX_STATUS_MASK_IE) ? 0: 1;
    return status;
}

os_stack_t *
os_arch_task_stack_init(struct os_task *t, os_stack_t *stack_top, int size)
{
    os_stack_t *s;
    uint32_t *tmp;
    uint32_t init_val;
    struct stack_frame *sf;

    /* Get stack frame pointer */
    s = (os_stack_t *) ((uint8_t *) stack_top - sizeof(*sf));
    sf = (struct stack_frame *) s;

    /* Initially set all values to 0 */
    memset(sf, 0, sizeof(struct stack_frame));

#ifndef ARC_FEATURE_RF16
    /*
     * Init r25 - r16. We set each byte in the stack equal to its register
     * number, in hex unless that register has a special purpose. For
     * example, r25 = 0x25252525 and r3 = 0x03030303
     */
    init_val = 0x25252525;
    tmp = s;
    while (init_val >= 0x16161616) {
        *tmp = init_val;
        ++tmp;
        if (init_val == 0x20202020) {
            init_val = 0x19191919;
        } else {
            init_val -= 0x01010101;
        }
    }

    /* Init r4-r9 the same way */
    init_val = 0x04040404;
    tmp = (uint32_t *)&sf->r4;
    while (init_val <= 0x09090909) {
        *tmp = init_val;
        ++tmp;
        init_val += 0x01010101;
    }
#endif

    sf->r15 = 0x15151515;
    sf->r14 = 0x14141414;
    sf->r13 = 0x13131313;
    sf->r11 = 0x12121212;
    sf->r11 = 0x11111111;
    sf->r10 = 0x10101010;
    sf->r30 = 0x30303030;
    sf->r3 = 0x03030303;
    sf->r2 = 0x02020202;
    sf->r1 = 0x01010101;
    sf->r0 = (uint32_t)t->t_arg;
    sf->pc = (uint32_t)t->t_func;
    sf->status32 = INITIAL_STATUS32;    /* this is the top of the stack */

    /* We should never return! Hopefully this will generate an exception */
    sf->blink = 0xffffffff;
    sf->gp = (uint32_t)&_f_sdata;


    /* TODO: Do we need to do this? */
#if 0
    /* Set registers R4 - R11 on stack. */
    os_arch_init_task_stack(s);
#endif

    return (s);
}

/**
 * os arch start
 *
 * Starts the OS. On ARC platforms this is called from the TRAP exception
 * handler.
 *
 * @return uint32_t
 */
static void
os_arch_start(void)
{
    uint32_t *ptr32;
    struct os_task *t;

    /* Get the highest priority ready to run to set the current task */
    t = os_sched_next_task();
    os_sched_set_current_task(t);

    /* TODO: hacked priority here to 0 */
    /* Intitialize and start system clock timer */
    os_tick_init(OS_TICKS_PER_SEC, 0);

    /* Mark the OS as started, right before we run our first task */
    g_os_started = 1;

    /*
     * OK, here is what this code is doing. We took an exception (a TRAP) to
     * get here. All tasks that have been created have their stack pointers
     * (t->stackptr) set to where they would be if they were swapped out (in
     * other words, not running). At the beginning of this function we set
     * the current task to the head of the run list. This makes it look like
     * the task at the head of the run list was the one we took the exception
     * from. In other words, no task swap is needed. In the case the stack
     * pointer in the task structure is off by the CALLEE registers as they
     * would not have been pushed if the exception had actually taken place
     * during the currently running task. The exception code pushes the
     * current stack pointer to the top of the exception stack. This basically
     * does this: *ptr = stack pointer where ptr = &_e_stack - 1 word. Thus,
     * we need to overwrite the value at _e_stack - 4 with where the stack
     * pointer in the task we want run would have been had an exception taken
     * place when that task was running (add back the callee regs).
     */
    ptr32 = &_e_stack;
    --ptr32;
    *ptr32 = (uint32_t)t->t_stackptr + offsetof(struct stack_frame, bta);
}

/**
 * os arch os start
 *
 * This gets called in _start prior to the os starting. For the ARC platform,
 * we are currently not running in any task and the stack pointer being
 * used is the exception stack pointer.
 *
 * @return os_error_t
 */
os_error_t
os_arch_os_start(void)
{
    /* Cause a trap with reason 0 (os start) */
    asm("TRAP_S 0");
    return 0;
}

void
os_arch_trap_handler(void *exc_frame)
{
    uint32_t parameter;

    (void)exc_frame;

    /*
     * Currently, only two traps should be generated: one to perform a context
     * switch when a context switch was asked for at task (not interrupt)
     * context.
     */
    parameter = _arc_aux_read(AUX_ECR) & AUX_ECR_PARAM_MASK;
    if (parameter == 0) {
        os_arch_start();
    }
}

os_error_t
os_arch_os_init(void)
{
    /* TODO: deal with this */
#if 0
    os_error_t err;
    int i;

    /* Cannot be called within an ISR */
    err = OS_ERR_IN_ISR;
    if (__get_IPSR() == 0) {
        err = OS_OK;

        /* Drop priority for all interrupts */
        for (i = 0; i < sizeof(NVIC->IP); i++) {
            NVIC->IP[i] = -1;
        }

        NVIC_SetVector(SVCall_IRQn, (uint32_t)SVC_Handler);
        NVIC_SetVector(PendSV_IRQn, (uint32_t)PendSV_Handler);
        NVIC_SetVector(SysTick_IRQn, (uint32_t)SysTick_Handler);

        /*
         * Install default interrupt handler, which'll print out system
         * state at the time of the interrupt, and few other regs which
         * should help in trying to figure out what went wrong.
         */
        NVIC_SetVector(NonMaskableInt_IRQn, (uint32_t)os_default_irq_asm);
        NVIC_SetVector(-13, (uint32_t)os_default_irq_asm);
        NVIC_SetVector(MemoryManagement_IRQn, (uint32_t)os_default_irq_asm);
        NVIC_SetVector(BusFault_IRQn, (uint32_t)os_default_irq_asm);
        NVIC_SetVector(UsageFault_IRQn, (uint32_t)os_default_irq_asm);
        for (i = 0; i < NVIC_NUM_VECTORS - NVIC_USER_IRQ_OFFSET; i++) {
            NVIC_SetVector(i, (uint32_t)os_default_irq_asm);
        }

        /* Set the PendSV interrupt exception priority to the lowest priority */
        NVIC_SetPriority(PendSV_IRQn, PEND_SV_PRIO);

        /* Set the SVC interrupt to priority 0 (highest configurable) */
        NVIC_SetPriority(SVCall_IRQn, SVC_PRIO);

        /* Check if privileged or not */
        if ((__get_CONTROL() & 1) == 0) {
            os_arch_init();
        } else {
            svc_os_arch_init();
        }
    }
#else
    /* Set the trap handler exception */
    exc_handler_install(AUX_ECR_V_TRAP, os_arch_trap_handler);

    /* Init the idle task */
    os_init_idle_task();
#endif

    return 0;
}
