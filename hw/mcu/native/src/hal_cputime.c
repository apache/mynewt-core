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
#include <stdint.h>
#include <assert.h>
#include "os/os.h"
#include "hal/hal_cputime.h"

/* CPUTIME data */
struct cputime_data
{
    uint32_t ticks_per_usec;    /* number of ticks per usec */
    uint32_t timer_isrs;        /* Number of timer interrupts */
    uint32_t ocmp_ints;         /* Number of ocmp interrupts */
    uint32_t uif_ints;          /* Number of overflow interrupts */
    uint32_t last_ostime;
    uint64_t cputime;           /* 64-bit cputime */
};
struct cputime_data g_cputime;

/* Queue for timers */
TAILQ_HEAD(cputime_qhead, cpu_timer) g_cputimer_q;

/* For native cpu implementation */
#define NATIVE_CPUTIME_STACK_SIZE   (1024)
os_stack_t g_native_cputime_stack[NATIVE_CPUTIME_STACK_SIZE];
struct os_task g_native_cputime_task;

struct os_callout_func g_native_cputimer;
struct os_eventq g_native_cputime_evq;
static uint32_t g_native_cputime_cputicks_per_ostick;


/**
 * Convert cpu time ticks to os ticks.
 * 
 * 
 * @param cputicks 
 * 
 * @return uint32_t 
 */
static uint32_t
native_cputime_ticks_to_osticks(uint32_t cputicks)
{
    uint32_t osticks;

    osticks = cputicks / g_native_cputime_cputicks_per_ostick;
    return osticks;
}

/**
 * cputime set ocmp 
 *  
 * Set the OCMP used by the cputime module to the desired cputime. 
 * 
 * @param timer Pointer to timer.
 */
static void
cputime_set_ocmp(struct cpu_timer *timer)
{
    uint32_t curtime;
    uint32_t osticks;

    curtime = cputime_get32();
    if ((int32_t)(timer->cputime - curtime) < 0) {
        osticks = 0;
    } else {
        osticks = native_cputime_ticks_to_osticks(timer->cputime - curtime);
    }

    /* Re-start the timer */
    os_callout_reset(&g_native_cputimer.cf_c, osticks);
}

/**
 * cputime chk expiration 
 *  
 * Iterates through the cputimer queue to determine if any timers have expired. 
 * If the timer has expired the timer is removed from the queue and the timer 
 * callback function is executed. 
 * 
 */
static void
cputime_chk_expiration(void)
{
    os_sr_t sr;
    struct cpu_timer *timer;

    OS_ENTER_CRITICAL(sr);
    while ((timer = TAILQ_FIRST(&g_cputimer_q)) != NULL) {
        if ((int32_t)(cputime_get32() - timer->cputime) >= 0) {
            TAILQ_REMOVE(&g_cputimer_q, timer, link);
            timer->cb(timer->arg);
        } else {
            break;
        }
    }

    /* Any timers left on queue? If so, we need to set OCMP */
    timer = TAILQ_FIRST(&g_cputimer_q);
    if (timer) {
        cputime_set_ocmp(timer);
    } else {
        os_callout_stop(&g_native_cputimer.cf_c);
    }
    OS_EXIT_CRITICAL(sr);
}

/**
 * This is the function called when the cputimer fires off. 
 * 
 * @param arg 
 */
void
native_cputimer_cb(void *arg)
{
    /* Count # of interrupts */
    ++g_cputime.ocmp_ints;

    /* Execute the timer */
    cputime_chk_expiration();
}

void
native_cputime_task_handler(void *arg)
{
    struct os_event *ev;
    struct os_callout_func *cf;

    while (1) {
        ev = os_eventq_get(&g_native_cputime_evq);
        switch (ev->ev_type) {
        case OS_EVENT_T_TIMER:
            cf = (struct os_callout_func *)ev;
            assert(cf->cf_func);
            cf->cf_func(cf->cf_arg);
            break;
        default:
            assert(0);
            break;
        }
    }
}

/**
 * cputime init 
 *  
 * Initialize the cputime module. This must be called after os_init is called 
 * and before any other timer API are used. This should be called only once 
 * and should be called before the hardware timer is used. 
 * 
 * @param clock_freq The desired cputime frequency, in hertz (Hz).
 * 
 * @return int 0 on success; -1 on error.
 */
int
cputime_init(uint32_t clock_freq)
{
    /* Clock frequency must be at least 1 MHz */
    if (clock_freq < 1000000U) {
        return -1;
    }

    /* Initialize the timer queue */
    TAILQ_INIT(&g_cputimer_q);

    /* Set the clock frequency */
    g_cputime.ticks_per_usec = clock_freq / 1000000U;
    g_native_cputime_cputicks_per_ostick = clock_freq / OS_TICKS_PER_SEC;

    os_task_init(&g_native_cputime_task, 
                 "native_cputimer", 
                 native_cputime_task_handler, 
                 NULL, 
                 OS_TASK_PRI_HIGHEST, 
                 OS_WAIT_FOREVER, 
                 g_native_cputime_stack, 
                 NATIVE_CPUTIME_STACK_SIZE);

    /* Initialize the eventq and task */
    os_eventq_init(&g_native_cputime_evq);

    /* Initialize the callout function */
    os_callout_func_init(&g_native_cputimer,
                         &g_native_cputime_evq,
                         native_cputimer_cb,
                         NULL);

    return 0;
}

/**
 * cputime get64
 *  
 * Returns cputime as a 64-bit number. 
 * 
 * @return uint64_t The 64-bit representation of cputime.
 */
uint64_t 
cputime_get64(void)
{
    cputime_get32();
    return g_cputime.cputime;
}

/**
 * cputime get32 
 *  
 * Returns the low 32 bits of cputime. 
 * 
 * @return uint32_t The lower 32 bits of cputime
 */
uint32_t
cputime_get32(void)
{
    os_sr_t sr;
    uint32_t ostime;
    uint32_t delta_osticks;

    OS_ENTER_CRITICAL(sr);
    ostime = os_time_get();
    delta_osticks = (uint32_t)(ostime - g_cputime.last_ostime);
    if (delta_osticks) {
        g_cputime.last_ostime = ostime;
        g_cputime.cputime +=
            (uint64_t)g_native_cputime_cputicks_per_ostick * delta_osticks;

    }
    OS_EXIT_CRITICAL(sr);

    return (uint32_t)g_cputime.cputime;
}

/**
 * cputime nsecs to ticks 
 *  
 * Converts the given number of nanoseconds into cputime ticks. 
 * 
 * @param usecs The number of nanoseconds to convert to ticks
 * 
 * @return uint32_t The number of ticks corresponding to 'nsecs'
 */
uint32_t 
cputime_nsecs_to_ticks(uint32_t nsecs)
{
    uint32_t ticks;

    ticks = ((nsecs * g_cputime.ticks_per_usec) + 999) / 1000;
    return ticks;
}

/**
 * cputime ticks to nsecs
 *  
 * Convert the given number of ticks into nanoseconds. 
 * 
 * @param ticks The number of ticks to convert to nanoseconds.
 * 
 * @return uint32_t The number of nanoseconds corresponding to 'ticks'
 */
uint32_t 
cputime_ticks_to_nsecs(uint32_t ticks)
{
    uint32_t nsecs;

    nsecs = ((ticks * 1000) + (g_cputime.ticks_per_usec - 1)) / 
            g_cputime.ticks_per_usec;

    return nsecs;
}

/**
 * cputime usecs to ticks 
 *  
 * Converts the given number of microseconds into cputime ticks. 
 * 
 * @param usecs The number of microseconds to convert to ticks
 * 
 * @return uint32_t The number of ticks corresponding to 'usecs'
 */
uint32_t 
cputime_usecs_to_ticks(uint32_t usecs)
{
    uint32_t ticks;

    ticks = (usecs * g_cputime.ticks_per_usec);
    return ticks;
}

/**
 * cputime ticks to usecs
 *  
 * Convert the given number of ticks into microseconds. 
 * 
 * @param ticks The number of ticks to convert to microseconds.
 * 
 * @return uint32_t The number of microseconds corresponding to 'ticks'
 */
uint32_t 
cputime_ticks_to_usecs(uint32_t ticks)
{
    uint32_t us;

    us =  (ticks + (g_cputime.ticks_per_usec - 1)) / g_cputime.ticks_per_usec;
    return us;
}

/**
 * cputime delay ticks
 *  
 * Wait until the number of ticks has elapsed. This is a blocking delay. 
 * 
 * @param ticks The number of ticks to wait.
 */
void 
cputime_delay_ticks(uint32_t ticks)
{
    uint32_t until;

    until = cputime_get32() + ticks;
    while ((int32_t)(cputime_get32() - until) < 0) {
        /* Loop here till finished */
    }
}

/**
 * cputime delay nsecs 
 *  
 * Wait until 'nsecs' nanoseconds has elapsed. This is a blocking delay. 
 *  
 * @param nsecs The number of nanoseconds to wait.
 */
void 
cputime_delay_nsecs(uint32_t nsecs)
{
    uint32_t ticks;

    ticks = cputime_nsecs_to_ticks(nsecs);
    cputime_delay_ticks(ticks);
}

/**
 * cputime delay usecs 
 *  
 * Wait until 'usecs' microseconds has elapsed. This is a blocking delay. 
 *  
 * @param usecs The number of usecs to wait.
 */
void 
cputime_delay_usecs(uint32_t usecs)
{
    uint32_t ticks;

    ticks = cputime_usecs_to_ticks(usecs);
    cputime_delay_ticks(ticks);
}

/**
 * cputime timer init
 * 
 * 
 * @param timer The timer to initialize. Cannot be NULL.
 * @param fp    The timer callback function. Cannot be NULL.
 * @param arg   Pointer to data object to pass to timer. 
 */
void 
cputime_timer_init(struct cpu_timer *timer, cputimer_func fp, void *arg)
{
    assert(timer != NULL);
    assert(fp != NULL);

    timer->cb = fp;
    timer->arg = arg;
    timer->link.tqe_prev = (void *) NULL;
}

/**
 * cputime timer start 
 *  
 * Start a cputimer that will expire at 'cputime'. If cputime has already 
 * passed, the timer callback will still be called (at interrupt context). 
 * 
 * @param timer     Pointer to timer to start. Cannot be NULL.
 * @param cputime   The cputime at which the timer should expire.
 */
void 
cputime_timer_start(struct cpu_timer *timer, uint32_t cputime)
{
    struct cpu_timer *entry;
    os_sr_t sr;

    assert(timer != NULL);

    /* XXX: should this use a mutex? not sure... */
    OS_ENTER_CRITICAL(sr);

    timer->cputime = cputime;
    if (TAILQ_EMPTY(&g_cputimer_q)) {
        TAILQ_INSERT_HEAD(&g_cputimer_q, timer, link);
    } else {
        TAILQ_FOREACH(entry, &g_cputimer_q, link) {
            if ((int32_t)(timer->cputime - entry->cputime) < 0) {
                TAILQ_INSERT_BEFORE(entry, timer, link);   
                break;
            }
        }
        if (!entry) {
            TAILQ_INSERT_TAIL(&g_cputimer_q, timer, link);
        }
    }

    /* If this is the head, we need to set new OCMP */
    if (timer == TAILQ_FIRST(&g_cputimer_q)) {
        cputime_set_ocmp(timer);
    }

    OS_EXIT_CRITICAL(sr);
}

/**
 * cputimer timer relative 
 *  
 * Sets a cpu timer that will expire 'usecs' microseconds from the current 
 * cputime. 
 * 
 * @param timer Pointer to timer. Cannot be NULL.
 * @param usecs The number of usecs from now at which the timer will expire.
 */
void 
cputime_timer_relative(struct cpu_timer *timer, uint32_t usecs)
{
    uint32_t cputime;

    assert(timer != NULL);

    cputime = cputime_get32() + cputime_usecs_to_ticks(usecs);
    cputime_timer_start(timer, cputime);
}

/**
 * cputime timer stop 
 *  
 * Stops a cputimer from running. The timer is removed from the timer queue 
 * and interrupts are disabled if no timers are left on the queue. Can be 
 * called even if timer is running. 
 * 
 * @param timer Pointer to cputimer to stop. Cannot be NULL.
 */
void 
cputime_timer_stop(struct cpu_timer *timer)
{
    os_sr_t sr;
    int reset_ocmp;
    struct cpu_timer *entry;

    assert(timer != NULL);

    OS_ENTER_CRITICAL(sr);

    /* If first on queue, we will need to reset OCMP */
    if (timer->link.tqe_prev != NULL) {
        reset_ocmp = 0;
        if (timer == TAILQ_FIRST(&g_cputimer_q)) {
            entry = TAILQ_NEXT(timer, link);
            reset_ocmp = 1;
        }
        TAILQ_REMOVE(&g_cputimer_q, timer, link);
        if (reset_ocmp) {
            if (entry) {
                cputime_set_ocmp(entry);
            } else {
                os_callout_stop(&g_native_cputimer.cf_c);
            }
        }
    }

    OS_EXIT_CRITICAL(sr);
}
