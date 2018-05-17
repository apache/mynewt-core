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

#include <assert.h>
#include <string.h>
#include "syscfg/syscfg.h"
#if !MYNEWT_VAL(OS_SYSVIEW_TRACE_CALLOUT)
#define OS_TRACE_DISABLE_FILE_API
#endif
#include "os/mynewt.h"
#include "os_priv.h"

struct os_callout_list g_callout_list;

void os_callout_init(struct os_callout *c, struct os_eventq *evq,
                     os_event_fn *ev_cb, void *ev_arg)
{
    os_trace_api_u32x2(OS_TRACE_ID_CALLOUT_INIT, (uint32_t)c, (uint32_t)evq);

    memset(c, 0, sizeof(*c));
    c->c_ev.ev_cb = ev_cb;
    c->c_ev.ev_arg = ev_arg;
    c->c_evq = evq;

    os_trace_api_ret(OS_TRACE_ID_CALLOUT_INIT);
}

void
os_callout_stop(struct os_callout *c)
{
    os_sr_t sr;

    os_trace_api_u32(OS_TRACE_ID_CALLOUT_STOP, (uint32_t)c);

    OS_ENTER_CRITICAL(sr);

    if (os_callout_queued(c)) {
        TAILQ_REMOVE(&g_callout_list, c, c_next);
        c->c_next.tqe_prev = NULL;
    }

    if (c->c_evq) {
        os_eventq_remove(c->c_evq, &c->c_ev);
    }

    OS_EXIT_CRITICAL(sr);

    os_trace_api_ret(OS_TRACE_ID_CALLOUT_STOP);
}

int
os_callout_reset(struct os_callout *c, int32_t ticks)
{
    struct os_callout *entry;
    os_sr_t sr;
    int ret;

    os_trace_api_u32x2(OS_TRACE_ID_CALLOUT_RESET, (uint32_t)c, (uint32_t)ticks);

    if (ticks < 0) {
        ret = OS_EINVAL;
        goto err;
    }

    OS_ENTER_CRITICAL(sr);

    os_callout_stop(c);

    if (ticks == 0) {
        ticks = 1;
    }

    c->c_ticks = os_time_get() + ticks;

    entry = NULL;
    TAILQ_FOREACH(entry, &g_callout_list, c_next) {
        if (OS_TIME_TICK_LT(c->c_ticks, entry->c_ticks)) {
            break;
        }
    }

    if (entry) {
        TAILQ_INSERT_BEFORE(entry, c, c_next);
    } else {
        TAILQ_INSERT_TAIL(&g_callout_list, c, c_next);
    }

    OS_EXIT_CRITICAL(sr);

    ret = OS_OK;

err:
    os_trace_api_ret_u32(OS_TRACE_ID_CALLOUT_RESET, (uint32_t)ret);
    return ret;
}


/**
 * This function is called by the OS in the time tick.  It searches the list
 * of callouts, and sees if any of them are ready to run.  If they are ready
 * to run, it posts an event for each callout that's ready to run,
 * to the event queue provided to os_callout_init().
 */
void
os_callout_tick(void)
{
    os_sr_t sr;
    struct os_callout *c;
    uint32_t now;

    os_trace_api_void(OS_TRACE_ID_CALLOUT_TICK);

    now = os_time_get();

    while (1) {
        OS_ENTER_CRITICAL(sr);
        c = TAILQ_FIRST(&g_callout_list);
        if (c) {
            if (OS_TIME_TICK_GEQ(now, c->c_ticks)) {
                TAILQ_REMOVE(&g_callout_list, c, c_next);
                c->c_next.tqe_prev = NULL;
            } else {
                c = NULL;
            }
        }
        OS_EXIT_CRITICAL(sr);

        if (c) {
            if (c->c_evq) {
                os_eventq_put(c->c_evq, &c->c_ev);
            } else {
                c->c_ev.ev_cb(&c->c_ev);
            }
        } else {
            break;
        }
    }

    os_trace_api_ret(OS_TRACE_ID_CALLOUT_TICK);
}

/*
 * Returns the number of ticks to the first pending callout. If there are no
 * pending callouts then return OS_TIMEOUT_NEVER instead.
 *
 * @param now The time now
 *
 * @return Number of ticks to first pending callout
 */
os_time_t
os_callout_wakeup_ticks(os_time_t now)
{
    os_time_t rt;
    struct os_callout *c;

    OS_ASSERT_CRITICAL();

    c = TAILQ_FIRST(&g_callout_list);
    if (c != NULL) {
        if (OS_TIME_TICK_GEQ(c->c_ticks, now)) {
            rt = c->c_ticks - now;
        } else {
            rt = 0;     /* callout time is in the past */
        }
    } else {
        rt = OS_TIMEOUT_NEVER;
    }

    return (rt);
}


os_time_t
os_callout_remaining_ticks(struct os_callout *c, os_time_t now)
{
    os_sr_t sr;
    os_time_t rt;

    OS_ENTER_CRITICAL(sr);

    if (OS_TIME_TICK_GEQ(c->c_ticks, now)) {
        rt = c->c_ticks - now;
    } else {
        rt = 0;     /* callout time is in the past */
    }

    OS_EXIT_CRITICAL(sr);

    return rt;
}

