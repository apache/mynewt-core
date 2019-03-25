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
#include "taskpool/taskpool.h"
#include "os_test_priv.h"

TEST_CASE_SELF(os_sem_test_case_1)
{
    os_error_t err;

    err = os_sem_init(&g_sem1, 1);
    TEST_ASSERT(err == OS_OK);

    taskpool_alloc_assert(sem_test_1_task1_handler,
                          MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 1);
    taskpool_alloc_assert(sem_test_1_task2_handler,
                          MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 2);
    taskpool_alloc_assert(sem_test_1_task3_handler,
                          MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 3);
}
