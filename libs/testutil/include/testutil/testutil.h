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

#ifndef H_TESTUTIL_
#define H_TESTUTIL_

#include <inttypes.h>
#include <setjmp.h>

/*****************************************************************************
 * Public declarations                                                       *
 *****************************************************************************/

typedef void tu_case_init_fn_t(void *arg);
typedef void tu_case_report_fn_t(char *msg, int msg_len, void *arg);
typedef void tu_suite_init_fn_t(void *arg);
typedef void tu_restart_fn_t(void *arg);

struct tu_config {
    int tc_print_results;
    int tc_system_assert;

    tu_case_init_fn_t *tc_case_init_cb;
    void *tc_case_init_arg;

    tu_case_report_fn_t *tc_case_fail_cb;
    void *tc_case_fail_arg;

    tu_case_report_fn_t *tc_case_pass_cb;
    void *tc_case_pass_arg;

    tu_suite_init_fn_t *tc_suite_init_cb;
    void *tc_suite_init_arg;

    tu_restart_fn_t *tc_restart_cb;
    void *tc_restart_arg;
};

extern struct tu_config tu_config;
extern const char *tu_suite_name;
extern const char *tu_case_name;
extern int tu_first_idx;

typedef void tu_post_test_fn_t(void *arg);

void tu_suite_set_post_test_cb(tu_post_test_fn_t *cb, void *cb_arg);
int tu_parse_args(int argc, char **argv);
int tu_init(void);
void tu_restart(void);

/*****************************************************************************
 * Private declarations                                                      *
 *****************************************************************************/

void tu_suite_complete(void);
void tu_suite_init(const char *name);

void tu_case_init(const char *name);
void tu_case_complete(void);
void tu_case_fail_assert(int fatal, const char *file, int line,
                         const char *expr, const char *format, ...);
void tu_case_write_pass_auto(void);
void tu_case_pass_manual(const char *file, int line,
                         const char *format, ...);
void tu_case_post_test(void);

extern int tu_any_failed;
extern int tu_suite_failed;
extern int tu_case_reported;
extern int tu_case_failed;
extern int tu_case_idx;
extern jmp_buf tu_case_jb;

#define TEST_SUITE(suite_name)                                                \
    static void TEST_SUITE_##suite_name(void);                                \
                                                                              \
    int                                                                       \
    suite_name(void)                                                          \
    {                                                                         \
        tu_suite_init(#suite_name);                                           \
        TEST_SUITE_##suite_name();                                            \
        tu_suite_complete();                                                  \
                                                                              \
        return tu_suite_failed;                                               \
    }                                                                         \
                                                                              \
    static void                                                               \
    TEST_SUITE_##suite_name(void)

/* for creating multiple files with test cases all belonging to the same
 * suite */
#define TEST_CASE_DECL(case_name)  int case_name(void);

#define TEST_CASE(case_name)                                                  \
    static void TEST_CASE_##case_name(void);                                  \
                                                                              \
    int                                                                       \
    case_name(void)                                                           \
    {                                                                         \
        if (tu_case_idx >= tu_first_idx) {                                    \
            tu_case_init(#case_name);                                         \
                                                                              \
            if (setjmp(tu_case_jb) == 0) {                                    \
                TEST_CASE_##case_name();                                      \
                tu_case_post_test();                                          \
                tu_case_write_pass_auto();                                    \
            }                                                                 \
        }                                                                     \
                                                                              \
        tu_case_complete();                                                   \
                                                                              \
        return tu_case_failed;                                                \
    }                                                                         \
                                                                              \
    static void                                                               \
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

#define TEST_ASSERT_FULL(fatal, expr, ...) do                                 \
{                                                                             \
    if (!(expr)) {                                                            \
        tu_case_fail_assert((fatal), __FILE__, __LINE__, XSTR(expr),          \
                            __VA_ARGS__);                                     \
    }                                                                         \
} while (0)

#define TEST_ASSERT(...)                                                      \
    TEST_ASSERT_FULL(0, FIRST(__VA_ARGS__), REST_OR_0(__VA_ARGS__))

#define TEST_ASSERT_FATAL(...)                                                \
    TEST_ASSERT_FULL(1, FIRST(__VA_ARGS__), REST_OR_0(__VA_ARGS__))

#define TEST_PASS(...)                                                        \
    tu_case_pass_manual(__FILE__, __LINE__, __VA_ARGS__);

#ifdef MYNEWT_UNIT_TEST
#define ASSERT_IF_TEST(expr) assert(expr)
#else
#define ASSERT_IF_TEST(expr)
#endif

#endif
