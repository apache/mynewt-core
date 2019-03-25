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

#ifndef H_TASKPOOL_
#define H_TASKPOOL_

#include "os/mynewt.h"

/**
 * @file taskpool.h
 * @brief Allows you to create and remove generic tasks at runtime.
 *
 * This package creates a single global task pool.  Each allocated task
 * uses the same size stack.  The task count and stack size settings are
 * specified via syscfg.
 */

/**
 * @brief Allocates a new task from the global task pool.
 *
 * A task allocated with this function is allowed to terminate via return.
 * That is, if its handler function runs to completion, the task complete and
 * can be collected.
 *
 * @param task_handler          The function to be executed by the new task.
 * @param prio                  The priority to assign to the new task.
 * @param out_task              On success, the address ofthe newly allocated
 *                                  task gets written here.  Pass NULL if you
 *                                  don't require this information.
 *
 * @return                      0 on success; SYS_E[...] error code on failure.
 */
int taskpool_alloc(os_task_func_t task_handler, uint8_t prio,
                   struct os_task **out_task);

/**
 * @brief Allocates a new task and asserts success.
 *
 * See taskpool_alloc() for details.
 *
 * @param task_handler          The function to be executed by the new task.
 * @param prio                  The priority to assign to the new task.
 *
 * @return                      The newly allocateed task.
 */
struct os_task *taskpool_alloc_assert(os_task_func_t task_handler,
                                      uint8_t prio);

/**
 * @brief Waits for all allocated tasks to complete.
 *
 * @param max_ticks             The maximum duration to wait before the wait
 *                                  operation times out.  Units are OS ticks.
 *
 * @return                      0 on success; 
 *                              OS_TIMEOUT on timeout.
 */
int taskpool_wait(os_time_t max_ticks);

/**
 * @brief Waits for all allocated tasks to complete and asserts success.
 *
 * @param max_ticks             The maximum duration to wait before the wait
 *                                  operation times out.  Units are OS ticks.
 */
void taskpool_wait_assert(os_time_t max_ticks);

#endif
