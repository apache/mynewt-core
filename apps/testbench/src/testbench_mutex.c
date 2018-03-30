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
#include <assert.h>
#include <sys/time.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"

#include "testbench.h"

#ifdef ARCH_sim
#define MUTEX_TEST_STACK_SIZE   1024
#else
#define MUTEX_TEST_STACK_SIZE   256
#endif

struct os_mutex g_mutex1;
struct os_mutex g_mutex2;

volatile int g_mutex_test;

void
testbench_mutex_ts_init(void *arg)
{
    LOG_DEBUG(&testlog, LOG_MODULE_TEST,
             "%s starting %s", buildID, tu_case_name);
}

/*
 * "Suspend" the test worker tasks after each test completes.
 * These tasks can be re-used for other tests, the stack also can be re-used.
 */
void
testbench_mutex_tc_posttest(void* arg)
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
 * "Suspend" the test worker tasks after each test completes.
 * These tasks can be re-used for other tests, the stack also can be re-used.
 */
void
testbench_mutex_posttest(void* arg)
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
 * callback run before testbench_mutex_suite starts
 */
void
testbench_mutex_init(void *arg)
{
    tu_case_idx = 0;
    tu_case_failed = 0;

    LOG_DEBUG(&testlog, LOG_MODULE_TEST,
             "%s testbench test_init", buildID);

    tu_suite_set_pass_cb(testbench_ts_pass, NULL);
    tu_suite_set_fail_cb(testbench_ts_fail, NULL);
}

TEST_CASE_DECL(os_mutex_test_basic)
TEST_CASE_DECL(os_mutex_test_case_1)
TEST_CASE_DECL(os_mutex_test_case_2)

TEST_SUITE(testbench_mutex_suite)
{
    int taskcount;

    LOG_DEBUG(&testlog, LOG_MODULE_TEST,
             "%s mutex_suite start", buildID);

    taskcount = 1;
    tu_case_set_post_cb(testbench_mutex_tc_posttest, (void*)taskcount);
    os_mutex_test_basic();

    taskcount = 3;
    tu_case_set_post_cb(testbench_mutex_tc_posttest, (void*)taskcount);
    os_mutex_test_case_1();

    taskcount = 4;
    tu_case_set_post_cb(testbench_mutex_tc_posttest, (void*)taskcount);
    os_mutex_test_case_2();
}

int
testbench_mutex()
{
    tu_suite_set_init_cb(testbench_mutex_init, NULL);

    testbench_mutex_suite();

    return tu_any_failed;
}
