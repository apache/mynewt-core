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
#if !MYNEWT_VAL(OS_SYSVIEW_TRACE_SEM)
#define OS_TRACE_DISABLE_FILE_API
#endif
#include "os/mynewt.h"

/* XXX:
 * 1) Should I check to see if we are within an ISR for some of these?
 * 2) Would I do anything different for os_sem_release() if we were in an
 *    ISR when this was called?
 */

os_error_t
os_sem_init(struct os_sem *sem, uint16_t tokens)
{
    os_error_t ret;

    os_trace_api_u32x2(OS_TRACE_ID_SEM_INIT, (uint32_t)sem, (uint32_t)tokens);

    if (!sem) {
        ret = OS_INVALID_PARM;
        goto done;
    }

    sem->sem_tokens = tokens;
    SLIST_FIRST(&sem->sem_head) = NULL;

    ret = OS_OK;

done:
    os_trace_api_ret_u32(OS_TRACE_ID_SEM_INIT, (uint32_t)ret);
    return ret;
}

os_error_t
os_sem_release(struct os_sem *sem)
{
    int resched;
    os_sr_t sr;
    struct os_task *current;
    struct os_task *rdy;
    os_error_t ret;

    os_trace_api_u32(OS_TRACE_ID_SEM_INIT, (uint32_t)sem);

    /* OS must be started to release semaphores */
    if (!g_os_started) {
        ret = OS_NOT_STARTED;
        goto done;
    }

    /* Check for valid semaphore */
    if (!sem) {
        ret = OS_INVALID_PARM;
        goto done;
    }

    /* Get current task */
    resched = 0;
    current = os_sched_get_current_task();

    OS_ENTER_CRITICAL(sr);

    /* Check if tasks are waiting for the semaphore */
    rdy = SLIST_FIRST(&sem->sem_head);
    if (rdy) {
        /* Clear flag that we are waiting on the semaphore; wake up task */
        rdy->t_flags &= ~OS_TASK_FLAG_SEM_WAIT;
        os_sched_wakeup(rdy);

        /* Schedule if waiting task higher priority */
        if (current->t_prio > rdy->t_prio) {
            resched = 1;
        }
    } else {
        /* Add to the number of tokens */
        sem->sem_tokens++;
    }

    OS_EXIT_CRITICAL(sr);

    /* Re-schedule if needed */
    if (resched) {
        os_sched(rdy);
    }

    ret = OS_OK;

done:
    os_trace_api_ret_u32(OS_TRACE_ID_SEM_INIT, (uint32_t)ret);
    return ret;
}

os_error_t
os_sem_pend(struct os_sem *sem, uint32_t timeout)
{
    os_sr_t sr;
    int sched;
    struct os_task *current;
    struct os_task *entry;
    struct os_task *last;
    os_error_t ret;

    os_trace_api_u32x2(OS_TRACE_ID_SEM_PEND, (uint32_t)sem, (uint32_t)timeout);

    /* Check if OS is started */
    if (!g_os_started) {
        ret = OS_NOT_STARTED;
        goto done;
    }

    /* Check for valid semaphore */
    if (!sem) {
        ret = OS_INVALID_PARM;
        goto done;
    }

    /* Assume we dont have to put task to sleep; get current task */
    sched = 0;
    current = os_sched_get_current_task();

    OS_ENTER_CRITICAL(sr);

    /*
     * If there is a token available, take it. If no token, either return
     * with error if timeout was 0 or put this task to sleep.
     */
    if (sem->sem_tokens != 0) {
        sem->sem_tokens--;
        ret = OS_OK;
    } else if (timeout == 0) {
        ret = OS_TIMEOUT;
    } else {
        /* Silence gcc maybe-uninitialized warning. */
        ret = OS_OK;

        /* Link current task to tasks waiting for semaphore */
        current->t_obj = sem;
        current->t_flags |= OS_TASK_FLAG_SEM_WAIT;
        last = NULL;
        if (!SLIST_EMPTY(&sem->sem_head)) {
            /* Insert in priority order */
            SLIST_FOREACH(entry, &sem->sem_head, t_obj_list) {
                if (current->t_prio < entry->t_prio) {
                    break;
                }
                last = entry;
            }
        }

        if (last) {
            SLIST_INSERT_AFTER(last, current, t_obj_list);
        } else {
            SLIST_INSERT_HEAD(&sem->sem_head, current, t_obj_list);
        }

        /* We will put this task to sleep */
        sched = 1;
        os_sched_sleep(current, timeout);
    }

    OS_EXIT_CRITICAL(sr);

    if (sched) {
        os_sched(NULL);
        /* Check if we timed out or got the semaphore */
        if (current->t_flags & OS_TASK_FLAG_SEM_WAIT) {
            OS_ENTER_CRITICAL(sr);
            current->t_flags &= ~OS_TASK_FLAG_SEM_WAIT;
            OS_EXIT_CRITICAL(sr);
            ret = OS_TIMEOUT;
        } else {
            ret = OS_OK;
        }
    }

done:
    os_trace_api_ret_u32(OS_TRACE_ID_SEM_PEND, (uint32_t)ret);
    return ret;
}
