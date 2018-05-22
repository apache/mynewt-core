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

void os_sched_ctx_sw_hook(struct os_task *);

/** @endcond */

/**
 * Returns the currently running task. Note that this task may or may not be
 * the highest priority task ready to run.
 *
 * @return The currently running task.
 */
struct os_task *os_sched_get_current_task(void);
void os_sched_set_current_task(struct os_task *);
struct os_task *os_sched_next_task(void);

/**
 * Performs context switch if needed. If next_t is set, that task will be made
 * running. If next_t is NULL, highest priority ready to run is swapped in. This
 * function can be called when new tasks were made ready to run or if the current
 * task is moved to sleeping state.
 *
 * This function will call the architecture specific routine to swap in the new task.
 *
 * @param next_t Pointer to task which must run next (optional)
 *
 * @return n/a
 *
 * @note Interrupts must be disabled when calling this.
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
void os_sched(struct os_task *);

/** @cond INTERNAL_HIDDEN */
void os_sched_os_timer_exp(void);
os_error_t os_sched_insert(struct os_task *);
int os_sched_sleep(struct os_task *, os_time_t nticks);
int os_sched_wakeup(struct os_task *);
int os_sched_remove(struct os_task *);
void os_sched_resort(struct os_task *);
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
