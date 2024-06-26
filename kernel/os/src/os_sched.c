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
#include "os/mynewt.h"
#include "os_priv.h"

struct os_task_list g_os_run_list = TAILQ_HEAD_INITIALIZER(g_os_run_list);
struct os_task_list g_os_sleep_list = TAILQ_HEAD_INITIALIZER(g_os_sleep_list);

struct os_task *g_current_task;

extern os_time_t g_os_time;
os_time_t g_os_last_ctx_sw_time;
static uint8_t os_sched_lock_count;

os_error_t
os_sched_insert(struct os_task *t)
{
    struct os_task *entry;
    os_sr_t sr;
    os_error_t rc;

    if (t->t_state != OS_TASK_READY) {
        rc = OS_EINVAL;
        goto err;
    }

    entry = NULL;
    OS_ENTER_CRITICAL(sr);
    TAILQ_FOREACH(entry, &g_os_run_list, t_os_list) {
        if (t->t_prio < entry->t_prio) {
            break;
        }
    }
    if (entry) {
        TAILQ_INSERT_BEFORE(entry, (struct os_task *) t, t_os_list);
    } else {
        TAILQ_INSERT_TAIL(&g_os_run_list, (struct os_task *) t, t_os_list);
    }
    OS_EXIT_CRITICAL(sr);

    return (0);
err:
    return (rc);
}

void
os_sched_ctx_sw_hook(struct os_task *next_t)
{
    uint32_t ticks;

#if MYNEWT_VAL(OS_CTX_SW_STACK_CHECK)
    os_stack_t *stack;
    int i;

    stack = g_current_task->t_stackbottom;
    for (i = 0; i < MYNEWT_VAL(OS_CTX_SW_STACK_GUARD); i++) {
        assert(stack[i] == OS_STACK_PATTERN);
    }
#endif
    next_t->t_ctx_sw_cnt++;
#if MYNEWT_VAL(OS_TASK_RUN_TIME_CPUTIME)
    ticks = os_cputime_get32();
#else
    ticks = g_os_time;
#endif
    g_current_task->t_run_time += ticks - g_os_last_ctx_sw_time;
    g_os_last_ctx_sw_time = ticks;
}

struct os_task *
os_sched_get_current_task(void)
{
    return (g_current_task);
}

void
os_sched_set_current_task(struct os_task *t)
{
    g_current_task = t;
}

void
os_sched(struct os_task *next_t)
{
    os_sr_t sr;

    if (os_sched_lock_count) {
        return;
    }

    OS_ENTER_CRITICAL(sr);

    if (!next_t) {
        next_t = os_sched_next_task();
    }

    os_arch_ctx_sw(next_t);

    OS_EXIT_CRITICAL(sr);
}

void
os_sched_suspend(void)
{
    os_sr_t sr;
    OS_ENTER_CRITICAL(sr);
    os_sched_lock_count++;
    OS_EXIT_CRITICAL(sr);
}

int
os_sched_resume(void)
{
    os_sr_t sr;
    int ret = 0;

    OS_ENTER_CRITICAL(sr);
    if (--os_sched_lock_count == 0) {
        os_sched(NULL);
    }
    ret = os_sched_lock_count;
    OS_EXIT_CRITICAL(sr);

    return ret;
}

int
os_sched_sleep(struct os_task *t, os_time_t nticks)
{
    struct os_task *entry;

    if (os_sched_lock_count) {
        return 0;
    }
    entry = NULL;

    TAILQ_REMOVE(&g_os_run_list, t, t_os_list);
    t->t_state = OS_TASK_SLEEP;
    t->t_next_wakeup = os_time_get() + nticks;
    if (nticks == OS_TIMEOUT_NEVER) {
        t->t_flags |= OS_TASK_FLAG_NO_TIMEOUT;
        TAILQ_INSERT_TAIL(&g_os_sleep_list, t, t_os_list);
    } else {
        TAILQ_FOREACH(entry, &g_os_sleep_list, t_os_list) {
            if ((entry->t_flags & OS_TASK_FLAG_NO_TIMEOUT) ||
                    OS_TIME_TICK_GT(entry->t_next_wakeup, t->t_next_wakeup)) {
                break;
            }
        }
        if (entry) {
            TAILQ_INSERT_BEFORE(entry, t, t_os_list);
        } else {
            TAILQ_INSERT_TAIL(&g_os_sleep_list, t, t_os_list);
        }
    }

    os_trace_task_stop_ready(t, OS_TASK_SLEEP);
    return (0);
}

int
os_sched_remove(struct os_task *t)
{

    if (t->t_state == OS_TASK_SLEEP) {
        TAILQ_REMOVE(&g_os_sleep_list, t, t_os_list);
    } else if (t->t_state == OS_TASK_READY) {
        TAILQ_REMOVE(&g_os_run_list, t, t_os_list);
    }
    t->t_next_wakeup = 0;
    t->t_flags |= OS_TASK_FLAG_NO_TIMEOUT;

    STAILQ_REMOVE(&g_os_task_list, t, os_task, t_os_task_list);

    os_trace_task_stop_exec();
    return OS_OK;
}

int
os_sched_wakeup(struct os_task *t)
{
    struct os_task_obj *os_obj;

    assert(t->t_state == OS_TASK_SLEEP);

    /* Remove self from object list if waiting on one */
    if (t->t_obj) {
        os_obj = (struct os_task_obj *)t->t_obj;
        assert(!SLIST_EMPTY(&os_obj->obj_head));
        SLIST_REMOVE(&os_obj->obj_head, t, os_task, t_obj_list);
        SLIST_NEXT(t, t_obj_list) = NULL;
        t->t_obj = NULL;
    }

    /* Remove task from sleep list */
    t->t_state = OS_TASK_READY;
    t->t_next_wakeup = 0;
    t->t_flags &= ~OS_TASK_FLAG_NO_TIMEOUT;
    TAILQ_REMOVE(&g_os_sleep_list, t, t_os_list);
    os_sched_insert(t);

    os_trace_task_start_ready(t);

    return (0);
}

void
os_sched_os_timer_exp(void)
{
    struct os_task *t;
    struct os_task *next;
    os_time_t now;
    os_sr_t sr;

    now = os_time_get();

    OS_ENTER_CRITICAL(sr);

    /*
     * Wakeup any tasks that have their sleep timer expired
     */
    t = TAILQ_FIRST(&g_os_sleep_list);
    while (t) {
        /* If task waiting forever, do not check next wakeup time */
        if (t->t_flags & OS_TASK_FLAG_NO_TIMEOUT) {
            break;
        }
        next = TAILQ_NEXT(t, t_os_list);
        if (OS_TIME_TICK_GEQ(now, t->t_next_wakeup)) {
            os_sched_wakeup(t);
        } else {
            break;
        }
        t = next;
    }

    OS_EXIT_CRITICAL(sr);
}

os_time_t
os_sched_wakeup_ticks(os_time_t now)
{
    os_time_t rt;
    struct os_task *t;

    OS_ASSERT_CRITICAL();

    t = TAILQ_FIRST(&g_os_sleep_list);
    if (t == NULL || (t->t_flags & OS_TASK_FLAG_NO_TIMEOUT)) {
        rt = OS_TIMEOUT_NEVER;
    } else if (OS_TIME_TICK_GEQ(t->t_next_wakeup, now)) {
        rt = t->t_next_wakeup - now;
    } else {
        rt = 0;     /* wakeup time was in the past */
    }
    return (rt);
}

struct os_task *
os_sched_next_task(void)
{
    return (TAILQ_FIRST(&g_os_run_list));
}

void
os_sched_resort(struct os_task *t)
{
    if (t->t_state == OS_TASK_READY) {
        TAILQ_REMOVE(&g_os_run_list, t, t_os_list);
        os_sched_insert(t);
    }
}
