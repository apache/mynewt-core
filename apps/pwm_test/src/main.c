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

struct pwm_dev *pwm;
uint16_t max_val;
static struct os_callout my_callout;

int
pwm_init(void)
{
    struct pwm_chan_cfg chan_conf = {
        .pin = LED_BLINK_PIN,
        .inverted = true,
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
    pwm_set_frequency(pwm, 10000);
    base_freq = pwm_get_clock_freq(pwm);
    max_val = (uint16_t) (base_freq / 10000);

    /* setup led 1 - 100% duty cycle*/
    rc = pwm_chan_config(pwm, 0, &chan_conf);
    assert(rc == 0);

    rc = pwm_enable_duty_cycle(pwm, 0, max_val);
    assert(rc == 0);

#ifdef LED_2
    /* setup led 2 - 50% duty cycle */
    chan_conf.pin = LED_2;
    rc = pwm_chan_config(pwm, 1, &chan_conf);
    assert(rc == 0);

    rc = pwm_enable_duty_cycle(pwm, 1, max_val/2);
    assert(rc == 0);
#endif
#ifdef LED_3
    /* setup led 3 - 25% duty cycle */
    chan_conf.pin = LED_3;
    rc = pwm_chan_config(pwm, 2, &chan_conf);
    assert(rc == 0);

    rc = pwm_enable_duty_cycle(pwm, 2, max_val/4);
    assert(rc == 0);
#endif
#ifdef LED_4
    /* setup led 4 - 10% duty cycle */
    chan_conf.pin = LED_4;
    rc = pwm_chan_config(pwm, 3, &chan_conf);
    assert(rc == 0);

    rc = pwm_enable_duty_cycle(pwm, 3, max_val/10);
    assert(rc == 0);
#endif

    return rc;
}

static void
pwm_toggle(struct os_event *ev)
{
    int rc;

    if(pwm!=0){
        rc = os_dev_close((struct os_dev *)pwm);
        assert(rc == 0);
        pwm = NULL;
    }else{
        rc = pwm_init();
        assert(rc == 0);
    }
    os_callout_reset(&my_callout, OS_TICKS_PER_SEC * 5);
}

int
main(int argc, char **argv)
{
    sysinit();

    os_callout_init(&my_callout, os_eventq_dflt_get(),
                    pwm_toggle, NULL);

    os_callout_reset(&my_callout, OS_TICKS_PER_SEC * 5);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return(0);
}
