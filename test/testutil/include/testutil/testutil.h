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

#ifndef H_TESTUTIL_
#define H_TESTUTIL_

#include <assert.h>
#include <inttypes.h>
#include <setjmp.h>

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @addtogroup OSSystem
  * @{
  *   @defgroup OSTestutil Test Utilities
  *   @{
  */

/*
 * General execution flow of test suites and callbacks.
 *
 * TEST_SUITE
 *     TEST_CASE
 *         tu_suite_pre_test_cb 
 *         <test-body>
 *         tu_case_pass/tu_case_fail
 *         tu_case_post_test_cb
 */

typedef void tu_case_report_fn_t(const char *msg, void *arg);
typedef void tu_pre_test_fn_t(void *arg);
typedef void tu_post_test_fn_t(void *arg);
typedef void tu_testsuite_fn_t(void);

void tu_set_pass_cb(tu_case_report_fn_t *cb, void *cb_arg);
void tu_set_fail_cb(tu_case_report_fn_t *cb, void *cb_arg);
void tu_suite_init(const char *name);
void tu_suite_pre_test(void);
void tu_suite_complete(void);
int tu_suite_register(tu_testsuite_fn_t* ts, const char *name);

struct ts_suite {
    SLIST_ENTRY(ts_suite) ts_next;
    const char *ts_name;
    tu_testsuite_fn_t *ts_test;
};

SLIST_HEAD(ts_testsuite_list, ts_suite);
extern struct ts_testsuite_list g_ts_suites;

struct tu_config {
    int ts_system_assert;

    const char *ts_suite_name;

    tu_pre_test_fn_t *pre_test_cb;
    void *pre_test_arg;

    /*
     * Called after the current test case completes.
     */
    tu_post_test_fn_t *post_test_cb;
    void *post_test_arg;

    /*
     * Called after test returns success
     */
    tu_case_report_fn_t *pass_cb;
    void *pass_arg;

    /*
     * Called after test fails (typically through a failed test assert)
     */
    tu_case_report_fn_t *fail_cb;
    void *fail_arg;
};

void tu_restart(void);
void tu_start_os(const char *test_task_name, os_task_func_t test_task_handler);

/*
 * Public declarations - test case configuration
 */

void tu_suite_set_pre_test_cb(tu_pre_test_fn_t *cb, void *cb_arg);
void tu_case_set_post_test_cb(tu_post_test_fn_t *cb, void *cb_arg);

void tu_case_init(const char *name);
void tu_case_complete(void);
void tu_case_pass(void);
void tu_case_fail(void);
void tu_case_fail_assert(int fatal, const char *file, int line,
                         const char *expr, const char *format, ...);
void tu_case_write_pass_auto(void);
void tu_case_pass_manual(const char *file, int line,
                         const char *format, ...);
void tu_case_post_test(void);

extern struct tu_config tu_config;

extern const char *tu_suite_name;
extern const char *tu_case_name;

extern int tu_any_failed;
extern int tu_suite_failed;
extern int tu_case_reported;
extern int tu_case_failed;
extern int tu_case_idx;
extern jmp_buf tu_case_jb;

#define TEST_SUITE_DECL(suite_name) void suite_name(void)

#define TEST_SUITE_REGISTER(suite_name)                     \
    tu_suite_register(suite_name, #suite_name);

#define TEST_SUITE(suite_name)                               \
void                                                         \
TEST_SUITE_##suite_name(void);                               \
                                                             \
    void                                                     \
    suite_name(void)                                         \
    {                                                        \
        tu_suite_init(#suite_name);                          \
        TEST_SUITE_##suite_name();                           \
        tu_suite_complete();                                 \
    }                                                        \
                                                             \
    void                                                     \
    TEST_SUITE_##suite_name(void)

/*
 * For declaring the test cases across multiple files
 * belonging to the same suite
 */
#define TEST_CASE_DECL(case_name)                           \
    int case_name(void);

#define TEST_CASE_DEFN(case_name, do_sysinit, body)         \
    int                                                     \
    case_name(void)                                         \
    {                                                       \
        if (do_sysinit) {                                   \
            sysinit();                                      \
        }                                                   \
        tu_suite_pre_test();                                \
        tu_case_init(#case_name);                           \
                                                            \
        if (setjmp(tu_case_jb) == 0) {                      \
            /* Execute test body. */                        \
            body;                                           \
            tu_case_post_test();                            \
            if (!tu_case_failed) {                          \
                tu_case_pass();                             \
            }                                               \
        }                                                   \
        tu_case_complete();                                 \
                                                            \
        return tu_case_failed;                              \
    }                                                       \

/**
 * @brief Defines a test case suitable for running in an application.
 *
 * The `TEST_CASE()` macro should not be used for self-tests (i.e., tests that
 * are run with `newt test`).  Instead, `TEST_CASE_SELF()` or
 * `TEST_CASE_TASK()` should be preferred; those macros perform system clean up
 * before the test runs.
 */
#define TEST_CASE(case_name)                                    \
    void TEST_CASE_##case_name(void);                           \
    TEST_CASE_DEFN(case_name, false, TEST_CASE_##case_name())   \
                                                                \
    void                                                        \
    TEST_CASE_##case_name(void)

#define TEST_CASE_SELF_EMIT_(case_name)                         \
    void TEST_CASE_##case_name(void);                           \
    TEST_CASE_DEFN(case_name, true, TEST_CASE_##case_name())    \
                                                                \
    void                                                        \
    TEST_CASE_##case_name(void)

#define TEST_CASE_TASK_EMIT_(case_name)                     \
    void TEST_CASE_##case_name(void *arg);                  \
    TEST_CASE_DEFN(case_name, true,                         \
                   tu_start_os(#case_name "_test_task",     \
                               TEST_CASE_##case_name));     \
                                                            \
    void                                                    \
    TEST_CASE_##case_name(void *TU_UNUSED_arg)

#if MYNEWT_VAL(SELFTEST) || defined __DOXYGEN__

/**
 * @brief Defines a test case for self-test mode (i.e., suitable for `newt
 * test`).
 *
 * Test cases defined with `TEST_CASE_SELF()` execute `sysinit()` before the
 * test body.
 */
#define TEST_CASE_SELF(case_name) TEST_CASE_SELF_EMIT_(case_name)

/**
 * @brief Defines a test case that runs inside a temporary task.
 *
 * Most tests don't utilize the OS scheduler; they simply run in main(),
 * outside the context of a task.  However, sometimes the OS is required to
 * fully test a package, e.g., to verify timeouts and other timed events.
 *
 * The `TEST_CASE_TASK()` macro simplifies the implementation of test cases
 * that require the OS.  This macro is identical in usage to
 * `TEST_CASE_SELF()`, except the test case it defines performs some additional
 * preliminary work:
 *
 * 1. Creates the default task.
 * 2. Creates the "test task" (the task where the test itself runs).
 * 3. Starts the OS.
 *
 * The body following the macro invocation is what actually runs in the test
 * task.  The test task has a priority of `OS_MAIN_TASK_PRIO + 1`, so it yields
 * to the main task.  Thus, tests using this macro typically have the following
 * form:
 *
 *     TEST_CASE_TASK(my_test)
 *     {
 *         enqueue_event_to_main_task();
 *         // Event immediately runs to completion.
 *         TEST_ASSERT(expected_event_result);
 *
 *         // ...
 *     }
 *
 * The `TEST_CASE_TASK()` macro is only usable in self-tests (i.e., tests that
 * are run with `newt test`).
 */
#define TEST_CASE_TASK(case_name) TEST_CASE_TASK_EMIT_(case_name)

#else

#define TEST_CASE_SELF(case_name)                               \
    static_assert(0, "Test `"#case_name"` is a self test.  "    \
                     "It can only be run from `newt test`");    \
    /* Emit case definition anyway to prevent syntax errors. */ \
    TEST_CASE_SELF_EMIT_(case_name)

#define TEST_CASE_TASK(case_name)                               \
    static_assert(0, "Test `"#case_name"` is a self test.  "    \
                     "It can only be run from `newt test`");    \
    /* Emit case definition anyway to prevent syntax errors. */ \
    TEST_CASE_TASK_EMIT_(case_name)
    
#endif

#define FIRST_AUX(first, ...) first
#define FIRST(...) FIRST_AUX(__VA_ARGS__, _)

#define NUM(...) ARG10(__VA_ARGS__, N, N, N, N, N, N, N, N, 1, _)
#define ARG10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ...) a10

#define REST_OR_0(...) REST_OR_0_AUX(NUM(__VA_ARGS__), __VA_ARGS__)
#define REST_OR_0_AUX(qty, ...) REST_OR_0_AUX_INNER(qty, __VA_ARGS__)
#define REST_OR_0_AUX_INNER(qty, ...) REST_OR_0_AUX_##qty(__VA_ARGS__)
#define REST_OR_0_AUX_1(first) 0
#define REST_OR_0_AUX_N(first, ...) __VA_ARGS__

#define XSTR(s) STR(s)
#ifndef STR 
#define STR(s) #s
#endif

#if MYNEWT_VAL(TESTUTIL_SYSTEM_ASSERT)

#define TEST_ASSERT_FULL(fatal, expr, ...) (assert(expr))

#else

#define TEST_ASSERT_FULL(fatal, expr, ...) do                 \
{                                                             \
    if (!(expr)) {                                            \
        tu_case_fail_assert((fatal), __FILE__,                \
                            __LINE__, XSTR(expr),             \
                            __VA_ARGS__);                     \
    }                                                         \
} while (0)

#endif

#define TEST_ASSERT(...)                                      \
    TEST_ASSERT_FULL(0, FIRST(__VA_ARGS__), REST_OR_0(__VA_ARGS__))

#define TEST_ASSERT_FATAL(...)                                \
    TEST_ASSERT_FULL(1, FIRST(__VA_ARGS__), REST_OR_0(__VA_ARGS__))

#define TEST_PASS(...)                                        \
    tu_case_pass_manual(__FILE__, __LINE__, __VA_ARGS__);

#if MYNEWT_VAL(TEST)
#define ASSERT_IF_TEST(expr) assert(expr)
#else
#define ASSERT_IF_TEST(expr)
#endif

#ifdef __cplusplus
}
#endif

#endif

/**
 *   @} OSTestutil
 * @} OSSys
 */
