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

#include "mtest/mtest.h"
#include "hal/hal_system.h"
#include "hal/hal_watchdog.h"
#include "mtest_watchdog/mtest_watchdog.h"
#include "os/os_cputime.h"

extern volatile int reset_count;

MTEST_CASE(watchdog_test_case_1)
{
    uint32_t start;
    uint32_t last_tickle;
    uint32_t now;

    enum hal_reset_reason reason = hal_reset_cause();
    MTEST_CASE_ASSERT(reason != HAL_RESET_WATCHDOG, "WDOG tickle failed");

    start = os_cputime_ticks_to_usecs(os_cputime_get32());
    last_tickle = start;
    while (1) {
        now = os_cputime_ticks_to_usecs(os_cputime_get32());
        if (now - last_tickle >= MYNEWT_VAL(WATCHDOG_INTERVAL) * 1000 / 4) {
            hal_watchdog_tickle();
            last_tickle = now;
        }

        if (now - start >= 2 * (MYNEWT_VAL(WATCHDOG_INTERVAL) * 1000)) {
            break;
        }
    }
}

MTEST_CASE(watchdog_test_case_2)
{
    uint32_t start;
    uint32_t now;
    enum hal_reset_reason reason;

    if (reset_count == 1) {
        reason = hal_reset_cause();
        MTEST_CASE_ASSERT(reason == HAL_RESET_WATCHDOG, "Expected WDOG reset");
        return;
    }

    reset_count++;
    /* Readback required by STM32H723ZG to ensure data reaches DTCM */
    (void)reset_count;

    printf("Starving WDOG...\n");
    start = os_cputime_ticks_to_usecs(os_cputime_get32());
    while (1) {
        now = os_cputime_ticks_to_usecs(os_cputime_get32());

        if (now - start > 2 * (MYNEWT_VAL(WATCHDOG_INTERVAL) * 1000)) {
            MTEST_CASE_ASSERT(0, "WDOG failed to fire");
        }
    }
}
