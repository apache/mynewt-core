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

#include <string.h>
#include "os/mynewt.h"
#include "timesched/timesched.h"

static TAILQ_HEAD(timesched_q, timesched_timer) g_timesched_q;

static struct os_callout g_timesched_co;

void
timesched_resched(void)
{
    struct timesched_timer *timer;
    os_time_t ticks;
    struct os_timeval time;
    uint64_t msec;

    os_callout_stop(&g_timesched_co);

    timer = TAILQ_FIRST(&g_timesched_q);
    if (!timer) {
        /* No timer was started, no need to start callout */
        return;
    }

    os_gettimeofday(&time, NULL);

    os_timersub(&timer->expire, &time, &time);

    if (time.tv_sec < 0) {
        /* We're already past expiry time - fire callout "immediately" */
        ticks = 0;
    } else {
        msec = time.tv_sec * 1000 + time.tv_usec / 1000;

        /*
         * We could just schedule next callout to expire at calculated value,
         * however we need to assume time may be adjusted on device in the
         * meantime. Since there is no notification from OS that this happened,
         * we'll just schedule callout at shorter interval to adjust callout if
         * necessary. For now let's use 1 minute max.
         */
        msec = min(msec, 60 * 1000);

        ticks = os_time_ms_to_ticks32(msec);
    }

    os_callout_reset(&g_timesched_co, ticks);
}

static void
timesched_timer_co_cb(struct os_event *ev)
{
    struct timesched_timer *timer;
    struct os_timeval time;
    os_sr_t sr;

    os_gettimeofday(&time, NULL);

    OS_ENTER_CRITICAL(sr);

    timer = TAILQ_FIRST(&g_timesched_q);
    while (timer && OS_TIMEVAL_LEQ(timer->expire, time)) {
        os_eventq_put(timer->evq, &timer->ev);

        TAILQ_REMOVE(&g_timesched_q, timer, link);
        timer = TAILQ_FIRST(&g_timesched_q);
    }

    OS_EXIT_CRITICAL(sr);

    timesched_resched();
}

void
timesched_timer_init(struct timesched_timer *timer, struct os_eventq *evq,
                     os_event_fn *ev_cb, void *ev_arg)
{
    memset(timer, 0, sizeof(*timer));

    timer->evq = evq;
    timer->ev.ev_cb = ev_cb;
    timer->ev.ev_arg = ev_arg;
}

int
timesched_timer_start(struct timesched_timer *timer, struct os_timeval *utctime)
{
    struct timesched_timer *entry;
    os_sr_t sr;

    timer->expire = *utctime;

    OS_ENTER_CRITICAL(sr);

    if (TAILQ_EMPTY(&g_timesched_q)) {
        TAILQ_INSERT_HEAD(&g_timesched_q, timer, link);
    } else {
        TAILQ_FOREACH(entry, &g_timesched_q, link) {
            if (OS_TIMEVAL_LT(timer->expire, entry->expire)) {
                TAILQ_INSERT_BEFORE(entry, timer, link);
                break;
            }
        }

        if (!entry) {
            TAILQ_INSERT_TAIL(&g_timesched_q, timer, link);
        }
    }

    OS_EXIT_CRITICAL(sr);

    timesched_resched();

    return OS_OK;
}

int
timesched_timer_stop(struct timesched_timer *timer)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);

    if (timer->link.tqe_prev != NULL) {
        TAILQ_REMOVE(&g_timesched_q, timer, link);
    }

    OS_EXIT_CRITICAL(sr);

    return OS_OK;
}

void
timesched_init(void)
{
    os_callout_init(&g_timesched_co, os_eventq_dflt_get(),
                    timesched_timer_co_cb, NULL);
}
