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

/**
 * The taskpool module implements a reusable task pool.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "os/mynewt.h"
#include "taskpool/taskpool.h"

#define TASKPOOL_STATE_UNUSED   0
#define TASKPOOL_STATE_ACTIVE   1
#define TASKPOOL_STATE_DONE     2

/** Ensures thread safety across the taskpool API. */
static struct os_mutex taskpool_mtx;

/** Signals completion of final task to waiters. */
static struct os_sem taskpool_wait_sem;

/** The number of tasks waiting for all taskpool tasks to complete. */
static int taskpool_waiter_count;

/** Represents a single taskpool task. */
struct taskpool_entry {
    OS_TASK_STACK_DEFINE_NOSTATIC(stack, MYNEWT_VAL(TASKPOOL_STACK_SIZE));
    os_task_func_t fn;
    struct os_task task;
    uint8_t state;
    char name[sizeof "taskXX"];
};

static struct taskpool_entry taskpool_entries[MYNEWT_VAL(TASKPOOL_NUM_TASKS)];

static void
taskpool_lock(void)
{
    int rc;

    rc = os_mutex_pend(&taskpool_mtx, OS_TIMEOUT_NEVER);
    assert(rc == 0 || rc == OS_NOT_STARTED);
}

static void
taskpool_unlock(void)
{
    int rc;

    rc = os_mutex_release(&taskpool_mtx);
    assert(rc == 0 || rc == OS_NOT_STARTED);
}

static bool
taskpool_locked(void)
{
    struct os_task *owner;

    owner = taskpool_mtx.mu_owner;
    return owner != NULL && owner == os_sched_get_current_task();
}

#define TASKPOOL_ASSERT_LOCKED() do  \
{                                   \
    if (os_started()) {             \
        assert(taskpool_locked());   \
    }                               \
} while (0)

static int
taskpool_find_state(uint8_t state)
{
    int i;

    TASKPOOL_ASSERT_LOCKED();

    for (i = 0; i < MYNEWT_VAL(TASKPOOL_NUM_TASKS); i++) {
        if (taskpool_entries[i].state == state) {
            return i;
        }
    }

    return -1;
}

static void
taskpool_wrapper(void *arg)
{
    struct taskpool_entry *entry;

    entry = arg;

    /* Execute wrapped task handler. */
    entry->fn(NULL);

    taskpool_lock();

    /* Mark task done. */
    entry->state = TASKPOOL_STATE_DONE;

    /* If this was the last running task, signal that the test is complete. */
    if (taskpool_find_state(TASKPOOL_STATE_ACTIVE) == -1) {
        while (taskpool_waiter_count > 0) {
            os_sem_release(&taskpool_wait_sem);
            taskpool_waiter_count--;
        }
    }

    taskpool_unlock();

    /* Block forever.  This task can now be collected with os_task_remove(). */
    while (1) {
        os_time_delay(OS_STIME_MAX);
    }
}

int
taskpool_alloc(os_task_func_t task_handler, uint8_t prio,
               struct os_task **out_task)
{
    struct taskpool_entry *entry;
    int idx;
    int rc;

    taskpool_lock();

    idx = taskpool_find_state(TASKPOOL_STATE_UNUSED);
    if (idx != -1) {
        entry = &taskpool_entries[idx];
        entry->state = TASKPOOL_STATE_ACTIVE;
    }

    taskpool_unlock();

    if (idx == -1) {
        return SYS_ENOMEM;
    }

    entry->fn = task_handler;
    snprintf(entry->name, sizeof entry->name, "task%02d", idx);

    rc = os_task_init(&entry->task, entry->name, taskpool_wrapper,
                      entry, prio, OS_WAIT_FOREVER, entry->stack,
                      MYNEWT_VAL(TASKPOOL_STACK_SIZE));
    if (rc != 0) {
        taskpool_lock();
        entry->state = TASKPOOL_STATE_UNUSED;
        taskpool_unlock();

        return rc;
    }

    *out_task = &entry->task;
    return 0;
}

struct os_task *
taskpool_alloc_assert(os_task_func_t task_handler, uint8_t prio)
{
    struct os_task *task;
    int rc;

    rc = taskpool_alloc(task_handler, prio, &task);
    assert(rc == 0);

    return task;
}

static void
taskpool_reset(void)
{
    int rc;
    int i;

    taskpool_lock();

    for (i = 0; i < MYNEWT_VAL(TASKPOOL_NUM_TASKS); i++) {
        if (taskpool_entries[i].state != TASKPOOL_STATE_UNUSED) {
            rc = os_task_remove(&taskpool_entries[i].task);
            assert(rc == OS_OK);

            taskpool_entries[i].state = TASKPOOL_STATE_UNUSED;
        }
    }

    taskpool_unlock();
}

int
taskpool_wait(os_time_t max_ticks)
{
    int any_tasks;
    int rc;

    taskpool_lock();

    any_tasks = taskpool_find_state(TASKPOOL_STATE_ACTIVE) != -1;
    if (any_tasks) {
        taskpool_waiter_count++;
    }

    taskpool_unlock();

    if (!any_tasks) {
        /* No active tasks; nothing to wait on. */
        return 0;
    }

    rc = os_sem_pend(&taskpool_wait_sem, max_ticks);
    if (rc != 0) {
        return rc;
    }

    /* Collect all the completed taskpool tasks. */
    taskpool_reset();

    return 0;
}

void
taskpool_wait_assert(os_time_t max_ticks)
{
    int rc;

    rc = taskpool_wait(max_ticks);
    assert(rc == 0);
}

/*
 * Package init routine to register newtmgr "run" commands
 */
void
taskpool_init(void)
{
    int rc;
    int i;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = os_mutex_init(&taskpool_mtx);
    SYSINIT_PANIC_ASSERT(rc == 0 || rc == OS_NOT_STARTED);

    rc = os_sem_init(&taskpool_wait_sem, 0);
    SYSINIT_PANIC_ASSERT(rc == 0 || rc == OS_NOT_STARTED);

    for (i = 0; i < MYNEWT_VAL(TASKPOOL_NUM_TASKS); i++) {
        taskpool_entries[i].state = TASKPOOL_STATE_UNUSED;
    }
}
