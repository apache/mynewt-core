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

#include "os/os.h"
#include "os/os_arch.h"
#include "syscfg/syscfg.h"
#include <hal/hal_bsp.h>
#include <hal/hal_os_tick.h>

#include "os_priv.h"

#include <string.h>

#define OS_TICK_PERIOD ((MYNEWT_VAL(CLOCK_FREQ) / 2) / OS_TICKS_PER_SEC)

extern void SVC_Handler(void);
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);

#if MYNEWT_VAL(HARDFLOAT)
struct ctx_fp {
    uint32_t regs[32];
    uint32_t fcsr;
};
#endif

struct ctx {
    uint32_t regs[30];
    uint32_t epc;
    uint32_t badvaddr;
    uint32_t status;
    uint32_t cause;
#if (__mips_isa_rev < 6)
    uint32_t lo;
    uint32_t hi;
#endif
};

/* XXX: determine how to deal with running un-privileged */
/* only priv currently supported */
uint32_t os_flags = OS_RUN_PRIV;

extern struct os_task g_idle_task;

struct os_task *g_fpu_task;

struct os_task_t* g_fpu_user;

/* core timer interrupt */
void __attribute__((interrupt(IPL1AUTO),
vector(_CORE_TIMER_VECTOR))) isr_core_timer(void)
{
    timer_handler();
    _CP0_SET_COMPARE(_CP0_GET_COMPARE() + OS_TICK_PERIOD);
    IFS0CLR = _IFS0_CTIF_MASK;
}

/* context switch interrupt, in ctx.S */
void
__attribute__((interrupt(IPL1AUTO), vector(_CORE_SOFTWARE_0_VECTOR)))
isr_sw0(void);

static int
os_in_isr(void)
{
    /* check the EXL bit */
    return (_CP0_GET_STATUS() & _CP0_STATUS_EXL_MASK) ? 1 : 0;
}

void
timer_handler(void)
{
    os_time_advance(1);
}

void
os_arch_ctx_sw(struct os_task *t)
{
    if ((os_sched_get_current_task() != 0) && (t != 0)) {
        os_sched_ctx_sw_hook(t);
    }

    IFS0SET = _IFS0_CS0IF_MASK;
}

os_sr_t
os_arch_save_sr(void)
{
    os_sr_t sr;
    OS_ENTER_CRITICAL(sr);
    return sr;
}

void
os_arch_restore_sr(os_sr_t isr_ctx)
{
    OS_EXIT_CRITICAL(isr_ctx);
}

int
os_arch_in_critical(void)
{
    return OS_IS_CRITICAL();
}

uint32_t get_global_pointer(void);

static inline int
os_bytes_to_stack_aligned_words(int byts) {
    return (((byts - 1) / OS_STACK_ALIGNMENT) + 1) *
        (OS_STACK_ALIGNMENT/sizeof(os_stack_t));
}

/* assumes stack_top will be 8 aligned */

os_stack_t *
os_arch_task_stack_init(struct os_task *t, os_stack_t *stack_top, int size)
{
    int ctx_space = os_bytes_to_stack_aligned_words(sizeof(struct ctx));
#if MYNEWT_VAL(HARDFLOAT)
    /* If stack does not have space for the FPU context, assume the
    thread won't use it. */
    int lazy_space = os_bytes_to_stack_aligned_words(sizeof(struct ctx_fp));
    if ((lazy_space + ctx_space + 4) >= (size * sizeof(os_stack_t))) {
        /* stack too small */
        stack_top -= 4;
    } else {
        struct ctx_fp ctx_fp;
        ctx_fp.fcsr = 0;
        memcpy(stack_top -
            os_bytes_to_stack_aligned_words(sizeof(struct ctx_fp)),
            &ctx_fp, sizeof(ctx_fp));
        stack_top -= lazy_space + 4;
    }
#else
    stack_top -= 4;
#endif

    os_stack_t *s = stack_top - ctx_space;

    struct ctx ctx;
    ctx.regs[3] = (uint32_t)t->t_arg;
    ctx.regs[27] = get_global_pointer();
    ctx.status = (_CP0_GET_STATUS() & ~_CP0_STATUS_CU1_MASK) | _CP0_STATUS_IE_MASK;
    ctx.cause = _CP0_GET_CAUSE();
    ctx.epc = (uint32_t)t->t_func;
    /* copy struct onto the stack */
    memcpy(s, &ctx, sizeof(ctx));

    return stack_top;
}

void
os_arch_init(void)
{
    os_init_idle_task();
}

os_error_t
os_arch_os_init(void)
{
    os_error_t err;

    err = OS_ERR_IN_ISR;
    if (os_in_isr() == 0) {
        err = OS_OK;
        os_sr_t sr;
        OS_ENTER_CRITICAL(sr);

        _CP0_BIC_STATUS(_CP0_STATUS_IPL_MASK);
        /* multi vector mode */
        INTCONSET = _INTCON_MVEC_MASK;
        /* vector spacing 0x20  */
        _CP0_SET_INTCTL(_CP0_GET_INTCTL() | (1 << _CP0_INTCTL_VS_POSITION));

        /* enable core timer interrupt */
        IEC0SET = _IEC0_CTIE_MASK;
        /* set interrupt priority */
        IPC0CLR = _IPC0_CTIP_MASK;
        IPC0SET = (1 << _IPC0_CTIP_POSITION); /* priority 1 */
        /* set interrupt subpriority */
        IPC0CLR = _IPC0_CTIS_MASK;
        IPC0SET = (0 << _IPC0_CTIS_POSITION); /* subpriority 0 */

        /* enable software interrupt 0 */
        IEC0SET = _IEC0_CS0IE_MASK;
        /* set interrupt priority */
        IPC0CLR = _IPC0_CS0IP_MASK;
        IPC0SET = (1 << _IPC0_CS0IP_POSITION); /* priority 1 */
        /* set interrupt subpriority */
        IPC0CLR = _IPC0_CS0IS_MASK;
        IPC0SET = (0 << _IPC0_CS0IS_POSITION); /* subpriority 0 */

        OS_EXIT_CRITICAL(sr);

        /* should be in kernel mode here */
        os_arch_init();
    }
    return err;
}

uint32_t
os_arch_start(void)
{
    struct os_task *t;

    /* Get the highest priority ready to run to set the current task */
    t = os_sched_next_task();

    /* set the core timer compare register */
    _CP0_SET_COMPARE(_CP0_GET_COUNT() + OS_TICK_PERIOD);

    /* global interrupt enable */
    __builtin_enable_interrupts();

    /* Mark the OS as started, right before we run our first task */
    g_os_started = 1;

    /* Perform context switch to first task */
    os_arch_ctx_sw(t);

    return (uint32_t)(t->t_arg);
}

os_error_t
os_arch_os_start(void)
{
    os_error_t err;

    err = OS_ERR_IN_ISR;
    if (os_in_isr() == 0) {
        err = OS_OK;
        /* should be in kernel mode here */
        os_arch_start();
    }

    return err;
}
