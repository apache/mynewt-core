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

/* For native cpu implementation */
#define NATIVE_CPUTIME_STACK_SIZE   (1024)
os_stack_t g_native_cputime_stack[NATIVE_CPUTIME_STACK_SIZE];
struct os_task g_native_cputime_task;

struct os_callout_func g_native_cputimer;
struct os_eventq g_native_cputime_evq;
static uint32_t g_native_cputime_cputicks_per_ostick;
static uint64_t g_native_cputime;
static uint32_t g_native_cputime_last_ostime;

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
void
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
            cf->cf_func(CF_ARG(cf));
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
cputime_hw_init(uint32_t clock_freq)
{
    /* Clock frequency must be at least 1 MHz */
    if (clock_freq < 1000000U) {
        return -1;
    }

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
    return g_native_cputime;
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
    delta_osticks = (uint32_t)(ostime - g_native_cputime_last_ostime);
    if (delta_osticks) {
        g_native_cputime_last_ostime = ostime;
        g_native_cputime +=
            (uint64_t)g_native_cputime_cputicks_per_ostick * delta_osticks;

    }
    OS_EXIT_CRITICAL(sr);

    return (uint32_t)g_native_cputime;
}
