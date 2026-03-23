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

#include "sysinit/sysinit.h"
#include "bsp/bsp.h"
#include "pwm/pwm.h"
#include "hal/hal_gpio.h"
#include "pwm_test/pwm_test.h"

pwm_test_ctx test_ctx;

void
pwm_init()
{
    struct pwm_chan_cfg chan_conf = { .pin = PWM_TEST_CH_CFG_PIN,
                                      .inverted = PWM_TEST_CH_CFG_INV,
                                      .data = NULL };

    test_ctx.pwm =
        (struct pwm_dev *)os_dev_open(PWM_TEST_DEV, OS_TIMEOUT_NEVER, NULL);
    MTEST_INIT_ASSERT(test_ctx.pwm != NULL, "device %s not available\n", PWM_TEST_DEV);
    int rc = pwm_set_frequency(test_ctx.pwm, PWM_FREQ_HZ);
    MTEST_INIT_ASSERT(rc > 0, "set frequency for pwm clock failed");

    rc = pwm_configure_channel(test_ctx.pwm, PWM_TEST_CH_NUM, &chan_conf);
    MTEST_INIT_ASSERT(rc == 0, "channel configuration failed");

    rc = pwm_enable(test_ctx.pwm);
    MTEST_INIT_ASSERT(rc == 0, "PWM enable");
}

void
timer_cb()
{
    if (test_ctx.sample_cnt < WINDOW_SIZE) {
        test_ctx.sample_cnt++;
        if (hal_gpio_read(MYNEWT_VAL(PWM_READ_PIN))) {
            test_ctx.high_cnt++;
        }

        if (test_ctx.sample_cnt == WINDOW_SIZE) {
            os_cputime_timer_stop(&test_ctx.timer);
            os_sem_release(&test_ctx.sem);
        } else {
            os_cputime_timer_relative(&test_ctx.timer, TIMER_TICKS);
        }
    }
}

MTEST_INIT(pwm_test)
{
    int rc = hal_gpio_init_in(MYNEWT_VAL(PWM_READ_PIN), HAL_GPIO_PULL_DOWN);
    MTEST_INIT_ASSERT(rc == 0, "pin configuration failed");

    os_error_t err = os_sem_init(&test_ctx.sem, 0);
    MTEST_INIT_ASSERT(err == OS_OK, "semaphore init failed");

    os_cputime_timer_init(&test_ctx.timer, timer_cb, NULL);

    pwm_init();
}

MTEST_CLEANUP(pwm_test)
{
    int rc = pwm_disable(test_ctx.pwm);
    MTEST_CLEANUP_ASSERT(rc == 0, "disable PWM failed");

    os_cputime_timer_stop(&test_ctx.timer);

    rc = os_dev_close((struct os_dev *)test_ctx.pwm);
    MTEST_CLEANUP_ASSERT(rc == 0, "dev close failed");
}

MTEST_SUITE(pwm_test)
{
    MTEST_RUN_INIT(pwm_test);
    pwm_test_case_1();
    pwm_test_case_2();
    pwm_test_case_3();
    pwm_test_case_4();
    pwm_test_case_5();
    pwm_test_case_6();
    MTEST_RUN_CLEANUP(pwm_test);
}

int
mynewt_main(int argc, char **argv)
{
    sysinit();

    pwm_test();

    while (1) {
        os_time_delay(OS_TICKS_PER_SEC);
    }
}
