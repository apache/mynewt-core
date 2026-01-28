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

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H
#include "console/console.h"

#ifdef __cplusplus
extern "C" {
#endif

void mtest_case_init(const char *name);
void mtest_case_fail();
void mtest_case_complete();

void mtest_suite_init(const char *name);
void mtest_suite_complete();
void mtest_suite_abort();
uint8_t mtest_suite_is_aborted();

#define MTEST_ASSERT_FAIL_HANDLER_init()                                      \
    do {                                                                      \
        mtest_suite_abort();                                                  \
        return;                                                               \
    } while (0)

#define MTEST_ASSERT_FAIL_HANDLER_cleanup()                                   \
    do {                                                                      \
        /* Do nothing */                                                      \
    } while (0)

#define MTEST_ASSERT_FAIL_HANDLER_case()                                      \
    do {                                                                      \
        mtest_case_fail();                                                    \
        return;                                                               \
    } while (0)

#define MTEST_ASSERT_IMPL(phase, cond, msg, ...)                              \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("MTEST [FAIL] \"" #cond "\" " msg "\n", ##__VA_ARGS__);    \
            MTEST_ASSERT_FAIL_HANDLER_##phase();                              \
        }                                                                     \
    } while (0)

#define MTEST_INIT_ASSERT(cond, msg, ...)                                     \
    MTEST_ASSERT_IMPL(init, cond, msg, ##__VA_ARGS__)

#define MTEST_CLEANUP_ASSERT(cond, msg, ...)                                  \
    MTEST_ASSERT_IMPL(cleanup, cond, msg, ##__VA_ARGS__)

#define MTEST_CASE_ASSERT(cond, msg, ...)                                     \
    MTEST_ASSERT_IMPL(case, cond, msg, ##__VA_ARGS__)

#define MTEST_INIT(suite_name) void MTEST_INIT_##suite_name(void)

#define MTEST_CLEANUP(suite_name) void MTEST_CLEANUP_##suite_name(void)

#define MTEST_RUN_INIT(suite_name)                                            \
    do {                                                                      \
        printf("MTEST start=init\n");                                         \
        MTEST_INIT_##suite_name();                                            \
        printf("MTEST end=init\n\n");                                         \
        if (mtest_suite_is_aborted()) {                                       \
            return;                                                           \
        }                                                                     \
    } while (0)

#define MTEST_RUN_CLEANUP(suite_name)                                         \
    do {                                                                      \
        printf("\nMTEST start=cleanup\n");                                    \
        MTEST_CLEANUP_##suite_name();                                         \
        printf("MTEST end=cleanup\n\n");                                      \
    } while (0)

#define MTEST_CASE_DECL(case_name) void case_name()

#define MTEST_CASE_DEF(case_name, body)                                       \
    void case_name()                                                          \
    {                                                                         \
        mtest_case_init(#case_name);                                          \
        body();                                                               \
        mtest_case_complete();                                                \
    }

#define MTEST_CASE(case_name)                                                 \
    void MTEST_CASE_##case_name();                                            \
    MTEST_CASE_DEF(case_name, MTEST_CASE_##case_name)                         \
    void MTEST_CASE_##case_name()

#define MTEST_SUITE_DECL(suite_name) void suite_name()

#define MTEST_SUITE(suite_name)                                               \
    void MTEST_SUITE_##suite_name(void);                                      \
    void suite_name()                                                         \
    {                                                                         \
        mtest_suite_init(#suite_name);                                        \
        MTEST_SUITE_##suite_name();                                           \
        mtest_suite_complete();                                               \
    }                                                                         \
    void MTEST_SUITE_##suite_name()

#ifdef __cplusplus
}
#endif

#endif
