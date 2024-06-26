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
  * @addtogroup OSKernel
  * @{
  *   @defgroup OSSched Scheduler
  *   @{
  */

#ifndef _OS_SCHED_H
#define _OS_SCHED_H

#include "os/os_task.h"

#ifdef __cplusplus
extern "C" {
#endif


/** @cond INTERNAL_HIDDEN */
struct os_task;

TAILQ_HEAD(os_task_list, os_task);

extern struct os_task *g_current_task;
extern struct os_task_list g_os_run_list;
extern struct os_task_list g_os_sleep_list;

void os_sched_ctx_sw_hook(struct os_task *next_t);

/** @endcond */

/**
 * Returns the currently running task. Note that this task may or may not be
 * the highest priority task ready to run.
 *
 * @return                      The currently running task.
 */
struct os_task *os_sched_get_current_task(void);

/**
 * Sets the currently running task to 't'. Note that this function simply sets
 * the global variable holding the currently running task. It does not perform
 * a context switch or change the os run or sleep list.
 *
 * @param t                     Pointer to currently running task.
 */
void os_sched_set_current_task(struct os_task *t);

/**
 * Returns the task that we should be running. This is the task at the head
 * of the run list.
 *
 * @note                        If you want to guarantee that the os run list
 *                                  does not change after calling this function,
 *                                  you have to call it with interrupts disabled.
 *
 * @return                      The task at the head of the list
 */
struct os_task *os_sched_next_task(void);

/**
 * Suspend task scheduling
 *
 * Function suspends the scheduler.
 * Suspending the scheduler prevents a context switch but leaves interrupts enabled.
 * Call to os_sched_resume() enables task scheduling again.
 * Calls to os_sched_suspend() can be nested. The same number of calls must be made
 * to os_sched_resume() as have previously been made to os_sched_suspend() before
 * task scheduling work again.
 */
void os_sched_suspend(void);

/**
 * Resume task scheduling
 *
 * Resumes the scheduler after it was suspended with os_sched_suspend().
 * @return                      0 when scheduling resumed;
 *                              non-0 when scheduling is still locked and more
 *                                  calls to os_sched_resume() are needed
 */
int os_sched_resume(void);

/**
 * Performs context switch if needed. If next_t is set, that task will be made
 * running. If next_t is NULL, highest priority ready to run is swapped in. This
 * function can be called when new tasks were made ready to run or if the current
 * task is moved to sleeping state.
 *
 * This function will call the architecture specific routine to swap in the new task.
 *
 * @param next_t                Pointer to task which must run next (optional)
 *
 * @note                        Interrupts must be disabled when calling this.
 *
 * @code{.c}
 * // example
 * os_error_t
 * os_mutex_release(struct os_mutex *mu)
 * {
 *     ...
 *     OS_EXIT_CRITICAL(sr);
 *
 *     // Re-schedule if needed
 *     if (resched) {
 *         os_sched(rdy);
 *     }
 *
 *     return OS_OK;
 *
 * }
 * @endcode
 */
void os_sched(struct os_task *next_t);

/** @cond INTERNAL_HIDDEN */

/**
 * Called when the OS tick timer expires. Search the sleep list for any tasks
 * that need waking up. This occurs when the current OS time exceeds the next
 * wakeup time stored in the task. Any tasks that need waking up will be
 * removed from the sleep list and added to the run list.
 */
void os_sched_os_timer_exp(void);

/**
 * Insert a task into the scheduler list. This causes the task to be evaluated
 * for running when os_sched is called.
 *
 * @param t                     Pointer to task to insert in run list
 *
 * @return                      OS_OK: task was inserted into run list;
 *                              OS_EINVAL: Task was not in ready state.
 */
os_error_t os_sched_insert(struct os_task *t);

/**
 * Removes the task from the run list and puts it on the sleep list.
 *
 * @param t                     Task to put to sleep
 * @param nticks                Number of ticks to put task to sleep
 *
 * @return                      Zero on success
 *
 * @note                        Must be called with interrupts disabled! This
 *                                  function does not call the scheduler.
 */
int os_sched_sleep(struct os_task *t, os_time_t nticks);

/**
 * Called to wake up a task. Waking up a task consists of setting the task state
 * to READY and moving it from the sleep list to the run list.
 *
 * @param t                     Pointer to task to wake up.
 *
 * @return                      Zero on success
 *
 * @note                        This function must be called with interrupts
 *                                  disabled.
 */
int os_sched_wakeup(struct os_task *t);

/**
 * @note This routine is currently experimental and not ready for common use
 *
 * Stops a task and removes it from the task list.
 *
 * @return int
 *
 * @note                        Must be called with interrupts disabled!
 *                                  This function does not call the scheduler.
 */
int os_sched_remove(struct os_task *t);

/**
 * Resort a task that is in the ready list as its priority has
 * changed. If the task is not in the ready state, there is
 * nothing to do.
 *
 * @param t                     Pointer to task to insert back into ready to run
 *                                  list.
 *
 * @note                        This function expects interrupts to be disabled,
 *                                  so they are not disabled here.
 */
void os_sched_resort(struct os_task *t);

/**
 * Return the number of ticks until the first sleep timer expires. If there are
 * no such tasks then return OS_TIMEOUT_NEVER instead.
 */
os_time_t os_sched_wakeup_ticks(os_time_t now);

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* _OS_SCHED_H */

/**
 *   @} OSSched
 * @} OSKernel
 */
