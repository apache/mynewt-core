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

#include <inttypes.h>
#include <setjmp.h>

#include "syscfg/syscfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * General execution flow of test suites and cases (more to come XXX)
 *
 * TEST_SUITE
 *      tu_suite_init
 *          tu_suite_pre_test
 *              tu_case_init
 *              tu_case_pre_test
 *                  TEST_CASE
 *              tu_case_post_test
 *              tu_case_pass/tu_case_fail
 *              tu_case_complete
 *          tu_suite_post_test
 *      tu_suite_complete
 */

typedef void tu_case_report_fn_t(char *msg, int msg_len, void *arg);
typedef void tu_suite_restart_fn_t(void *arg);

typedef void tu_pre_test_fn_t(void *arg);
typedef void tu_post_test_fn_t(void *arg);

typedef void tu_init_test_fn_t(void *arg);
typedef void tu_pre_test_fn_t(void *arg);
typedef void tu_post_test_fn_t(void *arg);

/*
 * Private declarations - Test Suite configuration
 */
void tu_suite_set_init_cb(tu_init_test_fn_t *cb, void *cb_arg);
void tu_suite_set_pre_test_cb(tu_pre_test_fn_t *cb, void *cb_arg);
void tu_suite_set_post_test_cb(tu_post_test_fn_t *cb, void *cb_arg);
void tu_suite_set_pass_cb(tu_case_report_fn_t *cb, void *cb_arg);
void tu_suite_set_fail_cb(tu_case_report_fn_t *cb, void *cb_arg);

void tu_suite_init(const char *name);
void tu_suite_pre_test(void);
void tu_suite_post_test(void);
void tu_suite_complete(void);

struct ts_config {
    int ts_print_results;
    int ts_system_assert;

    /*
     * Called prior to the first test in the suite
     */
    tu_init_test_fn_t *ts_suite_init_cb;
    void *ts_suite_init_arg;

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
     * Called after test fails (primarily thoough a failed test assert
     */
    tu_case_report_fn_t *ts_case_fail_cb;
    void *ts_case_fail_arg;

    /*
     * restart after running the test suite
     */
    tu_suite_restart_fn_t *ts_restart_cb;
    void *ts_restart_arg;
};

int tu_parse_args(int argc, char **argv);
int tu_init(void);
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

void tu_case_complete(void);

extern struct tc_config tc_config;
extern struct tc_config *tc_current_config;
extern struct ts_config ts_config;
extern struct ts_config *ts_current_config;

extern const char *tu_suite_name;
extern const char *tu_case_name;
extern int tu_first_idx;

extern int tu_any_failed;
extern int tu_suite_failed;
extern int tu_case_reported;
extern int tu_case_failed;
extern int tu_case_idx;
extern jmp_buf tu_case_jb;

#define TEST_SUITE(suite_name)                               \
static void                                                  \
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
    static void                                              \
    TEST_SUITE_##suite_name(void)

/*
 * for creating multiple files with test cases
 * all belonging to the same suite
 */
#define TEST_CASE_DECL(case_name)                            \
    int case_name(void);

/*
 * Unit test definition.
 */
#define TEST_CASE(case_name)                                  \
    static void TEST_CASE_##case_name(void);                  \
                                                              \
    int                                                       \
    case_name(void)                                           \
    {                                                         \
        tu_suite_pre_test();                                  \
        if (tu_case_idx >= tu_first_idx) {                    \
            tu_case_init(#case_name);                         \
                                                              \
            tu_case_pre_test();                               \
            if (setjmp(tu_case_jb) == 0) {                    \
                TEST_CASE_##case_name();                      \
                tu_case_post_test();                          \
                tu_case_pass();                               \
            }                                                 \
            tu_case_complete();                               \
        }                                                     \
        tu_suite_post_test();                                 \
                                                              \
        return tu_case_failed;                                \
    }                                                         \
                                                              \
    static void                                               \
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
#define STR(s) #s

#define TEST_ASSERT_FULL(fatal, expr, ...) do                 \
{                                                             \
    if (!(expr)) {                                            \
        tu_case_fail_assert((fatal), __FILE__,                \
                            __LINE__, XSTR(expr),             \
                            __VA_ARGS__);                     \
    }                                                         \
} while (0)

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
