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
#include "syscfg/syscfg.h"
#if !MYNEWT_VAL(OS_SYSVIEW_TRACE_MUTEX)
#define OS_TRACE_DISABLE_FILE_API
#endif
#include "os/mynewt.h"

os_error_t
os_mutex_init(struct os_mutex *mu)
{
    os_error_t ret;

    if (!mu) {
        ret = OS_INVALID_PARM;
        goto done;
    }

    os_trace_api_u32(OS_TRACE_ID_MUTEX_INIT, (uint32_t)mu);

    /* Initialize to 0 */
    mu->mu_prio = 0;
    mu->mu_level = 0;
    mu->mu_owner = NULL;
    SLIST_FIRST(&mu->mu_head) = NULL;

    ret = OS_OK;

done:
    os_trace_api_ret_u32(OS_TRACE_ID_MUTEX_INIT, (uint32_t)ret);
    return ret;
}

os_error_t
os_mutex_release(struct os_mutex *mu)
{
    int resched;
    os_sr_t sr;
    struct os_task *current;
    struct os_task *rdy;
    os_error_t ret;

    os_trace_api_u32(OS_TRACE_ID_MUTEX_RELEASE, (uint32_t)mu);

    /* Check if OS is started */
    if (!g_os_started) {
        ret = OS_NOT_STARTED;
        goto done;
    }

    /* Check for valid mutex */
    if (!mu) {
        ret = OS_INVALID_PARM;
        goto done;
    }

    /* We better own this mutex! */
    current = os_sched_get_current_task();
    if ((mu->mu_level == 0) || (mu->mu_owner != current)) {
        ret = OS_BAD_MUTEX;
        goto done;
    }

    /* Decrement nesting level by 1. If not zero, nested (so dont release!) */
    --mu->mu_level;
    if (mu->mu_level != 0) {
        ret = OS_OK;
        goto done;
    }

    OS_ENTER_CRITICAL(sr);

    /* Restore owner task's priority; resort list if different  */
    if (current->t_prio != mu->mu_prio) {
        current->t_prio = mu->mu_prio;
        os_sched_resort(current);
    }

    /* Check if tasks are waiting for the mutex */
    rdy = SLIST_FIRST(&mu->mu_head);
    if (rdy) {
        /* There is one waiting. Wake it up */
        assert(rdy->t_obj);
        os_sched_wakeup(rdy);

        /* Set mutex internals */
        mu->mu_level = 1;
        mu->mu_prio = rdy->t_prio;
    }

    /* Set new owner of mutex (or NULL if not owned) */
    mu->mu_owner = rdy;
    if (rdy) {
        rdy->t_lockcnt++;
    }
    --current->t_lockcnt;

    /* Do we need to re-schedule? */
    resched = 0;
    rdy = os_sched_next_task();
    if (rdy != current) {
        resched = 1;
    }
    OS_EXIT_CRITICAL(sr);

    /* Re-schedule if needed */
    if (resched) {
        os_sched(rdy);
    }

    ret = OS_OK;

done:
    os_trace_api_ret_u32(OS_TRACE_ID_MUTEX_RELEASE, (uint32_t)ret);
    return ret;
}

os_error_t
os_mutex_pend(struct os_mutex *mu, uint32_t timeout)
{
    os_sr_t sr;
    os_error_t ret;
    struct os_task *current;
    struct os_task *entry;
    struct os_task *last;

    os_trace_api_u32x2(OS_TRACE_ID_MUTEX_PEND, (uint32_t)mu, (uint32_t)timeout);

    /* OS must be started when calling this function */
    if (!g_os_started) {
        ret = OS_NOT_STARTED;
        goto done;
    }

    /* Check for valid mutex */
    if (!mu) {
        ret = OS_INVALID_PARM;
        goto done;
    }

    OS_ENTER_CRITICAL(sr);

    /* Is this owned? */
    current = os_sched_get_current_task();
    if (mu->mu_level == 0) {
        mu->mu_owner = current;
        mu->mu_prio  = current->t_prio;
        current->t_lockcnt++;
        mu->mu_level = 1;
        OS_EXIT_CRITICAL(sr);
        ret = OS_OK;
        goto done;
    }

    /* Are we owner? */
    if (mu->mu_owner == current) {
        ++mu->mu_level;
        OS_EXIT_CRITICAL(sr);
        ret = OS_OK;
        goto done;
    }

    /* Mutex is not owned by us. If timeout is 0, return immediately */
    if (timeout == 0) {
        OS_EXIT_CRITICAL(sr);
        ret = OS_TIMEOUT;
        goto done;
    }

    /* Change priority of owner if needed */
    if (mu->mu_owner->t_prio > current->t_prio) {
        mu->mu_owner->t_prio = current->t_prio;
        os_sched_resort(mu->mu_owner);
    }

    /* Link current task to tasks waiting for mutex */
    last = NULL;
    if (!SLIST_EMPTY(&mu->mu_head)) {
        /* Insert in priority order */
        SLIST_FOREACH(entry, &mu->mu_head, t_obj_list) {
            if (current->t_prio < entry->t_prio) {
                break;
            }
            last = entry;
        }
    }

    if (last) {
        SLIST_INSERT_AFTER(last, current, t_obj_list);
    } else {
        SLIST_INSERT_HEAD(&mu->mu_head, current, t_obj_list);
    }

    /* Set mutex pointer in task */
    current->t_obj = mu;
    current->t_flags |= OS_TASK_FLAG_MUTEX_WAIT;
    os_sched_sleep(current, timeout);
    OS_EXIT_CRITICAL(sr);

    os_sched(NULL);

    OS_ENTER_CRITICAL(sr);
    current->t_flags &= ~OS_TASK_FLAG_MUTEX_WAIT;
    OS_EXIT_CRITICAL(sr);

    /* If we are owner we did not time out. */
    if (mu->mu_owner == current) {
        ret = OS_OK;
    } else {
        ret = OS_TIMEOUT;
    }

done:
    os_trace_api_ret_u32(OS_TRACE_ID_MUTEX_PEND, (uint32_t)ret);
    return ret;
}

