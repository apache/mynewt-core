/**
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

#include <stdio.h>
#include <string.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"

#include "testbench.h"

#define SEM_TEST_STACK_SIZE     256

struct os_sem g_sem1;

void
testbench_sem_ts_init(void *arg)
{
    LOG_DEBUG(&testlog, LOG_MODULE_TEST,
             "%s starting %s", buildID, tu_case_name);
}

void sem_test_basic_handler(void* arg);

void
testbench_sem_posttest(void* arg)
{
    os_error_t err;
    int taskcount = (int) arg;

    if (taskcount >= 1) {
        err = os_task_remove(&task1);
        TEST_ASSERT(err == OS_OK);
    }
    if (taskcount >= 2) {
        err = os_task_remove(&task2);
        TEST_ASSERT(err == OS_OK);
    }
    if (taskcount >= 3) {
        err = os_task_remove(&task3);
        TEST_ASSERT(err == OS_OK);
    }
    if (taskcount >= 4) {
        err = os_task_remove(&task4);
        TEST_ASSERT(err == OS_OK);
    }

    return;
}

/*
 * run before testbench_sem starts executing
 */
void
testbench_sem_init(void *arg)
{
    tu_case_idx = 0;
    tu_case_failed = 0;

    LOG_DEBUG(&testlog, LOG_MODULE_TEST, "%s testbench_sem suite init",
              buildID);

    tu_suite_set_pass_cb(testbench_ts_pass, NULL);
    tu_suite_set_fail_cb(testbench_ts_fail, NULL);
}

TEST_CASE(os_sem_test_null)
{
    return;
}

TEST_CASE(os_sem_test_fail)
{
    extern int forcefail;

    if (forcefail) {
        TEST_ASSERT(0);
    }
    return;
}

TEST_CASE_DECL(os_sem_test_null)
TEST_CASE_DECL(os_sem_test_fail)
TEST_CASE_DECL(os_sem_test_basic)
TEST_CASE_DECL(os_sem_test_case_1)
TEST_CASE_DECL(os_sem_test_case_2)
TEST_CASE_DECL(os_sem_test_case_3)
TEST_CASE_DECL(os_sem_test_case_4)

TEST_SUITE(testbench_sem_suite)
{
    int taskcount;

    os_sem_test_null();

    taskcount = 1;
    tu_case_set_post_cb(testbench_sem_posttest, (void*)taskcount);
    os_sem_test_basic();

    taskcount = 3;
    tu_case_set_post_cb(testbench_sem_posttest, (void*)taskcount);
    os_sem_test_case_1();

    taskcount = 4;
    tu_case_set_post_cb(testbench_sem_posttest, (void*)taskcount);
    os_sem_test_case_2();

    tu_case_set_post_cb(testbench_sem_posttest, (void*)taskcount);
    os_sem_test_case_3();

    tu_case_set_post_cb(testbench_sem_posttest, (void*)taskcount);
    os_sem_test_case_4();
}

void
testbench_sem()
{
    tu_suite_set_init_cb(testbench_sem_init, NULL);
    testbench_sem_suite();

    LOG_DEBUG(&testlog, LOG_MODULE_TEST,
             "%s testbench_sem suite complete", buildID);

    return;
}
