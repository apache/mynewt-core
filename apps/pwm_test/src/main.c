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
#include "syscfg/syscfg.h"
#include "sysinit/sysinit.h"
#include <os/os.h>
#include <pwm/pwm.h>
#include <bsp/bsp.h>
#include <easing/easing.h>

struct pwm_dev *pwm;
static uint32_t pwm_freq = 1000;
static uint32_t max_steps = 2000; /* two seconds motion up/down */
static uint16_t top_val;
static volatile uint32_t step = 0;
static volatile bool up = false;

static void
pwm_handler(void* unused)
{
    int16_t eased;
    eased = sine_int_io(step, max_steps, top_val);
    pwm_enable_duty_cycle(pwm, 0, eased);
    if (step >= max_steps || step <= 0) {
        up = !up;
    }

    step += (up) ? 1 : -1;
}

int
pwm_init(void)
{
    struct pwm_chan_cfg chan_conf = {
        .pin = LED_BLINK_PIN,
        .inverted = true,
        .cycle_handler = pwm_handler, /* this won't work on soft_pwm */
        .cycle_int_prio = 3,
        .data = NULL
    };
    uint32_t base_freq;
    int rc;

#if MYNEWT_VAL(SOFT_PWM)
    pwm = (struct pwm_dev *) os_dev_open("spwm", 0, NULL);
#else
    pwm = (struct pwm_dev *) os_dev_open("pwm0", 0, NULL);
#endif

    /* set the PWM frequency */
    pwm_set_frequency(pwm, 1000);
    base_freq = pwm_get_clock_freq(pwm);
    top_val = (uint16_t) (base_freq / pwm_freq);

    /* setup led 1 - 100% duty cycle*/
    rc = pwm_chan_config(pwm, 0, &chan_conf);
    assert(rc == 0);

    rc = pwm_enable_duty_cycle(pwm, 0, top_val);
    assert(rc == 0);

    return rc;
}

int
main(int argc, char **argv)
{
    sysinit();

    pwm_init();

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return(0);
}
