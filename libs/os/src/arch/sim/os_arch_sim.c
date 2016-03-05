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
#include "os_priv.h"

#ifdef __APPLE__
#define _XOPEN_SOURCE
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <assert.h> 
#include <util/util.h>

struct stack_frame {
    int sf_mainsp;              /* stack on which main() is executing */
    jmp_buf sf_jb;
    int sf_sigsblocked;
    struct os_task *sf_task;
};

/*
 * Assert that 'sf_mainsp' and 'sf_jb' are at the specific offsets where
 * os_arch_frame_init() expects them to be.
 */
CTASSERT(offsetof(struct stack_frame, sf_mainsp) == 0);
CTASSERT(offsetof(struct stack_frame, sf_jb) == 4);

extern void os_arch_frame_init(struct stack_frame *sf);

#define CTX_SWITCH_ISR (1)
#define CTX_SWITCH_TASK (2)

#define ISR_BLOCK_OFF (0) 
#define ISR_BLOCK_ON (1)

#define sim_setjmp(__jb) _setjmp(__jb)
#define sim_longjmp(__jb, __ret) _longjmp(__jb, __ret) 

sigset_t g_sigset;  
volatile int g_block_isr = ISR_BLOCK_OFF;

static volatile int g_block_isr_on = ISR_BLOCK_ON;
static volatile int g_block_isr_off = ISR_BLOCK_OFF;

static int g_pending_ticks = 0;

static void 
isr_state(volatile int *state, volatile int *ostate) 
{
    if (ostate) {
        *ostate = g_block_isr;
    }

    if (state) {
        g_block_isr = *state;
    }
}


static void 
sigs_unblock(void)
{
    sigprocmask(SIG_UNBLOCK, &g_sigset, NULL);
}

static void
sigs_block(void)
{
    sigprocmask(SIG_BLOCK, &g_sigset, NULL);
}

/*
 * Called from 'os_arch_frame_init()' when setjmp returns indirectly via
 * longjmp. The return value of setjmp is passed to this function as 'rc'.
 */
void
os_arch_task_start(struct stack_frame *sf, int rc)
{
    struct os_task *task;

    task = sf->sf_task;

    isr_state(&g_block_isr_off, NULL);

    if (rc == CTX_SWITCH_ISR) {
        sigs_unblock();
    }

    task->t_func(task->t_arg);

    /* This should never return */ 
    assert(0); 
}

os_stack_t *
os_arch_task_stack_init(struct os_task *t, os_stack_t *stack_top, int size) 
{
    struct stack_frame *sf;

    sf = (struct stack_frame *) ((uint8_t *) stack_top - sizeof(*sf));
    sf->sf_sigsblocked = 0;
    sf->sf_task = t;

    isr_state(&g_block_isr_on, NULL); 
    os_arch_frame_init(sf);
    isr_state(&g_block_isr_off, NULL);

    return ((os_stack_t *)sf);
}

void
os_arch_ctx_sw(struct os_task *next_t)  
{
    struct os_task *t;
    struct stack_frame *sf; 
    int rc;

    t = os_sched_get_current_task();
    if (t) {
        sf = (struct stack_frame *) t->t_stackptr;

        rc = sim_setjmp(sf->sf_jb);
        if (rc != 0) {
            if (rc == CTX_SWITCH_ISR) { 
                sigs_unblock();
            }
            return;
        }
    }

    os_sched_ctx_sw_hook(next_t);

    os_sched_set_current_task(next_t);

    sf = (struct stack_frame *) next_t->t_stackptr;
    /* block signals if the task we switch to expects them blocked. */
    if (sf->sf_sigsblocked) {
        sigs_block();
        rc = CTX_SWITCH_ISR;
    } else {
        rc = CTX_SWITCH_TASK;
    }

    sim_longjmp(sf->sf_jb, rc);
}

void
os_arch_ctx_sw_isr(struct os_task *next_t)
{
    struct os_task *t;
    struct stack_frame *sf;
    int isr_ctx;
    volatile int block_isr_off;
    int rc;

    block_isr_off = g_block_isr_off; 

    t = os_sched_get_current_task();
    if (t) {
        sf = (struct stack_frame *) t->t_stackptr;
        
        /* block signals coming from an interrupt context */
        sf->sf_sigsblocked = 1;

        isr_state(&g_block_isr_on, &isr_ctx);

        rc = sim_setjmp(sf->sf_jb);
        if (rc != 0) {
            sf->sf_sigsblocked = 0;
            isr_state(&block_isr_off, NULL);
            return;
        }
    }

    isr_state(&block_isr_off, NULL);

    os_sched_ctx_sw_hook(next_t);

    os_sched_set_current_task(next_t);
    
    sf = (struct stack_frame *) next_t->t_stackptr;
    sim_longjmp(sf->sf_jb, CTX_SWITCH_ISR); 
}

os_sr_t 
os_arch_save_sr(void)
{
    int isr_ctx;

    isr_state(&g_block_isr_on, &isr_ctx); 

    return (isr_ctx); 
}

void
os_arch_restore_sr(int isr_ctx)  
{
    if (isr_ctx == ISR_BLOCK_ON) {
        return;
    } else {
        isr_state(&g_block_isr_off, NULL);
    }
}

static void timer_handler(int sig);

static void
initialize_signals(void)
{
    struct sigaction sa;

    sigemptyset(&g_sigset);
    sigaddset(&g_sigset, SIGALRM); 
    sigaddset(&g_sigset, SIGVTALRM);

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = timer_handler;

    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGVTALRM, &sa, NULL);
}

static void
cancel_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_DFL;

    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGVTALRM, &sa, NULL);
}

static void
timer_handler(int sig)
{
    static struct timeval time_last;
    static int time_inited; 
    struct timeval time_now, time_diff; 
    int isr_ctx;

    if (!time_inited) {
        gettimeofday(&time_last, NULL);
        time_inited = 1;
    }

    isr_state(NULL, &isr_ctx); 
    if (isr_ctx == ISR_BLOCK_ON) {
        ++g_pending_ticks;
        return;
    }

    gettimeofday(&time_now, NULL);
    timersub(&time_now, &time_last, &time_diff);

    g_pending_ticks = time_diff.tv_usec / 1000;

    while (--g_pending_ticks >= 0) {
        os_time_tick();
        os_callout_tick();
    }

    time_last = time_now;
    g_pending_ticks = 0;

    os_sched_os_timer_exp();
    os_sched(NULL, 1); 
}

static void
start_timer(void)
{
    struct itimerval it; 
    int rc;

    initialize_signals();

    /* 1 msec OS tick */
    memset(&it, 0, sizeof(it));
    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = 1000;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 1000;

    rc = setitimer(ITIMER_VIRTUAL, &it, NULL);
    if (rc != 0) {
        const char msg[] = "Cannot set itimer";
        write(2, msg, sizeof(msg));
        _exit(1);
    }
}

static void
stop_timer(void)
{
    struct itimerval it;
    int rc;

    memset(&it, 0, sizeof(it));

    rc = setitimer(ITIMER_VIRTUAL, &it, NULL);
    if (rc != 0) {
        const char msg[] = "Cannot stop itimer";
        write(2, msg, sizeof(msg));
        _exit(1);
    }

    cancel_signals();
}

os_error_t 
os_arch_os_init(void)
{
    g_current_task = NULL;

    TAILQ_INIT(&g_os_run_list);
    TAILQ_INIT(&g_os_sleep_list);

    os_init_idle_task();
    os_sanity_task_init(1);

    os_bsp_init();

    return OS_OK;
}

os_error_t
os_arch_os_start(void)
{
    struct stack_frame *sf; 
    struct os_task *t;

    start_timer();

    t = os_sched_next_task();
    os_sched_set_current_task(t);

    g_os_started = 1; 

    sf = (struct stack_frame *) t->t_stackptr;
    sim_longjmp(sf->sf_jb, CTX_SWITCH_TASK); 

    return 0;
}

/**
 * Stops the tick timer and clears the "started" flag.  This function is only
 * implemented for sim.
 */
void
os_arch_os_stop(void)
{
    stop_timer();
    g_os_started = 0;
}
