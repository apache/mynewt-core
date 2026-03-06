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

#include "test_framework_priv.h"
#include "test_framework/test_framework.h"

suite_context suite_ctx;

void
mtest_suite_init(const char *name)
{
    printf("MTEST test=%s \n", MYNEWT_VAL(APP_NAME));
    printf("MTEST bsp=%s \n", MYNEWT_VAL(BSP_NAME));
    printf("MTEST core=%s\n\n", MYNEWT_VAL(REPO_HASH_APACHE_MYNEWT_CORE));
    suite_ctx.name = name;
    suite_ctx.tests_failed = 0;
    suite_ctx.tests_passed = 0;
    suite_ctx.tests_run = 0;
}

void
mtest_suite_complete()
{
    if (suite_ctx.suite_aborted) {
        printf("MTEST suite=%s, status=fail\n", suite_ctx.name);
    } else {
        const char *result = suite_ctx.tests_failed ? "fail" : "pass";
        printf("MTEST suite=%s, status=%s, pass=%d/%d, fail=%d/%d\n",
               suite_ctx.name, result, suite_ctx.tests_passed,
               suite_ctx.tests_run, suite_ctx.tests_failed, suite_ctx.tests_run);
    }
    printf("MTEST finished test=%s\n", MYNEWT_VAL(APP_NAME));
}

void
mtest_suite_abort()
{
    suite_ctx.suite_aborted = 1;
}

uint8_t
mtest_suite_is_aborted()
{
    return suite_ctx.suite_aborted;
}
