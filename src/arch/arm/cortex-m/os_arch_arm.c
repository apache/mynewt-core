/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "os/os.h"
#include "os/os_arch.h"

/* XXX: How do we know which cortex to use? */
#define __CMSIS_GENERIC
#include "cmsis-core/core_cm4.h"

/* XXX: not sure how much of the intrinsic code is specific to M4. */

/* XXX: this is cortex-m specific; not sure if applies to M0-M4. */
#define INITIAL_xPSR    0x01000000

/* XXX: This is cortex-m specific stuff. Not sure if applies to M0-M4 */
struct stack_frame {
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

int     die_line;
char    *die_module;

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

/* WWW: remove this or do this better */
uint32_t os_flags = OS_RUN_PRIV;

void
timer_handler(void)
{
    os_time_tick();
    os_callout_tick();
    os_sched(NULL, 1); 
}

void
os_arch_ctx_sw(struct os_task *t)
{
    os_bsp_ctx_sw();
}

os_sr_t 
os_arch_save_sr(void)
{
    uint32_t isr_ctx;

    isr_ctx = __get_PRIMASK();
    __disable_irq();
    return (isr_ctx & 1); 
}

void
os_arch_restore_sr(os_sr_t isr_ctx)
{
    if (!isr_ctx) {
        __enable_irq();
    }
}

void
_Die(char *file, int line)
{
    die_line = line;
    die_module = file;
    while (1) {
    }
}

os_stack_t *
os_arch_task_stack_init(struct os_task *t, os_stack_t *stack_top, int size) 
{
    int i;
    os_stack_t *s;
    struct stack_frame *sf;

    /* Get stack frame pointer */
    s = (os_stack_t *) ((uint8_t *) stack_top - sizeof(*sf));

    /* Zero out R4-R11, R0-R3, R12, LR */
    for (i = 0; i < 14; ++i) {
        s[i] = 0;
    }

    /* Set remaining portions of stack frame */
    sf = (struct stack_frame *) s;
    sf->xpsr = INITIAL_xPSR;
    sf->pc = (uint32_t)t->t_func; 
    sf->r0 = (uint32_t)t->t_arg;

    return (s);
}

void
os_arch_init(void)
{
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

    /* Cannot be called within an ISR */
    err = OS_ERR_IN_ISR;
    if (__get_IPSR() == 0) {
        err = OS_OK;

        /* Call bsp related OS initializations */
        os_bsp_init();

        /* 
         * Set the os environment. This will set stack pointers and, based
         * on the contents of os_flags, will determine if the tasks run in
         * priviliged or un-privileged mode.
         */
        os_set_env();

        /* XXX: the code for os_set_env() is in the .s file. NOTE: there are
         * no DSB/ISB instructions in that code but there are in the old
         * os_start code. Why? Do I need them in the assembly code? Read up
           on this!!! */

        /* Check if priviliged or not */
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
    t = os_sched_next_task(0);
    os_sched_set_current_task(t);

    /* Adjust PSP so it looks like this task just took an exception */
    __set_PSP((uint32_t)t->t_stackptr + offsetof(struct stack_frame, r0));

    /* Intitialize and start system clock timer */
    os_bsp_systick_init(OS_TIME_TICK * 1000);

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

os_error_t 
os_arch_os_start(void)
{
    os_error_t err;

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
            /* Privileged Thread mode w/SP = PSP */
            if ((os_flags & 1) == 0) {
                err = OS_ERR_PRIV;
            }
            break;
        case 0x03:
            /* Unpriviliged thread mode w/sp = PSP */
            if  (os_flags & 1) {
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

