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

#include "os/mynewt.h"
#include "runtest/runtest.h"
#include "taskpool/taskpool.h"
#include "os_test_priv.h"

TEST_CASE_TASK(os_mutex_test_case_2)
{
    g_mutex_test = 2;
    g_task1_val = 0;
    g_task2_val = 0;
    g_task3_val = 0;
    os_mutex_init(&g_mutex1);
    os_mutex_init(&g_mutex2);

    taskpool_alloc_assert(mutex_test2_task1_handler,
                          MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 2);
    taskpool_alloc_assert(mutex_task2_handler,
                          MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 3);
    taskpool_alloc_assert(mutex_task3_handler,
                          MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 4);
    taskpool_alloc_assert(mutex_task4_handler,
                          MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 5);

    taskpool_wait_assert(200);
}
