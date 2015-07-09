/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "os/os.h"
#include "os/queue.h"

#include <assert.h>

TAILQ_HEAD(, os_task) g_os_run_list = TAILQ_HEAD_INITIALIZER(g_os_run_list); 

TAILQ_HEAD(, os_task) g_os_sleep_list = 
    TAILQ_HEAD_INITIALIZER(g_os_sleep_list); 

struct os_task *g_current_task; 


/**
 * Insert a newly created task into the scheduler list.  This causes the task to 
 * be evaluated for running when os_scheduler_run() is called.
 */
int 
os_sched_insert(struct os_task *t, int isr) 
{
    struct os_task *entry; 
    os_sr_t sr; 
    int rc;

    if (t->t_state != OS_TASK_READY) {
        rc = OS_EINVAL;
        goto err;
    }

    entry = NULL;
    if (!isr) {
        OS_ENTER_CRITICAL(sr); 
    }
    TAILQ_FOREACH(entry, &g_os_run_list, t_run_list) {
        if (t->t_prio < entry->t_prio) { 
            break;
        }
    }
    if (entry) {
        TAILQ_INSERT_BEFORE(entry, (struct os_task *) t, t_run_list);
    } else {
        TAILQ_INSERT_TAIL(&g_os_run_list, (struct os_task *) t, t_run_list);
    }
    if (!isr) {
        OS_EXIT_CRITICAL(sr);
    }

    return (0);
err:
    return (rc);
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
os_sched(struct os_task *next_t, int isr) 
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);

    if (!next_t) {
        next_t = os_sched_next_task(isr);
    }

    if (next_t != os_sched_get_current_task()) {
        os_arch_ctx_sw(next_t);
        OS_EXIT_CRITICAL(sr);
    } else {
        OS_EXIT_CRITICAL(sr);
    }
}


int 
os_sched_sleep(struct os_task *t, os_time_t nticks) 
{
    struct os_task *entry;
    os_sr_t sr; 

    entry = NULL; 

    OS_ENTER_CRITICAL(sr);
    TAILQ_REMOVE(&g_os_run_list, t, t_run_list);
    t->t_state = OS_TASK_SLEEP;
    t->t_next_wakeup = os_time_get() + nticks;
    if (nticks == OS_TIMEOUT_NEVER) {
        t->t_flags |= OS_TASK_FLAG_NO_TIMEOUT;
        TAILQ_INSERT_TAIL(&g_os_sleep_list, t, t_sleep_list); 
    } else {
        TAILQ_FOREACH(entry, &g_os_sleep_list, t_sleep_list) {
            if ((t->t_flags & OS_TASK_FLAG_NO_TIMEOUT) ||
                    OS_TIME_TICK_GT(entry->t_next_wakeup, t->t_next_wakeup)) {
                break;
            }
        }
        if (entry) {
            TAILQ_INSERT_BEFORE(entry, t, t_sleep_list); 
        } else {
            TAILQ_INSERT_TAIL(&g_os_sleep_list, t, t_sleep_list); 
        }
    }
    OS_EXIT_CRITICAL(sr);

    os_sched(NULL, 0);

    return (0);
}

int 
os_sched_wakeup(struct os_task *t, int sched_now, int isr) 
{
    os_sr_t sr; 

    if (!isr) {
        OS_ENTER_CRITICAL(sr); 
    }

    /* Remove self from mutex list if waiting on one */
    if (t->t_mutex) {
        assert(!SLIST_EMPTY(&t->t_mutex->mu_head));
        SLIST_REMOVE(&t->t_mutex->mu_head, t, os_task, t_mutex_list);
        SLIST_NEXT(t, t_mutex_list) = NULL;
        t->t_mutex = NULL; 
    }

    /* Remove task from sleep list */
    t->t_state = OS_TASK_READY;
    t->t_next_wakeup = 0;
    t->t_flags &= ~OS_TASK_FLAG_NO_TIMEOUT;
    TAILQ_REMOVE(&g_os_sleep_list, t, t_sleep_list);
    os_sched_insert(t, isr);
    if (!isr) {
        OS_EXIT_CRITICAL(sr);
    }

    if (sched_now) {
        os_sched(NULL, isr);
    }

    return (0);
}


/**
 * Get the next task to run. 
 */
struct os_task *  
os_sched_next_task(int isr) 
{
    struct os_task *t; 
    os_time_t now; 
    os_sr_t sr;

    now = os_time_get();

    if (!isr) {
        OS_ENTER_CRITICAL(sr);
    }
    /*
     * Wakeup any tasks that have their sleep timer expired
     */
    TAILQ_FOREACH(t, &g_os_sleep_list, t_sleep_list) {
        /* If task waiting forever, do not check next wakeup time */
        if (t->t_flags & OS_TASK_FLAG_NO_TIMEOUT) {
            break;
        }
        if (OS_TIME_TICK_GEQ(now, t->t_next_wakeup)) {
            os_sched_wakeup(t, 0, isr);
        } else {
            break;
        }
    }

    /* 
     * Run the head of the run list
     */
    t = TAILQ_FIRST(&g_os_run_list);
    if (!isr) {
        OS_EXIT_CRITICAL(sr); 
    }

    return (t);
}

/**
 * os sched resort 
 *  
 * Resort a task that is in the ready list as its priority has 
 * changed. If the task is not in the ready state, there is 
 * nothing to do. 
 * 
 * @param t Pointer to task to insert back into ready to run 
 *          list.
 *  
 * NOTE: this function expects interrupts to be disabled so they 
 * are not disabled here. 
 */
void 
os_sched_resort(struct os_task *t) 
{
    if (t->t_state == OS_TASK_READY) {
        TAILQ_REMOVE(&g_os_run_list, t, t_run_list);
        os_sched_insert(t, 0);
    }
}

