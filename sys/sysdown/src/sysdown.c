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
#include "os/mynewt.h"

/**
 * The NEWT_FEATURE_SYSDOWN setting is injected by new-enough versions of the
 * newt tool.
 */
#if !MYNEWT_VAL(NEWT_FEATURE_SYSDOWN)

int
sysdown(int reason)
{
    os_system_reset();
    return 0;
}

#else

#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <limits.h>
#include "hal/hal_system.h"
#include "hal/hal_watchdog.h"

#define SYSDOWN_TIMEOUT_TICKS   \
    (MYNEWT_VAL(SYSDOWN_TIMEOUT_MS) * OS_TICKS_PER_SEC / 1000)

static_assert(SYSDOWN_TIMEOUT_TICKS >= 0 && SYSDOWN_TIMEOUT_TICKS < INT32_MAX,
              "SYSDOWN_TIMEOUT_MS value not in valid range");

static volatile int sysdown_num_in_progress;
bool sysdown_active;
static struct os_callout sysdown_timer;

static void
sysdown_dflt_panic_cb(const char *file, int line, const char *func,
                      const char *expr, const char *msg)
{
#if MYNEWT_VAL(SYSDOWN_PANIC_MESSAGE)
    if (msg != NULL) {
        fprintf(stderr, "sysdown failure: %s\n", msg);
    }
#endif

    __assert_func(file, line, func, expr);
}

sysdown_panic_fn *sysdown_panic_cb = sysdown_dflt_panic_cb;

/**
 * Sets the sysdown panic function; i.e., the function which executes when
 * shutdown fails.  By default, a panic triggers a failed assertion.
 */
void
sysdown_panic_set(sysdown_panic_fn *panic_cb)
{
    sysdown_panic_cb = panic_cb;
}

static void
sysdown_complete(void)
{
    os_callout_stop(&sysdown_timer);
    os_system_reset();
}

void
sysdown_release(void)
{
    os_sr_t sr;
    int count;

    OS_ENTER_CRITICAL(sr);
    count = --sysdown_num_in_progress;
    OS_EXIT_CRITICAL(sr);

    if (count <= 0) {
        sysdown_complete();
    }
}

static void
sysdown_timer_exp(struct os_event *unused)
{
    assert(0);
}

int
sysdown(int reason)
{
    os_sr_t sr;
    int rc;
    int i;

    /* Only allow one shutdown operation. */
    OS_ENTER_CRITICAL(sr);
    if (sysdown_active) {
        rc = SYS_EALREADY;
    } else {
        sysdown_active = true;
        rc = 0;
    }
    OS_EXIT_CRITICAL(sr);

    if (rc != 0) {
        return rc;
    }

    os_callout_init(&sysdown_timer, os_eventq_dflt_get(), sysdown_timer_exp,
                    NULL);
    rc = os_callout_reset(&sysdown_timer, SYSDOWN_TIMEOUT_TICKS);
    assert(rc == 0);

    /* Call each configured sysdown callback. */
    for (i = 0; sysdown_cbs[i] != NULL; i++) {
        rc = sysdown_cbs[i](reason);
        switch (rc) {
        case SYSDOWN_COMPLETE:
            break;

        case SYSDOWN_IN_PROGRESS:
            OS_ENTER_CRITICAL(sr);
            sysdown_num_in_progress++;
            OS_EXIT_CRITICAL(sr);
            break;

        default:
            break;
        }
    }

    /* If all subprocedures are complete, signal completion of sysdown.
     * Otherwise, wait for in-progress subprocedures to signal completion
     * asynchronously.
     */
    if (sysdown_num_in_progress == 0) {
        sysdown_complete();
    }

    return 0;
}

#endif
