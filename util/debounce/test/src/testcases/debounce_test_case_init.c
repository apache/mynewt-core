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

TEST_CASE_SELF(debounce_test_case_init)
{
    struct debouncer d;
    int rc;

    // Invalid configuration - thresh_low > thresh_high.
    rc = debouncer_init(&d, 20, 10, 100);
    TEST_ASSERT_FATAL(rc == SYS_EINVAL);

    // Invalid configuration - thresh_high > max.
    rc = debouncer_init(&d, 10, 20, 15);
    TEST_ASSERT_FATAL(rc == SYS_EINVAL);

    // Successful init.
    rc = debouncer_init(&d, 10, 20, 100);
    TEST_ASSERT_FATAL(rc == 0);

    // Verify initial state.
    TEST_ASSERT(!debouncer_state(&d));
    TEST_ASSERT(debouncer_val(&d) == 0);
}
