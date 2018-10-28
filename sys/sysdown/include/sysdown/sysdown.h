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

#ifndef H_SYSDOWN_
#define H_SYSDOWN_

#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>
#include "syscfg/syscfg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYSDOWN_COMPLETE    0
#define SYSDOWN_IN_PROGRESS 1

/**
 * Whether the system is currently shutting down
 */
extern bool sysdown_active;

typedef int sysdown_fn(int reason);
typedef void sysdown_panic_fn(const char *file, int line, const char *func,
                              const char *expr, const char *msg);
typedef void sysdown_complete_fn(int status, void *arg);

extern sysdown_fn * const sysdown_cbs[];
extern sysdown_panic_fn *sysdown_panic_cb;

void sysdown_panic_set(sysdown_panic_fn *panic_fn);

#if MYNEWT_VAL(SYSDOWN_PANIC_MESSAGE)

#if MYNEWT_VAL(SYSDOWN_PANIC_FILE_LINE)
#define SYSDOWN_PANIC_MSG(msg) sysdown_panic_cb(__FILE__, __LINE__, 0, 0, msg)
#else
#define SYSDOWN_PANIC_MSG(msg) sysdown_panic_cb(0, 0, 0, 0, msg)
#endif

#else

#if MYNEWT_VAL(SYSDOWN_PANIC_FILE_LINE)
#define SYSDOWN_PANIC_MSG(msg) sysdown_panic_cb(__FILE__, __LINE__, 0, 0, 0)
#else
#define SYSDOWN_PANIC_MSG(msg) sysdown_panic_cb(0, 0, 0, 0, 0)
#endif

#endif

#define SYSDOWN_PANIC() SYSDOWN_PANIC_MSG(NULL)

#define SYSDOWN_ASSERT_MSG(rc, msg) do \
{                                            \
    if (!(rc)) {                             \
        SYSDOWN_PANIC_MSG(msg);              \
    }                                        \
} while (0)

#define SYSDOWN_ASSERT(rc) SYSDOWN_ASSERT_MSG(rc, NULL)

/**
 * Asserts that system shutdown is in progress.  This macro is used to ensure
 * packages don't get shut down a second time after system shutdown has
 * completed.
 */
#if MYNEWT_VAL(SYSDOWN_CONSTRAIN_DOWN)
#define SYSDOWN_ASSERT_ACTIVE() assert(sysdown_active)
#else
#define SYSDOWN_ASSERT_ACTIVE()
#endif

/**
 * @brief Performs a controlled shutdown and reset of the system.
 *
 * This function executes each package's shutdown sequence, then triggers a
 * reboot.
 *
 * @param reason                The reason for the shutdown.  One of the
 *                                  HAL_RESET_[...] codes or an
 *                                  implementation-defined value.
 *
 * @return
 */
int sysdown(int reason);

/**
 * @brief Signals completion of an in-progress sysdown subprocedure.
 *
 * If a sysdown subprocedure needs to perform additional work after its
 * callback finishes, it returns SYSDOWN_IN_PROGRESS.  Later, when the
 * subprocedure completes, it signals its completion asynchronously with a call
 * to this function.
 */
void sysdown_release(void);

#ifdef __cplusplus
}
#endif

#endif
