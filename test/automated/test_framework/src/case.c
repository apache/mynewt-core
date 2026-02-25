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

static case_context case_ctx;
extern suite_context suite_ctx;

void
mtest_case_init(const char *name)
{
    case_ctx.failed = 0;
    case_ctx.name = name;
    printf("MTEST start=%s\n", case_ctx.name);
}

void
mtest_case_fail()
{
    case_ctx.failed = 1;
}

void
mtest_case_complete()
{
    printf("MTEST end=%s, status=%s\n", case_ctx.name,
           case_ctx.failed != 1 ? "pass" : "fail");

    suite_ctx.tests_run++;

    if (case_ctx.failed) {
        suite_ctx.tests_failed++;
    } else {
        suite_ctx.tests_passed++;
    }
}
