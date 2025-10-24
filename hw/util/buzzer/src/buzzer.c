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

#include "buzzer/buzzer.h"

#include <os/mynewt.h>
#include <pwm/pwm.h>

#define BUZZER_PIN MYNEWT_VAL(BUZZER_PIN)
#define BUZZER_PWM MYNEWT_VAL(BUZZER_PWM)
#define BUZZER_PWM_CHAN 0

#if BUZZER_PIN >= 0
struct pwm_dev *_pwm_dev;
#endif

// TEST

/***
 * Initialize the PWM device to be used as a square wave generator. This is
 * automatically invoked by the Mynewt initialization system.
 */
void
buzzer_driver_init()
{
#if BUZZER_PIN >= 0
    struct pwm_dev_cfg dev_conf = {
        .n_cycles = 0,
        .int_prio = -1,
        .cycle_handler = NULL,
        .seq_end_handler = NULL,
        .cycle_data = NULL,
        .seq_end_data = NULL,
        .data = NULL
    };

    struct pwm_chan_cfg chan_conf = {
        .pin = BUZZER_PIN,
        .inverted = false,
        .data = NULL,
    };

    int rc;

    /* PWM device */
    _pwm_dev = (struct pwm_dev *)os_dev_open(BUZZER_PWM, 0, NULL);
    assert(_pwm_dev != NULL);

    /* PWM configuration */
    rc = pwm_configure_device(_pwm_dev, &dev_conf);
    assert(rc == 0);

    /* set channel */
    rc = pwm_configure_channel(_pwm_dev, BUZZER_PWM_CHAN, &chan_conf);
    assert(rc == 0);

    /* set tone off */
    pwm_set_duty_cycle(_pwm_dev, BUZZER_PWM_CHAN, 0);
    assert(rc == 0);

    /* enable PMW device */
    rc = pwm_enable(_pwm_dev);
    assert(rc == 0);
#endif
}

/***
 * Activate the PWM to generate a square wave of a specified frequency.
 *
 * @param freq Frequency of the tone in Hz.
 */
void
buzzer_tone_on(uint32_t freq)
{
#if BUZZER_PIN >= 0
    if (freq == 0) {
        /* stop PWM */
        pwm_set_duty_cycle(_pwm_dev, BUZZER_PWM_CHAN, 0);
    } else {
        /* set frequency */
        pwm_set_frequency(_pwm_dev, freq);

        /* set duty at 50% */
        pwm_set_duty_cycle(_pwm_dev, BUZZER_PWM_CHAN, pwm_get_top_value(_pwm_dev) / 2);
    }
#endif
}

/***
 * Stop the generation of the square wave triggered by buzzer_tone_on().
 */
inline void
buzzer_tone_off()
{
#if BUZZER_PIN >= 0
    buzzer_tone_on(0);
#endif
}
