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

#include "os/queue.h"

#include "syscfg/syscfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * General execution flow of test suites and callbacks (more to come XXX)
 *
 * TEST_SUITE
 *      tu_suite_init -> ts_suite_init_cb
 *          tu_suite_pre_test -> ts_case_pre_test_cb
 *              tu_case_init -> tc_case_init_cb
 *              tu_case_pre_test -> tc_case_pre_test_cb
 *                  TEST_CASE
 *              tu_case_post_test -> tc_case_post_test_cb
 *              tu_case_pass/tu_case_fail -> ts_case_{pass,fail}_cb
 *              tu_case_complete
 *          tu_suite_post_test -> ts_case_post_test_cb
 *      tu_suite_complete -> ts_suite_complete_cb
 */

typedef void tu_case_report_fn_t(char *msg, void *arg);
typedef void tu_suite_restart_fn_t(void *arg);

typedef void tu_init_test_fn_t(void *arg);
typedef void tu_pre_test_fn_t(void *arg);
typedef void tu_post_test_fn_t(void *arg);

typedef void tu_testsuite_fn_t(void);

/*
 * Private declarations - Test Suite configuration
 */
void tu_suite_set_init_cb(tu_init_test_fn_t *cb, void *cb_arg);
void tu_suite_set_complete_cb(tu_init_test_fn_t *cb, void *cb_arg);
void tu_suite_set_pre_test_cb(tu_pre_test_fn_t *cb, void *cb_arg);
void tu_suite_set_post_test_cb(tu_post_test_fn_t *cb, void *cb_arg);
void tu_suite_set_pass_cb(tu_case_report_fn_t *cb, void *cb_arg);
void tu_suite_set_fail_cb(tu_case_report_fn_t *cb, void *cb_arg);

void tu_suite_init(const char *name);
void tu_suite_pre_test(void);
void tu_suite_post_test(void);
void tu_suite_complete(void);

int tu_suite_register(tu_testsuite_fn_t* ts, const char *name);

struct ts_suite {
    SLIST_ENTRY(ts_suite) ts_next;
    const char *ts_name;
    tu_testsuite_fn_t *ts_test;
};

SLIST_HEAD(ts_testsuite_list, ts_suite);
extern struct ts_testsuite_list g_ts_suites;

struct ts_config {
    int ts_print_results;
    int ts_system_assert;

    const char *ts_suite_name;

    /*
     * Called prior to the first test in the suite
     */
    tu_init_test_fn_t *ts_suite_init_cb;
    void *ts_suite_init_arg;

    /*
     * Called after the last test in the suite
     */
    tu_init_test_fn_t *ts_suite_complete_cb;
    void *ts_suite_complete_arg;

    /*
     * Called before every test in the suite
     */
    tu_pre_test_fn_t *ts_case_pre_test_cb;
    void *ts_case_pre_arg;

    /*
     * Called after every test in the suite
     */
    tu_post_test_fn_t *ts_case_post_test_cb;
    void *ts_case_post_arg;

    /*
     * Called after test returns success
     */
    tu_case_report_fn_t *ts_case_pass_cb;
    void *ts_case_pass_arg;

    /*
     * Called after test fails (typically thoough a failed test assert)
     */
    tu_case_report_fn_t *ts_case_fail_cb;
    void *ts_case_fail_arg;

    /*
     * restart after running the test suite - self-test only 
     */
    tu_suite_restart_fn_t *ts_restart_cb;
    void *ts_restart_arg;
};

void tu_restart(void);

/*
 * Public declarations - test case configuration
 */

void tu_case_set_init_cb(tu_init_test_fn_t *cb, void *cb_arg);
void tu_case_set_pre_cb(tu_pre_test_fn_t *cb, void *cb_arg);
void tu_case_set_post_cb(tu_post_test_fn_t *cb, void *cb_arg);

struct tc_config {
    /*
     * Called to initialize the test case
     */
    tu_init_test_fn_t *tc_case_init_cb;
    void *tc_case_init_arg;

    /*
     * Called prior to the test case start
     */
    tu_pre_test_fn_t *tc_case_pre_test_cb;
    void *tc_case_pre_arg;

    /*
     * Called after the test case completes
     */
    tu_post_test_fn_t *tc_case_post_test_cb;
    void *tc_case_post_arg;
};

void tu_case_init(const char *name);
void tu_case_complete(void);
void tu_case_pass(void);
void tu_case_fail(void);
void tu_case_fail_assert(int fatal, const char *file, int line,
                         const char *expr, const char *format, ...);
void tu_case_write_pass_auto(void);
void tu_case_pass_manual(const char *file, int line,
                         const char *format, ...);
void tu_case_pre_test(void);
void tu_case_post_test(void);

extern struct tc_config tc_config;
extern struct tc_config *tc_current_config;
extern struct ts_config ts_config;
extern struct ts_config *ts_current_config;

extern const char *tu_suite_name;
extern const char *tu_case_name;

extern int tu_any_failed;
extern int tu_suite_failed;
extern int tu_case_reported;
extern int tu_case_failed;
extern int tu_case_idx;
extern jmp_buf tu_case_jb;

#define TEST_SUITE_DECL(suite_name) extern void suite_name()

#define TEST_SUITE_REGISTER(suite_name)                      \
  tu_suite_register((tu_testsuite_fn_t*)suite_name, ((const char *)#suite_name));

#define TEST_SUITE(suite_name)                               \
void                                                         \
TEST_SUITE_##suite_name(void);                               \
                                                             \
    int                                                      \
    suite_name(void)                                         \
    {                                                        \
        tu_suite_init(#suite_name);                          \
        TEST_SUITE_##suite_name();                           \
        tu_suite_complete();                                 \
                                                             \
        return tu_suite_failed;                              \
    }                                                        \
                                                             \
    void                                                     \
    TEST_SUITE_##suite_name(void)

/*
 * For declaring the test cases across multiple files
 * belonging to the same suite
 */
#define TEST_CASE_DECL(case_name)                            \
    int case_name(void);

/*
 * Unit test definition.
 */
#define TEST_CASE(case_name)                                  \
    void TEST_CASE_##case_name(void);                         \
                                                              \
    int                                                       \
    case_name(void)                                           \
    {                                                         \
        tu_suite_pre_test();                                  \
        tu_case_init(#case_name);                             \
                                                              \
        tu_case_pre_test();                                   \
        if (setjmp(tu_case_jb) == 0) {                        \
            TEST_CASE_##case_name();                          \
            tu_case_post_test();                              \
            if (!tu_case_failed) {                            \
                tu_case_pass();                               \
            }                                                 \
        }                                                     \
        tu_case_complete();                                   \
        tu_suite_post_test();                                 \
                                                              \
        return tu_case_failed;                                \
    }                                                         \
                                                              \
    void                                                      \
    TEST_CASE_##case_name(void)

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
