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
#include <hal/hal_bsp.h>
#include <hal/hal_os_tick.h>
#include <mcu/cmsis_nvic.h>
#include "os_priv.h"

/*
 * From HAL_CM33.s
 */
extern void SVC_Handler(void);
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);

/* Initial program status register */
#define INITIAL_xPSR    0x01000000

/*
 * Initial LR indicating basic frame.
 * Only FType bit is used.
 * See ARMv8-M Architecture Ref Manual
 */
#define INITIAL_LR_FPTYPE      0x10;

/*
 * Exception priorities. The higher the number, the lower the priority. A
 * higher priority exception will interrupt a lower priority exception.
 */
#define PEND_SV_PRIO    ((1 << __NVIC_PRIO_BITS) - 1)
#define OS_TICK_PRIO    (PEND_SV_PRIO - 1)

/* Make the SVC instruction highest priority */
#define SVC_PRIO        (1)

/* Stack frame structure */
struct stack_frame {
#if MYNEWT_VAL(HARDFLOAT)
    uint32_t    exc_lr;
#endif
    uint32_t    r4;
    uint32_t    r5;
    uint32_t    r6;
    uint32_t    r7;
    uint32_t    r8;
    uint32_t    r9;
    uint32_t    r10;
    uint32_t    r11;
    uint32_t    r0;
    uint32_t    r1;
    uint32_t    r2;
    uint32_t    r3;
    uint32_t    r12;
    uint32_t    lr;
    uint32_t    pc;
    uint32_t    xpsr;
};

#define SVC_ArgN(n) \
  register int __r##n __asm("r"#n);

#define SVC_Arg0()  \
  SVC_ArgN(0)       \
  SVC_ArgN(1)       \
  SVC_ArgN(2)       \
  SVC_ArgN(3)

#if (defined (__CORTEX_M0)) || defined (__CORTEX_M0PLUS)
#define SVC_Call(f)                                                     \
  __asm volatile                                                        \
  (                                                                     \
    "ldr r7,="#f"\n\t"                                                  \
    "mov r12,r7\n\t"                                                    \
    "svc 0"                                                             \
    :               "=r" (__r0), "=r" (__r1), "=r" (__r2), "=r" (__r3)  \
    :                "r" (__r0),  "r" (__r1),  "r" (__r2),  "r" (__r3)  \
    : "r7", "r12", "lr", "cc"                                           \
  );
#else
#define SVC_Call(f)                                                     \
  __asm volatile                                                        \
  (                                                                     \
    "ldr r12,="#f"\n\t"                                                 \
    "svc 0"                                                             \
    :               "=r" (__r0), "=r" (__r1), "=r" (__r2), "=r" (__r3)  \
    :                "r" (__r0),  "r" (__r1),  "r" (__r2),  "r" (__r3)  \
    : "r12", "lr", "cc"                                                 \
  );
#endif

/* XXX: determine how we will deal with running un-privileged */
uint32_t os_flags = OS_RUN_PRIV;

void
timer_handler(void)
{
    os_time_advance(1);
}

void
os_arch_ctx_sw(struct os_task *t)
{
    os_sched_ctx_sw_hook(t);

    /* Set PendSV interrupt pending bit to force context switch */
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

os_sr_t
os_arch_save_sr(void)
{
    uint32_t isr_ctx;

#if MCU_CRITICAL_BASEPRI
    isr_ctx = __get_BASEPRI();
    __set_BASEPRI((MCU_CRITICAL_BASEPRI) << (8 - __NVIC_PRIO_BITS));
#else
    isr_ctx = __get_PRIMASK() & 1;
    __disable_irq();
#endif

    return isr_ctx;
}

void
os_arch_restore_sr(os_sr_t isr_ctx)
{
#if MCU_CRITICAL_BASEPRI
    __set_BASEPRI(isr_ctx);
#else
    if (!isr_ctx) {
        __enable_irq();
    }
#endif
}

int
os_arch_in_critical(void)
{
    int ret;

#if MCU_CRITICAL_BASEPRI
    ret = __get_BASEPRI() > 0;
#else
    ret = __get_PRIMASK() & 1;
#endif

    return ret;
}

static void
os_arch_task_return_handler(void)
{
    /*
     * If you are stuck here it means that task finished by
     * simple return which is not supported.
     */
    while (1);
}

os_stack_t *
os_arch_task_stack_init(struct os_task *t, os_stack_t *stack_top, int size)
{
    int i;
    os_stack_t *s;
    struct stack_frame *sf;

    /* Get stack frame pointer */
    s = (os_stack_t *) ((uint8_t *) stack_top - sizeof(*sf));

    /* Zero out R1-R3, R12 */
    for (i = 9; i < 13; ++i) {
        s[i] = 0;
    }

    /* Set registers R4 - R11 on stack. */
    os_arch_init_task_stack(s);

    /* Set remaining portions of stack frame */
    sf = (struct stack_frame *) s;
    sf->xpsr = INITIAL_xPSR;
    sf->pc = (uint32_t)t->t_func;
    sf->r0 = (uint32_t)t->t_arg;
    /* Set function to cache returns from tasks. */
    sf->lr = (uint32_t)os_arch_task_return_handler;
#if MYNEWT_VAL(HARDFLOAT)
    sf->exc_lr = INITIAL_LR_FPTYPE;
#endif

    return (s);
}

void
os_arch_init(void)
{
    /*
     * Trap on divide-by-zero.
     */
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
    os_init_idle_task();
}

__attribute__((always_inline))
static inline void
svc_os_arch_init(void)
{
    SVC_Arg0();
    SVC_Call(os_arch_init);
}

os_error_t
os_arch_os_init(void)
{
    os_error_t err;
    int i;

    /* Cannot be called within an ISR */
    err = OS_ERR_IN_ISR;
    if (__get_IPSR() == 0) {
        err = OS_OK;

        /* Drop priority for all interrupts */
        for (i = 0; i < sizeof(NVIC->IPR); i++) {
            NVIC->IPR[i] = -1;
        }

        /*
         * Install default interrupt handler for all interrupts except Reset,
         * which'll print out system state at the time of the interrupt, and
         * few other regs which should help in trying to figure out what went
         * wrong.
         */
        for (i = -NVIC_USER_IRQ_OFFSET + 2;
             i < NVIC_NUM_VECTORS - NVIC_USER_IRQ_OFFSET; i++) {
            NVIC_SetVector(i, (uint32_t)os_default_irq_asm);
        }

        /* Install our system interrupt handlers */
        NVIC_SetVector(SVCall_IRQn, (uint32_t)SVC_Handler);
        NVIC_SetVector(PendSV_IRQn, (uint32_t)PendSV_Handler);
        NVIC_SetVector(SysTick_IRQn, (uint32_t)SysTick_Handler);

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

    return err;
}

uint32_t
os_arch_start(void)
{
    struct os_task *t;

    /* Get the highest priority ready to run to set the current task */
    t = os_sched_next_task();
    os_sched_set_current_task(t);

    /* Adjust PSP so it looks like this task just took an exception */
    __set_PSP((uint32_t)t->t_stackptr + offsetof(struct stack_frame, r0));

    /* Intitialize and start system clock timer */
    os_tick_init(OS_TICKS_PER_SEC, OS_TICK_PRIO);

    /* Mark the OS as started, right before we run our first task */
    g_os_started = 1;

    /* Perform context switch */
    os_arch_ctx_sw(t);

    return (uint32_t)(t->t_arg);
}

__attribute__((always_inline))
static inline void svc_os_arch_start(void)
{
    SVC_Arg0();
    SVC_Call(os_arch_start);
}

/**
 * Start the OS. First check to see if we are running with the correct stack
 * pointer set (PSP) and privilege mode (PRIV).
 *
 *
 * @return os_error_t
 */
os_error_t
os_arch_os_start(void)
{
    os_error_t err;

    /*
     * Set the os environment. This will set stack pointers and, based
     * on the contents of os_flags, will determine if the tasks run in
     * privileged or un-privileged mode.
     *
     * We switch to using "empty" part of idle task's stack until
     * the svc_os_arch_start() executes SVC, and we will never return.
     */
    os_set_env(g_idle_task.t_stackptr - 1);

    err = OS_ERR_IN_ISR;
    if (__get_IPSR() == 0) {
        /*
         * The following switch statement is really just a sanity check to
         * insure that the os initialization routine was called prior to the
         * os start routine.
         */
        err = OS_OK;
        switch (__get_CONTROL() & 0x03) {
        /*
         * These two cases are for completeness. Thread mode should be set
         * to use PSP already.
         *
         * Fall-through intentional!
         */
        case 0x00:
        case 0x01:
            err = OS_ERR_PRIV;
            break;
        case 0x02:
            /*
             * We are running in Privileged Thread mode w/SP = PSP but we
             * are supposed to be un-privileged.
             */
            if ((os_flags & 1) == OS_RUN_UNPRIV) {
                err = OS_ERR_PRIV;
            }
            break;
        case 0x03:
            /*
             * We are running in Unprivileged Thread mode w/SP = PSP but we
             * are supposed to be privileged.
             */
            if  ((os_flags & 1) == OS_RUN_PRIV) {
                err = OS_ERR_PRIV;
            }
            break;
        }
        if (err == OS_OK) {
            /* Always start OS through SVC call */
            svc_os_arch_start();
        }
    }

    return err;
}
