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
#ifndef __RUNTEST_H__
#define __RUNTEST_H__

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RUNTEST_NMGR_OP_TEST    0
#define RUNTEST_NMGR_OP_LIST    1

/* Define the prefix to to add to all test log messages.  If the user's syscfg
 * specifies the `RUNTEST_PREFIX` setting, use that value.  Otherwise, generate
 * the prefix from some legacy cflags (`BUILD_TARGET` and `BUILD_ID`).
 */

#if MYNEWT_VAL(RUNTEST_PREFIX)

#define RUNTEST_PREFIX  MYNEWT_VAL(RUNTEST_PREFIX)

#else

#ifndef BUILD_ID
#define BUILD_ID        "UNKNOWN_ID"
#endif

#ifndef BUILD_TARGET
#define BUILD_TARGET    "UNKNOWN_TARGET"
#endif

#define RUNTEST_PREFIX  BUILD_TARGET " Build " BUILD_ID

#endif

/**
 * Retrieves the event queue used by the runtest package.
 */
struct os_eventq *runtest_evq_get(void);

/**
 * Designates the event queue that runtest should use.
 */
void runtest_evq_set(struct os_eventq *evq);

/**
 * Enqueues a single test for immediate execution.
 *
 * @param test_name             The name of the test to run.
 * @param token                 Optional string to include in test results.
 *
 * @return                      0 on success;
 *                              SYS_EAGAIN if a test is already in progress.
 */
int runtest_run(const char *test_name, const char *token);

/**
 * Retrieves the total number of test failures.
 */
int runtest_total_fails_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __RUNTEST_H__ */
