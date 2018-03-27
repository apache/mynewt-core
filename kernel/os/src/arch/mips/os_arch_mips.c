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

#include "os/mynewt.h"
#include <hal/hal_bsp.h>
#include <hal/hal_os_tick.h>

#include "os_priv.h"

#include <mips/hal.h>
#include <mips/uhi_syscalls.h>

#include <string.h>

extern void SVC_Handler(void);
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);

/* XXX: determine how to deal with running un-privileged */
/* only priv currently supported */
uint32_t os_flags = OS_RUN_PRIV;

extern struct os_task g_idle_task;

/* core timer interrupt */
void __attribute__((interrupt, keep_interrupts_masked))
_mips_isr_hw5(void)
{
    unsigned long int inc = (MYNEWT_VAL(CLOCK_FREQ) / 2) / OS_TICKS_PER_SEC;
    unsigned long int compare = mips_getcompare();
    mips_setcompare(compare + inc);
    timer_handler();
}

static int
os_in_isr(void)
{
    /* check the EXL bit */
    return (mips_getsr() & (1 << 1)) ? 1 : 0;
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

    /* trigger sw interrupt */
    mips_biscr(1 << 8);
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

reg_t get_global_pointer(void);

/* assumes stack_top will be 8 aligned */

os_stack_t *
os_arch_task_stack_init(struct os_task *t, os_stack_t *stack_top, int size)
{
    os_stack_t *s = stack_top - ((((sizeof(struct gpctx) - 1) /
        OS_STACK_ALIGNMENT) + 1) * (OS_STACK_ALIGNMENT/sizeof(os_stack_t)));

    struct gpctx ctx = {
        .r = {
            [3] = (reg_t)t->t_arg,
            [27] = get_global_pointer(),
            [28] = (reg_t)(stack_top - 4)
        },
        .status = mips_getsr(),
        .cause = mips_getcr(),
        .epc = (reg_t)t->t_func
    };

    /* copy struct onto the stack */
    memcpy(s, &ctx, sizeof(struct gpctx));

    return (s);
}

void
os_arch_init(void)
{
    /* enable software interrupt 0 */
    mips_bissr((1 << 15) | (1 << 8));
    os_init_idle_task();
}

os_error_t
os_arch_os_init(void)
{
    os_error_t err;

    err = OS_ERR_IN_ISR;
    if (os_in_isr() == 0) {
        err = OS_OK;

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
    mips_setcompare(mips_getcount() + ((MYNEWT_VAL(CLOCK_FREQ) / 2) / 1000));

    /* global interrupt enable */
    mips_bissr(1);

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
