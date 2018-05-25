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

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include "os/mynewt.h"
#include "hal/hal_flash.h"
#include "hal/hal_system.h"
#include "testutil/testutil.h"
#include "testutil_priv.h"

/* The test task runs at a lower priority (greater number) than the default
 * task.  This allows the test task to assume events get processed as soon as
 * they are initiated.  The test code can then immediately assert the expected
 * result of event processing.
 */
#define TU_TEST_TASK_PRIO   (MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 1)
#define TU_TEST_STACK_SIZE  1024

struct tc_config tc_config;
struct tc_config *tc_current_config = &tc_config;

struct ts_config ts_config;
struct ts_config *ts_current_config = &ts_config;

int tu_any_failed;

struct ts_testsuite_list *ts_suites;

void
tu_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

#if MYNEWT_VAL(SELFTEST)
    os_init(NULL);
    ts_config.ts_print_results = 1;
#endif
}

void
tu_arch_restart(void)
{
#if MYNEWT_VAL(SELFTEST)
    os_arch_os_stop();
    tu_case_abort();
#else
    hal_system_reset();
#endif
}

void
tu_restart(void)
{
    tu_case_write_pass_auto();

    if (ts_config.ts_restart_cb != NULL) {
        ts_config.ts_restart_cb(ts_config.ts_restart_arg);
    }

    tu_arch_restart();
}

static void
tu_dflt_task_handler(void *arg)
{
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
}

static void
tu_create_dflt_task(void)
{
    static os_stack_t stack[OS_STACK_ALIGN(OS_MAIN_STACK_SIZE)];
    static struct os_task task;

    os_task_init(&task, "tu_dflt_task", tu_dflt_task_handler, NULL,
                 OS_MAIN_TASK_PRIO, OS_WAIT_FOREVER, stack,
                 OS_STACK_ALIGN(OS_MAIN_STACK_SIZE));
}

/**
 * Creates the "test task."  For test cases running in the OS, this is the task
 * that contains the actual test logic.
 */
static void
tu_create_test_task(const char *task_name, os_task_func_t task_handler)
{
    static os_stack_t stack[OS_STACK_ALIGN(TU_TEST_STACK_SIZE)];
    static struct os_task task;

    os_task_init(&task, task_name, task_handler, NULL,
                 TU_TEST_TASK_PRIO, OS_WAIT_FOREVER, stack,
                 OS_STACK_ALIGN(TU_TEST_STACK_SIZE));

    os_start();
}

/**
 * Creates the default task, creates the test task to run a test case in, and
 * starts the OS.
 */
void
tu_start_os(const char *test_task_name, os_task_func_t test_task_handler)
{
    sysinit();

    tu_create_dflt_task();
    tu_create_test_task(test_task_name, test_task_handler);

    os_start();
}
