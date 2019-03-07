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

#include "os/mynewt.h"
#include <pwm/pwm.h>
#include <bsp/bsp.h>
#include <easing/easing.h>
#include <console/console.h>
#include "pwm_shell.h"

#if (defined NUCLEO_F767ZI)
#   define  PWM_TEST_CH_CFG_PIN  MCU_AFIO_GPIO(LED_BLINK_PIN, 2)
#   define  PWM_TEST_CH_CFG_INV  false 
#   define  PWM_TEST_CH_NUM      2
#   define  PWM_TEST_IRQ_PRIO    0
#elif (defined NUCLEO_F303RE)
#   define  PWM_TEST_CH_CFG_PIN  MCU_AFIO_GPIO(LED_BLINK_PIN, 1)
#   define  PWM_TEST_CH_CFG_INV  false 
#   define  PWM_TEST_CH_NUM      0
#   define  PWM_TEST_IRQ_PRIO    0
#elif (defined NUCLEO_F303K8)
#   define  PWM_TEST_CH_CFG_PIN  MCU_AFIO_GPIO(LED_BLINK_PIN, 1)
#   define  PWM_TEST_CH_CFG_INV  false 
#   define  PWM_TEST_CH_NUM      1
#   define  PWM_TEST_IRQ_PRIO    0
#else
#   define  PWM_TEST_CH_CFG_PIN  LED_BLINK_PIN
#   define  PWM_TEST_CH_CFG_INV  true 
#   define  PWM_TEST_CH_NUM      0
#   define  PWM_TEST_IRQ_PRIO    3
#endif

struct pwm_dev *pwm;
static uint32_t pwm_freq = 200;
static uint32_t max_steps = 200; /* two seconds motion up/down */
static uint16_t top_val;
static volatile uint32_t step = 0;
static volatile bool up = false;
static volatile int func_num = 1;
static easing_int_func_t easing_funct = sine_int_io;

static void
pwm_cycle_handler(void* unused)
{
    int16_t eased;
    eased = easing_funct(step, max_steps, top_val);
    pwm_set_duty_cycle(pwm, PWM_TEST_CH_NUM, eased);
    if (step >= max_steps || step <= 0) {
        up = !up;
    }

    step += (up) ? 1 : -1;
}

static void
pwm_end_seq_handler(void* unused)
{
    int rc;
    step = 0;
    up = false;

    switch (func_num)
    {
    case 0:
        easing_funct = sine_int_io;
        console_printf ("Easing: sine io\n");
        break;
    case 1:
        easing_funct = bounce_int_io;
        console_printf ("Easing: bounce io\n");
        break;
    case 2:
        easing_funct = circular_int_io;
        console_printf ("Easing: circular io\n");
        break;
    case 3:
        easing_funct = quadratic_int_io;
        console_printf ("Easing: quadratic io\n");
        break;
    case 4:
        easing_funct = cubic_int_io;
        console_printf ("Easing: cubic io\n");
        break;
    case 5:
        easing_funct = quartic_int_io;
        console_printf ("Easing: quartic io\n");
        break;
    default:
        easing_funct = quintic_int_io;
        console_printf ("Easing: quintic io\n");
    }

    if (func_num > 5) {
        func_num = 0;
    } else {
        func_num++;
    }
    rc = pwm_disable(pwm); /* Not needed but used for testing purposes. */
    assert(rc == 0);

    rc = pwm_enable(pwm);
    assert(rc == 0);
}

int
pwm_init(struct pwm_dev *pwm_dev, int pin)
{
    struct pwm_chan_cfg chan_conf = {
        .pin = PWM_TEST_CH_CFG_PIN,
        .inverted = PWM_TEST_CH_CFG_INV,
        .data = NULL,
    };
    struct pwm_dev_cfg dev_conf = {
        .n_cycles = pwm_freq * 6, /* 6 seconds cycles */
        .int_prio = PWM_TEST_IRQ_PRIO,
        .cycle_handler = pwm_cycle_handler, /* this won't work on soft_pwm */
        .seq_end_handler = pwm_end_seq_handler, /* this won't work on soft_pwm */
        .cycle_data = NULL,
        .seq_end_data = NULL,
        .data = NULL
    };
    int rc;

    if (pwm) {
        pwm = pwm_dev;
    } else {
        pwm = (struct pwm_dev *)os_dev_open(PWM_TEST_DEV, 0, NULL);
        if (!pwm) {
            console_printf("Device %s not available\n", PWM_TEST_DEV);
            return 0;
        }
    }

    if (pin >= 0) {
        chan_conf.pin = pin;
    }

    pwm_configure_device(pwm, &dev_conf);

    /* set the PWM frequency */
    rc = pwm_set_frequency(pwm, pwm_freq);
    assert(rc > 0);

    top_val = (uint16_t) pwm_get_top_value(pwm);

    /* setup led */
    rc = pwm_configure_channel(pwm, PWM_TEST_CH_NUM, &chan_conf);
    assert(rc == 0);

    /* console_printf ("Easing: sine io\n"); */
    rc = pwm_set_duty_cycle(pwm, PWM_TEST_CH_NUM, top_val);
    rc = pwm_enable(pwm);
    assert(rc == 0);

    return rc;
}

int
main(int argc, char **argv)
{
    sysinit();

#if MYNEWT_VAL(SHELL_TASK)
    pwm_shell_init();
#else
    pwm_init(NULL, -1);
#endif

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return(0);
}
