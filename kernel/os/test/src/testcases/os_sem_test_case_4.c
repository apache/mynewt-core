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
#include "os_test_priv.h"

TEST_CASE(os_sem_test_case_4)
{
    os_error_t err;

#if MYNEWT_VAL(SELFTEST)
    sysinit();
#endif

    err = os_sem_init(&g_sem1, 1);
    TEST_ASSERT(err == OS_OK);

    runtest_init_task(sem_test_sleep_task_handler,
                      MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 1);
    runtest_init_task(sem_test_4_task2_handler,
                      MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 2);
    runtest_init_task(sem_test_4_task4_handler,
                      MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 3);
    runtest_init_task(sem_test_4_task4_handler,
                      MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 4);
}
