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
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "os/mynewt.h"
#include "console/console.h"
#include "testutil/testutil.h"

#include "runtest/runtest.h"
#include "runtest_priv.h"

#if MYNEWT_VAL(RUNTEST_CLI)
#include "shell/shell.h"
struct shell_cmd runtest_cmd_struct;
#endif

#if MYNEWT_VAL(RUNTEST_LOG)
#include "cbmem/cbmem.h"
#include "modlog/modlog.h"
static uint8_t runtest_cbmem_buf[MYNEWT_VAL(RUNTEST_LOG_SIZE)];
static struct cbmem runtest_cbmem;
static struct log runtest_log;
#endif

static void runtest_evt_fn(struct os_event *ev);

static int runtest_total_tests;
static int runtest_total_fails;
static struct ts_suite *runtest_current_ts;
static char runtest_token[MYNEWT_VAL(RUNTEST_MAX_TOKEN_LEN)];
static char runtest_test_name[MYNEWT_VAL(RUNTEST_MAX_TEST_NAME_LEN)];
static bool runtest_busy;

static struct os_eventq *runtest_evq;
static struct os_mutex runtest_mtx;
static struct os_event run_test_event = {
    .ev_cb = runtest_evt_fn,
};

struct runtest_task {
    struct os_task task;
    char name[sizeof "taskX"];
    OS_TASK_STACK_DEFINE_NOSTATIC(stack, MYNEWT_VAL(RUNTEST_STACK_SIZE));
};

static struct runtest_task runtest_tasks[MYNEWT_VAL(RUNTEST_NUM_TASKS)];

#define RUNTEST_BUILD_ID 

static int runtest_next_task_idx;

/**
 * Retrieves the event queue used by the runtest package.
 */
struct os_eventq *
runtest_evq_get(void)
{
    return runtest_evq;
}

void
runtest_evq_set(struct os_eventq *evq)
{
    runtest_evq = evq;
}

static void
runtest_lock(void)
{
    int rc;

    rc = os_mutex_pend(&runtest_mtx, OS_TIMEOUT_NEVER);
    assert(rc == 0);
}

static void
runtest_unlock(void)
{
    int rc;

    rc = os_mutex_release(&runtest_mtx);
    assert(rc == 0);
}

int
runtest_total_fails_get(void)
{
    return runtest_total_fails;
}

struct os_task *
runtest_init_task(os_task_func_t task_handler, uint8_t prio)
{
    struct os_task *task;
    os_stack_t *stack;
    char *name;
    int rc;

    if (runtest_next_task_idx >= MYNEWT_VAL(RUNTEST_NUM_TASKS)) {
        TEST_ASSERT_FATAL(0, "No more test tasks");
        return NULL;
    }

    task = &runtest_tasks[runtest_next_task_idx].task;
    stack = runtest_tasks[runtest_next_task_idx].stack;
    name = runtest_tasks[runtest_next_task_idx].name;

    strcpy(name, "task");
    name[4] = '0' + runtest_next_task_idx;
    name[5] = '\0';

    rc = os_task_init(task, name, task_handler, NULL,
                      prio, OS_WAIT_FOREVER, stack,
                      MYNEWT_VAL(RUNTEST_STACK_SIZE));
    TEST_ASSERT_FATAL(rc == 0);

    runtest_next_task_idx++;

    return task;
}

static void
runtest_reset(void)
{
    int rc;

    while (runtest_next_task_idx > 0) {
        runtest_next_task_idx--;

        rc = os_task_remove(&runtest_tasks[runtest_next_task_idx].task);
        assert(rc == OS_OK);
    }
}

static void
runtest_log_result(const char *msg, bool passed)
{
#if MYNEWT_VAL(RUNTEST_LOG)
    /* Must log valid json with a strlen less than LOG_PRINTF_MAX_ENTRY_LEN */
    char buf[LOG_PRINTF_MAX_ENTRY_LEN];
    char *n;
    int n_len;
    char *s;
    int s_len;
    char *m;
    int m_len;
    int len;

    /* str length of {"k":"","n":"","s":"","m":"","r":1}<token> */
    len = 35 + strlen(runtest_token);

    /* How much of the test name can we log? */
    n_len = strlen(tu_case_name);
    if (len + n_len >= LOG_PRINTF_MAX_ENTRY_LEN) {
        n_len = LOG_PRINTF_MAX_ENTRY_LEN - len - 1;
    }
    len += n_len;
    n = buf;
    strncpy(n, tu_case_name, n_len + 1);

    /* How much of the suite name can we log? */
    s_len = strlen(runtest_current_ts->ts_name);
    if (len + s_len >= LOG_PRINTF_MAX_ENTRY_LEN) {
        s_len = LOG_PRINTF_MAX_ENTRY_LEN - len - 1;
    }
    len += s_len;
    s = n + n_len + 2;
    strncpy(s, runtest_current_ts->ts_name, s_len + 1);

    /* How much of the message can we log? */
    m_len = strlen(msg);
    if (len + m_len >= LOG_PRINTF_MAX_ENTRY_LEN) {
        m_len = LOG_PRINTF_MAX_ENTRY_LEN - len - 1;
    }
    m = s + s_len + 2;
    strncpy(m, msg, m_len + 1);

    /* XXX Hack to allow the idle task to run and update the timestamp. */
    os_time_delay(1);

    runtest_total_tests++;
    if (!passed) {
        runtest_total_fails++;
    }

    MODLOG_INFO(
        LOG_MODULE_TEST,
        "{\"k\":\"%s\",\"n\":\"%s\",\"s\":\"%s\",\"m\":\"%s\",\"r\":%d}",
        runtest_token, n, s, m, passed);
#endif
}

static void
runtest_pass(char *msg, void *arg)
{
    runtest_reset();
    runtest_log_result(msg, true);
}

static void
runtest_fail(char *msg, void *arg)
{
    runtest_reset();
    runtest_log_result(msg, false);
}

static struct ts_suite *
runtest_find_test(const char *suite_name)
{
    struct ts_suite *cur;

    SLIST_FOREACH(cur, &g_ts_suites, ts_next) {
        if (strcmp(cur->ts_name, suite_name) == 0) {
            return cur;
        }
    }

    return NULL;
}

static void
runtest_test_complete(void)
{
#if MYNEWT_VAL(RUNTEST_LOG)
    if (runtest_total_tests > 0) {
        MODLOG_INFO(LOG_MODULE_TEST, "%s Done", runtest_token);
        MODLOG_INFO(LOG_MODULE_TEST,
                    "%s TESTBENCH TEST %s - Tests run:%d pass:%d fail:%d %s",
                    RUNTEST_PREFIX,
                    runtest_total_fails ? "FAILED" : "PASSED",
                    runtest_total_tests,
                    runtest_total_tests - runtest_total_fails,
                    runtest_total_fails,
                    runtest_token);
    }
#endif
    runtest_busy = false;
}

static void
runtest_evt_fn(struct os_event *ev)
{
    int runtest_all;

    runtest_total_tests = 0;
    runtest_total_fails = 0;

    tu_suite_set_pass_cb(runtest_pass, NULL);
    tu_suite_set_fail_cb(runtest_fail, NULL);

    if (ev != NULL) {
        ts_config.ts_print_results = 0;
        ts_config.ts_system_assert = 0;

        runtest_all = runtest_test_name[0] == '\0' ||
                      strcmp(runtest_test_name, "all") == 0;

        if (runtest_all) {
            SLIST_FOREACH(runtest_current_ts, &g_ts_suites, ts_next) {
                runtest_current_ts->ts_test();
            }
        } else {
            runtest_current_ts = runtest_find_test(runtest_test_name);
            if (runtest_current_ts != NULL) {
                runtest_current_ts->ts_test();
            }
        }
    }
    runtest_test_complete();
}

int
runtest_run(const char *test_name, const char *token)
{
    int rc;

    /* Only allow one test at a time. */
    runtest_lock();

    if (runtest_busy) {
        rc = SYS_EAGAIN;
    } else if (test_name[0] != '\0'             &&
               strcmp(test_name, "all") != 0    &&
               runtest_find_test(test_name) == NULL) {
        rc = SYS_ENOENT;
    } else {
        strncpy(runtest_test_name, test_name, sizeof runtest_test_name);
        strncpy(runtest_token, token, sizeof runtest_token);

        runtest_busy = true;
        os_eventq_put(runtest_evq_get(), &run_test_event);
        rc = 0;
    }

    runtest_unlock();

    return rc;
}

/*
 * Package init routine to register newtmgr "run" commands
 */
void
runtest_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    runtest_next_task_idx = 0;

#if MYNEWT_VAL(RUNTEST_LOG)
    rc = cbmem_init(&runtest_cbmem, runtest_cbmem_buf,
                    MYNEWT_VAL(RUNTEST_LOG_SIZE));
    SYSINIT_PANIC_ASSERT(rc == 0);

    /* Create a cbmem log and map the "test" module to it. */
    rc = log_register("testlog", &runtest_log, &log_cbmem_handler,
                      &runtest_cbmem, LOG_SYSLEVEL);
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = modlog_register(LOG_MODULE_TEST, &runtest_log, LOG_LEVEL_DEBUG, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(RUNTEST_CLI)
    shell_cmd_register(&runtest_cmd_struct);
#endif

#if MYNEWT_VAL(RUNTEST_NEWTMGR)
    rc = runtest_nmgr_register_group();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

    rc = os_mutex_init(&runtest_mtx);
    SYSINIT_PANIC_ASSERT(rc == 0);

    /* Use the main event queue by default. */
    runtest_evq_set(os_eventq_dflt_get());
}
