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
#include "os/mynewt.h"

#include "hal/hal_timer.h"

/*
 * For native cpu implementation.
 */
static uint8_t native_timer_task_started;
#define NATIVE_TIMER_STACK_SIZE   (1024)
static os_stack_t native_timer_stack[NATIVE_TIMER_STACK_SIZE];
static struct os_task native_timer_task_struct;
static struct os_eventq native_timer_evq;

struct native_timer {
    struct os_callout callout;
    uint32_t ticks_per_ostick;
    uint32_t cnt;
    uint32_t last_ostime;
    int num;
    TAILQ_HEAD(hal_timer_qhead, hal_timer) timers;
} native_timers[1];

/**
 * This is the function called when the timer fires.
 *
 * @param arg
 */
static void
native_timer_cb(struct os_event *ev)
{
    struct native_timer *nt = (struct native_timer *)ev->ev_arg;
    uint32_t cnt;
    struct hal_timer *ht;
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    cnt = hal_timer_read(nt->num);
    while ((ht = TAILQ_FIRST(&nt->timers)) != NULL) {
        if (((int32_t)(cnt - ht->expiry)) >= 0) {
            TAILQ_REMOVE(&nt->timers, ht, link);
            ht->link.tqe_prev = NULL;
            ht->cb_func(ht->cb_arg);
        } else {
            break;
        }
    }
    ht = TAILQ_FIRST(&nt->timers);
    if (ht) {
        os_callout_reset(&nt->callout,
          (ht->expiry - hal_timer_read(nt->num)) / nt->ticks_per_ostick);
    }
    OS_EXIT_CRITICAL(sr);
}

static void
native_timer_task(void *arg)
{
    while (1) {
        os_eventq_run(&native_timer_evq);
    }
}

int
hal_timer_init(int num, void *cfg)
{
    return 0;
}

int
hal_timer_config(int num, uint32_t clock_freq)
{
    struct native_timer *nt;

    if (num != 0) {
        return -1;
    }
    nt = &native_timers[num];

    /* Set the clock frequency */
    nt->ticks_per_ostick = clock_freq / OS_TICKS_PER_SEC;
    if (!nt->ticks_per_ostick) {
        nt->ticks_per_ostick = 1;
    }
    nt->num = num;
    nt->cnt = 0;
    nt->last_ostime = os_time_get();
    if (!native_timer_task_started) {
        os_task_init(&native_timer_task_struct, "native_timer",
          native_timer_task, NULL, OS_TASK_PRI_HIGHEST, OS_WAIT_FOREVER,
          native_timer_stack, NATIVE_TIMER_STACK_SIZE);

        /* Initialize the eventq and task */
        os_eventq_init(&native_timer_evq);
        native_timer_task_started = 1;
    }

    /* Initialize the callout function */
    os_callout_init(&nt->callout, &native_timer_evq, native_timer_cb, nt);

    return 0;
}

int
hal_timer_deinit(int num)
{
    struct native_timer *nt;

    if (num != 0) {
        return -1;
    }
    nt = &native_timers[num];

    os_callout_stop(&nt->callout);
    return 0;
}

/**
 * hal timer get resolution
 *
 * Get the resolution of the timer. This is the timer period, in nanoseconds
 *
 * @param timer_num
 *
 * @return uint32_t
 */
uint32_t
hal_timer_get_resolution(int num)
{
    struct native_timer *nt;

    if (num >= 0) {
        return 0;
    }
    nt = &native_timers[num];
    return 1000000000 / (nt->ticks_per_ostick * OS_TICKS_PER_SEC);
}

/**
 * hal_timer_read
 *
 * Returns the low 32 bits of timer counter.
 *
 * @return uint32_t The timer counter register.
 */
uint32_t
hal_timer_read(int num)
{
    struct native_timer *nt;
    os_sr_t sr;
    uint32_t ostime;
    uint32_t delta_osticks;

    if (num != 0) {
        return -1;
    }
    nt = &native_timers[num];
    OS_ENTER_CRITICAL(sr);
    ostime = os_time_get();
    delta_osticks = (uint32_t)(ostime - nt->last_ostime);
    if (delta_osticks) {
        nt->last_ostime = ostime;
        nt->cnt += nt->ticks_per_ostick * delta_osticks;

    }
    OS_EXIT_CRITICAL(sr);

    return (uint32_t)nt->cnt;
}

/**
 * hal timer delay
 *
 * Blocking delay for n ticks
 *
 * @param timer_num
 * @param ticks
 *
 * @return int 0 on success; error code otherwise.
 */
int
hal_timer_delay(int num, uint32_t ticks)
{
    uint32_t until;

    if (num != 0) {
        return -1;
    }

    until = hal_timer_read(0) + ticks;
    while ((int32_t)(hal_timer_read(0) - until) <= 0) {
        ;
    }
    return 0;
}

/**
 *
 * Initialize the HAL timer structure with the callback and the callback
 * argument. Also initializes the HW specific timer pointer.
 *
 * @param cb_func
 *
 * @return int
 */
int
hal_timer_set_cb(int num, struct hal_timer *timer, hal_timer_cb cb_func,
                 void *arg)
{
    struct native_timer *nt;

    if (num != 0) {
        return -1;
    }
    nt = &native_timers[num];
    timer->cb_func = cb_func;
    timer->cb_arg = arg;
    timer->bsp_timer = nt;
    timer->link.tqe_prev = NULL;

    return 0;
}

/**
 * hal_timer_start()
 *
 * Start a timer. Timer fires 'ticks' ticks from now.
 *
 * @param timer
 * @param ticks
 *
 * @return int
 */
int
hal_timer_start(struct hal_timer *timer, uint32_t ticks)
{
    struct native_timer *nt;
    uint32_t tick;

    nt = (struct native_timer *)timer->bsp_timer;

    tick = ticks + hal_timer_read(nt->num);
    return hal_timer_start_at(timer, tick);
}

/**
 * hal_timer_start_at()
 *
 * Start a timer. Timer fires at tick 'tick'.
 *
 * @param timer
 * @param tick
 *
 * @return int
 */
int
hal_timer_start_at(struct hal_timer *timer, uint32_t tick)
{
    struct native_timer *nt;
    struct hal_timer *ht;
    uint32_t curtime;
    uint32_t osticks;
    os_sr_t sr;

    nt = (struct native_timer *)timer->bsp_timer;

    timer->expiry = tick;

    OS_ENTER_CRITICAL(sr);

    if (TAILQ_EMPTY(&nt->timers)) {
        TAILQ_INSERT_HEAD(&nt->timers, timer, link);
    } else {
        TAILQ_FOREACH(ht, &nt->timers, link) {
            if ((int32_t)(timer->expiry - ht->expiry) < 0) {
                TAILQ_INSERT_BEFORE(ht, timer, link);
                break;
            }
        }
        if (!ht) {
            TAILQ_INSERT_TAIL(&nt->timers, timer, link);
        }
    }

    curtime = hal_timer_read(nt->num);
    if ((int32_t)(tick - curtime) <= 0) {
        /*
         * Event in the past (should be the case if it was just inserted).
         */
        os_callout_reset(&nt->callout, 0);
    } else {
        if (timer == TAILQ_FIRST(&nt->timers)) {
            osticks = (tick - curtime) / nt->ticks_per_ostick;
            os_callout_reset(&nt->callout, osticks);
        }
    }
    OS_EXIT_CRITICAL(sr);

    return 0;
}

/**
 * hal_timer_stop()
 *
 * Cancels the timer.
 *
 * @param timer
 *
 * @return int
 */
int
hal_timer_stop(struct hal_timer *timer)
{
    struct native_timer *nt;
    struct hal_timer *ht;
    int reset_ocmp;
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);

    nt = (struct native_timer *)timer->bsp_timer;
    if (timer->link.tqe_prev != NULL) {
        reset_ocmp = 0;
        if (timer == TAILQ_FIRST(&nt->timers)) {
            /* If first on queue, we will need to reset OCMP */
            ht = TAILQ_NEXT(timer, link);
            reset_ocmp = 1;
        }
        TAILQ_REMOVE(&nt->timers, timer, link);
        timer->link.tqe_prev = NULL;
        if (reset_ocmp) {
            if (ht) {
                os_callout_reset(&nt->callout,
                  (ht->expiry - hal_timer_read(nt->num) /
                    nt->ticks_per_ostick));
            } else {
                os_callout_stop(&nt->callout);
            }
        }
    }
    OS_EXIT_CRITICAL(sr);

    return 0;
}
