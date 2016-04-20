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
    sigjmp_buf sf_jb;
    struct os_task *sf_task;
};

/*
 * Assert that 'sf_mainsp' and 'sf_jb' are at the specific offsets where
 * os_arch_frame_init() expects them to be.
 */
CTASSERT(offsetof(struct stack_frame, sf_mainsp) == 0);
CTASSERT(offsetof(struct stack_frame, sf_jb) == 4);

extern void os_arch_frame_init(struct stack_frame *sf);

#define sim_setjmp(__jb) sigsetjmp(__jb, 0)
#define sim_longjmp(__jb, __ret) siglongjmp(__jb, __ret)

#define OS_USEC_PER_TICK    (1000000 / OS_TICKS_PER_SEC)

static pid_t mypid;
static sigset_t allsigs, nosigs;
static void timer_handler(int sig);

static bool suspended;      /* process is blocked in sigsuspend() */
static sigset_t suspsigs;   /* signals delivered in sigsuspend() */

/*
 * Called from 'os_arch_frame_init()' when setjmp returns indirectly via
 * longjmp. The return value of setjmp is passed to this function as 'rc'.
 */
void
os_arch_task_start(struct stack_frame *sf, int rc)
{
    struct os_task *task;

    /*
     * Interrupts are disabled when a task starts executing. This happens in
     * two different ways:
     * - via os_arch_os_start() for the first task.
     * - via os_sched() for all other tasks.
     *
     * Enable interrupts before starting the task.
     */
    OS_EXIT_CRITICAL(0);

    task = sf->sf_task;
    task->t_func(task->t_arg);

    /* This should never return */
    assert(0);
}

os_stack_t *
os_arch_task_stack_init(struct os_task *t, os_stack_t *stack_top, int size)
{
    struct stack_frame *sf;

    sf = (struct stack_frame *) ((uint8_t *) stack_top - sizeof(*sf));
    sf->sf_task = t;

    os_arch_frame_init(sf);

    return ((os_stack_t *)sf);
}

void
os_arch_ctx_sw(struct os_task *next_t)
{
    /*
     * gdb will stop execution of the program on most signals (e.g. SIGUSR1)
     * whereas it passes SIGURG to the process without any special settings.
     */
    kill(mypid, SIGURG);
}

static void
ctxsw_handler(int sig)
{
    struct os_task *t, *next_t;
    struct stack_frame *sf; 
    int rc;

    OS_ASSERT_CRITICAL();

    /*
     * Just record that this handler was called when the process was blocked.
     * The handler will be called after sigsuspend() returns in the correct
     * order.
     */
    if (suspended) {
        sigaddset(&suspsigs, sig);
        return;
    }

    t = os_sched_get_current_task();
    next_t = os_sched_next_task();
    if (t == next_t) {
        /*
         * Context switch not needed - just return.
         */
        return;
    }

    if (t) {
        sf = (struct stack_frame *) t->t_stackptr;

        rc = sim_setjmp(sf->sf_jb);
        if (rc != 0) {
            OS_ASSERT_CRITICAL();
            return;
        }
    }

    os_sched_ctx_sw_hook(next_t);

    os_sched_set_current_task(next_t);

    sf = (struct stack_frame *) next_t->t_stackptr;
    sim_longjmp(sf->sf_jb, 1);
}

/*
 * Disable signals and enter a critical section.
 *
 * Returns 1 if signals were already blocked and 0 otherwise.
 */
os_sr_t
os_arch_save_sr(void)
{
    int error;
    sigset_t omask;

    error = sigprocmask(SIG_BLOCK, &allsigs, &omask);
    assert(error == 0);

    /*
     * If any one of the signals in 'allsigs' is present in 'omask' then
     * we are already inside a critical section.
     */
    return (sigismember(&omask, SIGALRM));
}

void
os_arch_restore_sr(os_sr_t osr)
{
    int error;

    OS_ASSERT_CRITICAL();
    assert(osr == 0 || osr == 1);

    if (osr == 1) {
        /* Exiting a nested critical section */
        return;
    }

    error = sigprocmask(SIG_UNBLOCK, &allsigs, NULL);
    assert(error == 0);
}

int
os_arch_in_critical(void)
{
    int error;
    sigset_t omask;

    error = sigprocmask(SIG_SETMASK, NULL, &omask);
    assert(error == 0);

    /*
     * If any one of the signals in 'allsigs' is present in 'omask' then
     * we are already inside a critical section.
     */
    return (sigismember(&omask, SIGALRM));
}

static struct {
    int num;
    void (*handler)(int sig);
} signals[] = {
    { SIGALRM, timer_handler },
    { SIGURG, ctxsw_handler },
};

#define NUMSIGS     (sizeof(signals)/sizeof(signals[0]))

void
os_tick_idle(os_time_t ticks)
{
    int i, rc, sig;
    struct itimerval it;
    void (*handler)(int sig);

    OS_ASSERT_CRITICAL();

    if (ticks > 0) {
        /*
         * Enter tickless regime and set the timer to fire after 'ticks'
         * worth of time has elapsed.
         */
        it.it_value.tv_sec = ticks / OS_TICKS_PER_SEC;
        it.it_value.tv_usec = (ticks % OS_TICKS_PER_SEC) * OS_USEC_PER_TICK;
        it.it_interval.tv_sec = 0;
        it.it_interval.tv_usec = OS_USEC_PER_TICK;
        rc = setitimer(ITIMER_REAL, &it, NULL);
        assert(rc == 0);
    }

    suspended = true;
    sigemptyset(&suspsigs);
    sigsuspend(&nosigs);        /* Wait for a signal to wake us up */
    suspended = false;

    /*
     * Call handlers for signals delivered to the process during sigsuspend().
     * The SIGALRM handler is called before any other handlers to ensure that
     * OS time is always correct.
     */
    if (sigismember(&suspsigs, SIGALRM)) {
        timer_handler(SIGALRM);
    }
    for (i = 0; i < NUMSIGS; i++) {
        sig = signals[i].num;
        handler = signals[i].handler;
        if (sig != SIGALRM && sigismember(&suspsigs, sig)) {
            handler(sig);
        }
    }

    if (ticks > 0) {
        /*
         * Enable the periodic timer interrupt.
         */
        it.it_value.tv_sec = 0;
        it.it_value.tv_usec = OS_USEC_PER_TICK;
        it.it_interval.tv_sec = 0;
        it.it_interval.tv_usec = OS_USEC_PER_TICK;
        rc = setitimer(ITIMER_REAL, &it, NULL);
        assert(rc == 0);
    }
}

static void
signals_init(void)
{
    int i, error;
    struct sigaction sa;

    sigemptyset(&nosigs);
    sigemptyset(&allsigs);
    for (i = 0; i < NUMSIGS; i++) {
        sigaddset(&allsigs, signals[i].num);
    }

    for (i = 0; i < NUMSIGS; i++) {
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = signals[i].handler;
        sa.sa_mask = allsigs;
        sa.sa_flags = SA_RESTART;
        error = sigaction(signals[i].num, &sa, NULL);
        assert(error == 0);
    }

    /*
     * We use SIGALRM as a proxy for 'allsigs' to check if we are inside
     * a critical section (for e.g. see os_arch_in_critical()). Make sure
     * that SIGALRM is indeed present in 'allsigs'.
     */
    assert(sigismember(&allsigs, SIGALRM));
}

static void
signals_cleanup(void)
{
    int i, error;
    struct sigaction sa;

    for (i = 0; i < NUMSIGS; i++) {
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = SIG_DFL;
        error = sigaction(signals[i].num, &sa, NULL);
        assert(error == 0);
    }
}

static void
timer_handler(int sig)
{
    struct timeval time_now, time_diff;
    int ticks;

    static struct timeval time_last;
    static int time_inited;

    OS_ASSERT_CRITICAL();

    /*
     * Just record that this handler was called when the process was blocked.
     * The handler will be called after sigsuspend() returns in the proper
     * order.
     */
    if (suspended) {
        sigaddset(&suspsigs, sig);
        return;
    }

    if (!time_inited) {
        gettimeofday(&time_last, NULL);
        time_inited = 1;
    }

    gettimeofday(&time_now, NULL);
    if (timercmp(&time_now, &time_last, <)) {
        /*
         * System time going backwards.
         */
        time_last = time_now;
    } else {
        timersub(&time_now, &time_last, &time_diff);

        ticks = time_diff.tv_sec * OS_TICKS_PER_SEC;
        ticks += time_diff.tv_usec / OS_USEC_PER_TICK;

        /*
         * Update 'time_last' but account for the remainder usecs that did not
         * contribute towards whole 'ticks'.
         */
        time_diff.tv_sec = 0;
        time_diff.tv_usec %= OS_USEC_PER_TICK;
        timersub(&time_now, &time_diff, &time_last);

        os_time_advance(ticks);
    }
}

static void
start_timer(void)
{
    struct itimerval it;
    int rc;

    memset(&it, 0, sizeof(it));
    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = OS_USEC_PER_TICK;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = OS_USEC_PER_TICK;

    rc = setitimer(ITIMER_REAL, &it, NULL);
    assert(rc == 0);
}

static void
stop_timer(void)
{
    struct itimerval it;
    int rc;

    memset(&it, 0, sizeof(it));

    rc = setitimer(ITIMER_REAL, &it, NULL);
    assert(rc == 0);
}

os_error_t
os_arch_os_init(void)
{
    mypid = getpid();
    g_current_task = NULL;

    TAILQ_INIT(&g_os_task_list);
    TAILQ_INIT(&g_os_run_list);
    TAILQ_INIT(&g_os_sleep_list);

    /*
     * Setup all interrupt handlers.
     *
     * This must be done early because task initialization uses critical
     * sections which function correctly only when 'allsigs' is initialized.
     */
    signals_init();

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
    os_sr_t sr;

    /*
     * Disable interrupts before enabling any interrupt sources. Pending
     * interrupts will be recognized when the first task starts executing.
     */
    OS_ENTER_CRITICAL(sr);
    assert(sr == 0);

    /* Enable the interrupt sources */
    start_timer();

    t = os_sched_next_task();
    os_sched_set_current_task(t);

    g_os_started = 1;

    sf = (struct stack_frame *) t->t_stackptr;
    sim_longjmp(sf->sf_jb, 1);

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
    signals_cleanup();
    g_os_started = 0;
}
