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

#include "pwm_test/pwm_test.h"
#include "pwm/pwm.h"

void
test_pwm_duty_cycle(int duty_percent)
{
    test_ctx.sample_cnt = 0;
    test_ctx.high_cnt = 0;

    int top = pwm_get_top_value(test_ctx.pwm);
    MTEST_CASE_ASSERT(top > 0, "PWM get top value failed");

    int duty_val = top * duty_percent / 100;
    int rc = pwm_set_duty_cycle(test_ctx.pwm, PWM_TEST_CH_NUM, duty_val);
    MTEST_CASE_ASSERT(rc == 0, "set duty cycle %u%% (value=%d) failed",
                      duty_percent, duty_val);

    rc = os_cputime_timer_relative(&test_ctx.timer, TIMER_TICKS);
    MTEST_CASE_ASSERT(rc == 0, "timer start failed");

    rc = os_sem_pend(&test_ctx.sem, OS_TICKS_PER_SEC * 10);
    MTEST_CASE_ASSERT(rc == 0, "measurement timeout for duty %d%%", duty_percent);

    MTEST_CASE_ASSERT(test_ctx.sample_cnt > 0, "no samples collected");

    int measured_duty = test_ctx.high_cnt * 100 / test_ctx.sample_cnt;
    int diff = measured_duty - duty_percent;
    if (diff < 0) {
        diff = -diff;
    }

    MTEST_CASE_ASSERT(
        diff < MYNEWT_VAL(PWM_TOLERANCE),
        "duty tolerance exceeded: expected %d%%, measured %d%%, diff %d%% (max %d%%)",
        duty_percent, measured_duty, diff, MYNEWT_VAL(PWM_TOLERANCE));
}

MTEST_CASE(pwm_test_case_1)
{
    test_pwm_duty_cycle(0);
}

MTEST_CASE(pwm_test_case_2)
{
    test_pwm_duty_cycle(20);
}

MTEST_CASE(pwm_test_case_3)
{
    test_pwm_duty_cycle(40);
}

MTEST_CASE(pwm_test_case_4)
{
    test_pwm_duty_cycle(60);
}

MTEST_CASE(pwm_test_case_5)
{
    test_pwm_duty_cycle(80);
}

MTEST_CASE(pwm_test_case_6)
{
    test_pwm_duty_cycle(100);
}
