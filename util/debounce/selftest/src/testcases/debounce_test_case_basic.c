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

#include "os/mynewt.h"
#include "debounce/debounce.h"
#include "debounce_test.h"

TEST_CASE_SELF(debounce_test_case_basic)
{
    struct debouncer d;
    int rc;
    int i;

    rc = debouncer_init(&d, 10, 20, 100);
    TEST_ASSERT_FATAL(rc == 0);

    // Ensure debouncer not triggered for first 19 increments.
    for (i = 0; i < 19; i++) {
        rc = debouncer_adjust(&d, 1);
        TEST_ASSERT_FATAL(rc == 0);
        TEST_ASSERT(!debouncer_state(&d));
        TEST_ASSERT(debouncer_val(&d) == i + 1);
    }

    // Ensure 20th increment triggers debouncer.
    rc = debouncer_adjust(&d, 1);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT(debouncer_state(&d));
    TEST_ASSERT(debouncer_val(&d) == 20);

    // Ensure debouncer remains triggered during decrease to 11.
    for (i = 0; i < 9; i++) {
        rc = debouncer_adjust(&d, -1);
        TEST_ASSERT_FATAL(rc == 0);
        TEST_ASSERT(debouncer_state(&d));
        TEST_ASSERT(debouncer_val(&d) == 20 - i - 1);
    }

    // Ensure decrement to 10 removes trigger.
    rc = debouncer_adjust(&d, -1);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT(!debouncer_state(&d));
    TEST_ASSERT(debouncer_val(&d) == 10);

    // Increment back to 11 does not trigger debouncer.
    rc = debouncer_adjust(&d, 1);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT(!debouncer_state(&d));
    TEST_ASSERT(debouncer_val(&d) == 11);

    // Ensure debouncer won't increase beyond max.
    debouncer_set(&d, 100);
    TEST_ASSERT(debouncer_state(&d));
    TEST_ASSERT(debouncer_val(&d) == 100);

    rc = debouncer_adjust(&d, 1);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT(debouncer_state(&d));
    TEST_ASSERT(debouncer_val(&d) == 100);
}
