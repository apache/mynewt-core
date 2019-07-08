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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include "os/mynewt.h"
#include "syscfg/syscfg.h"
#include "testutil/testutil.h"
#include "da1469x_test/da1469x_test.h"

TEST_SUITE(da1469x_test_suite)
{
#if MYNEWT_VAL(TESTBENCH_DA1469X_SNC == 1)
    da1469x_snc_test_case_1();
    da1469x_snc_test_case_2();
    da1469x_snc_test_case_3();
#endif
}

void
da1469x_hosttest_init(void)
{
    TEST_SUITE_REGISTER(da1469x_test_suite);
}
