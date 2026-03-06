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

#ifndef PWM_TEST_H
#define PWM_TEST_H
#include "test_framework/test_framework.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PWM_TEST_DEV "pwm0"

#define TIMER_FREQ_HZ  MYNEWT_VAL(OS_CPUTIME_FREQ)
#define SAMPLE_FREQ_HZ 1000
#define PWM_FREQ_HZ    200

#define TIMER_TICKS    (TIMER_FREQ_HZ / SAMPLE_FREQ_HZ)
#define MEASURE_TIME_S 1
#define WINDOW_SIZE    (SAMPLE_FREQ_HZ * MEASURE_TIME_S)

#if MYNEWT_VAL(BSP_nucleo_f767zi) || MYNEWT_VAL(BSP_nucleo_h753zi) ||         \
    MYNEWT_VAL(BSP_nucleo_f411re) || MYNEWT_VAL(BSP_nucleo_g491re) ||         \
    MYNEWT_VAL(BSP_nucleo_h723zg)
#define PWM_TEST_CH_CFG_PIN MCU_AFIO_GPIO(ARDUINO_PIN_D12, 2)
#define PWM_TEST_CH_CFG_INV false
#define PWM_TEST_CH_NUM     0
#else
#define PWM_TEST_CH_CFG_PIN LED_BLINK_PIN
#define PWM_TEST_CH_NUM     0
#define PWM_TEST_CH_CFG_INV false
#endif

typedef struct {
    struct pwm_dev *pwm;
    struct os_sem sem;
    struct hal_timer timer;

    int sample_cnt;
    int high_cnt;
} pwm_test_ctx;

extern pwm_test_ctx test_ctx;

MTEST_CASE_DECL(pwm_test_case_1);
MTEST_CASE_DECL(pwm_test_case_2);
MTEST_CASE_DECL(pwm_test_case_3);
MTEST_CASE_DECL(pwm_test_case_4);
MTEST_CASE_DECL(pwm_test_case_5);
MTEST_CASE_DECL(pwm_test_case_6);

#ifdef __cplusplus
}
#endif

#endif
