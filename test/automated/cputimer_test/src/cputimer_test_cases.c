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

#include "bsp.h"
#include "stdlib.h"
#include "console/console.h"
#include "cputimer_test/cputimer_test.h"

void
ms_tick_cb(void *arg)
{
    test_ctx.ms_counter++;
    os_cputime_timer_relative(&test_ctx.ms_timer, 1000);
}

void
check_cb(struct os_event *ev)
{
    test_ctx.act_ms = test_ctx.ms_counter;
    test_ctx.exp_ms = test_ctx.dur_s * 1000;

    int delta = abs(test_ctx.act_ms - test_ctx.exp_ms);
    test_ctx.err_scaled = delta * 10000 / test_ctx.exp_ms;

    test_ctx.err_int = test_ctx.err_scaled / 100;
    test_ctx.err_frac = test_ctx.err_scaled % 100;
}

void
test_cputimer_accuracy(int duration_s)
{
    struct cputimer_test_ctx *tc = &test_ctx;

    tc->dur_s = duration_s;
    tc->ms_counter = 0;

    os_cputime_timer_init(&tc->ms_timer, ms_tick_cb, NULL);
    os_callout_init(&tc->check_callout, os_eventq_dflt_get(), check_cb, NULL);

    os_cputime_timer_relative(&tc->ms_timer, 1000);
    os_callout_reset(&tc->check_callout, OS_TICKS_PER_SEC * duration_s);

    os_eventq_run(os_eventq_dflt_get());

    int thrld = MYNEWT_VAL(TOLERANCE) * 100;

    printf("expected=%d, actual=%d, error=%d.%02d%%\n", tc->exp_ms, tc->act_ms,
           tc->err_int, tc->err_frac);

    MTEST_CASE_ASSERT(
        tc->err_scaled < thrld,
        "timing out of tolerance: exp=%d, act=%d, err=%d.%02d%%, (tol=%d%%)",
        tc->exp_ms, tc->act_ms, tc->err_int, tc->err_frac, MYNEWT_VAL(TOLERANCE));

    os_cputime_timer_stop(&tc->ms_timer);
    os_callout_stop(&tc->check_callout);
}

MTEST_CASE(cputimer_test_case_1)
{
    test_cputimer_accuracy(1);
}

MTEST_CASE(cputimer_test_case_2)
{
    test_cputimer_accuracy(2);
}

MTEST_CASE(cputimer_test_case_3)
{
    test_cputimer_accuracy(4);
}

MTEST_CASE(cputimer_test_case_4)
{
    test_cputimer_accuracy(8);
}

MTEST_CASE(cputimer_test_case_5)
{
    test_cputimer_accuracy(16);
}
