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

#ifndef H_SYSINIT_
#define H_SYSINIT_

#include "syscfg/syscfg.h"
#include "bootutil/bootutil.h"

#if __APPLE__

/* mach-o uses segment,section pairs. */
#define SYSINIT_SEGMENT     "__TEXT"
#define SYSINIT_SECTION     "sysinit"
#define SYSINIT_SECT_STR    SYSINIT_SEGMENT "," SYSINIT_SECTION

#else

#define SYSINIT_SECT_STR    "sysinit"

#endif

#define SECT_SYSINIT        __attribute__((section(SYSINIT_SECT_STR)))

#if MYNEWT_VAL(SPLIT_APPLICATION)
#include "split/split.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t sysinit_active;
void sysinit_init_pkgs(void);

struct sysinit_init_ctxt;

/* Package initialization function. */
typedef void sysinit_init_fn(struct sysinit_init_ctxt *ctxt);

struct sysinit_entry {
    /* Initializes a package. */
    sysinit_init_fn *init_fn;

    /* Specifies when the init function gets called.  0=first, 1=next, etc. */
    uint8_t stage;
};

struct sysinit_init_ctxt {
    /* Corresponds to the init function currently executing. */
    const struct sysinit_entry *entry;

    /* The stage that sysinit is currently processing. */
    uint8_t cur_stage;
};

#define SYSINIT_ENTRY_NAME(fn_name) fn_name ## _sysinit_entry

/**
 * Registers a package initialization function
 *
 * @param init_cb               Pointer to the init function to register.
 * @param init_stage            Indicates when this init function gets called,
 *                                  relative to other init functions.  0=first,
 *                                  1=next, etc.
 */
#define SYSINIT_REGISTER_INIT(init_cb, init_stage)                          \
    const SECT_SYSINIT struct sysinit_entry SYSINIT_ENTRY_NAME(init_cb) = { \
        .init_fn = (init_cb),                                               \
        .stage = (init_stage),                                              \
    }

#define SYSINIT_CONCAT_3(a,b,c)  a##_##b##_##c
#define SYSINIT_CONCAT_2(a,b) SYSINIT_CONCAT_3(sysinit_keep, a, b)
#define SYSINIT_UNIQUE_ID(a) SYSINIT_CONCAT_2(a,__COUNTER__)

/**
 * This macro addresses some linker woes involved in sim builds.  Use this
 * macro to force the specified sysinit entry to be included in the build.
 * This macro is the solution when a package does not get initialized in a sim
 * build.
 *
 * This macro is not necessary for non-sim builds, because the linker script
 * ensures that the sysinit section gets included.
 */
#define SYSINIT_KEEP(init_cb)                       \
    extern const SECT_SYSINIT struct sysinit_entry  \
        SYSINIT_ENTRY_NAME(init_cb);                \
    const void *SYSINIT_UNIQUE_ID(init_cb) = &SYSINIT_ENTRY_NAME(init_cb)

typedef void sysinit_panic_fn(const char *file, int line);

/* By default, a panic triggers an assertion failure.  If the project overrides
 * the sysinit panic function setting, the specified function gets called
 * instead.
 */
#ifndef MYNEWT_VAL_SYSINIT_PANIC_FN
#include <assert.h>
#define SYSINIT_PANIC() assert(0)
#else
void MYNEWT_VAL(SYSINIT_PANIC_FN)(const char *file, int line);
#define SYSINIT_PANIC() MYNEWT_VAL(SYSINIT_PANIC_FN)(__FILE__, __LINE__)
#endif

#define SYSINIT_PANIC_ASSERT(rc) do \
{                                   \
    if (!(rc)) {                    \
        SYSINIT_PANIC();            \
    }                               \
} while (0)

/**
 * Asserts that system initialization is in progress.  This macro is used to
 * ensure packages don't get initialized a second time after system
 * initialization has completed.
 */
#if MYNEWT_VAL(SYSINIT_CONSTRAIN_INIT)
#define SYSINIT_ASSERT_ACTIVE() assert(sysinit_active)
#else
#define SYSINIT_ASSERT_ACTIVE()
#endif

/**
 * In a split app, this macro records that a split app is running.  imgmgt
 * needs to know whether one is running.  In a loader or unified image, this
 * macro is a no-op.
 *
 * Note: This check has to happen here in a macro, rather than in a package's
 * initialization function.  The reason is: only the app package knows whether
 * it is running as a loader/unified or second-stage-app; other packages don't
 * have this information because they may be used both both stages.
 */
#if MYNEWT_VAL(SPLIT_APPLICATION)
#define SYSINIT_SPLIT_INIT() split_app_active_set(1)
#else
#define SYSINIT_SPLIT_INIT()
#endif

/**
 * Initializes all packages in the system.
 */
#define sysinit() do                                                        \
{                                                                           \
    SYSINIT_SPLIT_INIT();                                                   \
    sysinit_init_pkgs();                                                    \
} while (0)

#ifdef __cplusplus
}
#endif

#endif
