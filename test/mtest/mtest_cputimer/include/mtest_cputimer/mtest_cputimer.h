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

#ifndef MTEST_CPUTIMER_H
#define MTEST_CPUTIMER_H
#include "mtest/mtest.h"

#ifdef __cplusplus
extern "C" {
#endif

MTEST_CASE_DECL(cputimer_test_case_1);
MTEST_CASE_DECL(cputimer_test_case_2);
MTEST_CASE_DECL(cputimer_test_case_3);
MTEST_CASE_DECL(cputimer_test_case_4);
MTEST_CASE_DECL(cputimer_test_case_5);

void ms_tick_cb(void *arg);
void check_cb(struct os_event *ev);

struct cputimer_test_ctx {
    struct os_callout check_callout;
    struct hal_timer ms_timer;

    int ms_counter;
    int act_ms;
    int exp_ms;
    int dur_s;

    int err_scaled;
    int err_int;
    int err_frac;
};

extern struct cputimer_test_ctx test_ctx;

#ifdef __cplusplus
}
#endif
#endif
