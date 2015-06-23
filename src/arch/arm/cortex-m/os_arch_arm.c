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

#include "os/cortex-m/core_cmFunc.h"
#include "os/cortex-m/rt_HAL_CM.h"
#include "os/cortex-m/core_cmInstr.h"
#include "os/cortex-m/m4/core_cm4.h"

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

int os_tick_irqn;

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

#define OS_TICK         1000
#define OS_CLOCK        168000000
#define OS_TRV          ((uint32_t)(((double)OS_CLOCK*(double)OS_TICK)/1E6)-1)
uint32_t const os_trv = OS_TRV;

uint32_t os_flags = OS_RUN_PRIV;

__inline static void
rt_systick_init (void)
{
    NVIC_ST_RELOAD  = os_trv;
    NVIC_ST_CURRENT = 0;
    NVIC_ST_CTRL    = 0x0007;
    NVIC_SYS_PRI3  |= 0xFF000000;
}

__inline static void
rt_svc_init (void) 
{
#if !(__TARGET_ARCH_6S_M)
    int sh,prigroup;
#endif
    NVIC_SYS_PRI3 |= 0x00FF0000;
#if (__TARGET_ARCH_6S_M)
    NVIC_SYS_PRI2 |= (NVIC_SYS_PRI3<<(8+1)) & 0xFC000000;
#else
    sh       = 8 - __clz (~((NVIC_SYS_PRI3 << 8) & 0xFF000000));
    prigroup = ((NVIC_AIR_CTRL >> 8) & 0x07);
    if (prigroup >= sh) {
        sh = prigroup + 1;
    }
    NVIC_SYS_PRI2 = ((0xFEFFFFFF << sh) & 0xFF000000) | (NVIC_SYS_PRI2 & 0x00FFFFFF);
#endif
}

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
    /* Set the PendSV interrupt pending to force context switch */
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

os_sr_t 
os_arch_save_sr(void)
{
    uint32_t isr_ctx;

    isr_ctx = __disable_irq();
    return (isr_ctx); 
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

    rt_svc_init();
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
        /* Check if priviliged or not */
        if ((__get_CONTROL() & 1) == 0) {
            os_arch_init();
        } else {
            svc_os_arch_init();
        }
    }

    return err;
}

void
os_arch_start(void)
{
    struct os_task *t;
    //os_sr_t sr;

    //OS_ENTER_CRITICAL(sr);
    t = os_sched_next_task(0);
    os_sched_set_current_task(t);

    /* WWW: might have to modify this to go through SVC so it cant get
       interrupted. ie. make this svc... not sure yet. This should work though 
    */
    __set_PSP((uint32_t)t->t_stackptr + sizeof(struct stack_frame));

    /* Intitialize and start system clock timer */
    rt_systick_init();
    os_tick_irqn = -1;

    t->t_func(t->t_arg);
    //OS_EXIT_CRITICAL(sr);
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
    uint32_t stack[8];

    /* WWW */
    os_set_env();

    err = OS_ERR_IN_ISR;
    if (__get_IPSR() == 0) {
        /* Get the current mode of the processor */
        err = OS_OK;
        switch (__get_CONTROL() & 0x03) {
        case 0x00:
            /* We are running in priviliged Thread mode w/SP = MSP. */
            /* WWW: why are we doing this? */
            __set_PSP((uint32_t)(stack + 8));
            if (os_flags & 1) {
                __set_CONTROL(0x02);
            } else {
                __set_CONTROL(0x03);
            }
            __DSB();
            __ISB();
            break;
        case 0x01:
            /* Unpriviliged Thread mode w/sp = MSP. This is an error */
            err = OS_ERR_PRIV;
        case 0x02:
            /* Privileged Thread mode w/SP = PSP */
            if ((os_flags & 1) == 0) {
              __set_CONTROL(0x03);
              __DSB();
              __ISB();
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
            //svc_os_arch_start();
            os_arch_start();
        }
    }

    return err;
}

/* WWW: we should add code to be able to lock/unlock the scheduler.
   This is rt_tsk_lock and rt_tsk_unlock in mbed */
