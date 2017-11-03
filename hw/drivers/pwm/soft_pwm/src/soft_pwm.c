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

#include <assert.h>
#include <os/os.h>
#include <errno.h>
#include <string.h>
#include <pwm/pwm.h>
#include <hal/hal_bsp.h>
#include <hal/hal_gpio.h>

#define BASE_FREQ MYNEWT_VAL(OS_CPUTIME_FREQ)
#define MAX_FREQ BASE_FREQ / 2
#define CHAN_COUNT MYNEWT_VAL(SOFT_PWM_CHANS)
#define PIN_NOT_USED 0xff


struct soft_pwm_channel {
    uint16_t pin;
    uint16_t fraction;
    bool inverted;
    bool playing;
    struct hal_timer toggle_timer;
};

struct soft_pwm_dev_global {
    uint32_t frequency;
    uint16_t top_value;
    struct hal_timer cycle_timer;
    struct soft_pwm_channel chans[CHAN_COUNT];
};

static struct soft_pwm_dev_global soft_pwm_dev;

/**
 * Cycle start callback
 *
 * Initializes every channel's output to high (or low accrding to its polarity).
 * Schedules toggle_cb for every channel.
 */
static void cycle_cb(void* arg)
{
    int cnum;
    bool inverted;
    uint32_t now = os_cputime_get32();

    for (cnum = 0; cnum < CHAN_COUNT; cnum++) {
        if (soft_pwm_dev.chans[cnum].playing) {

            inverted = soft_pwm_dev.chans[cnum].inverted;
            hal_gpio_write(soft_pwm_dev.chans[cnum].pin, (inverted) ? 0 : 1);

            os_cputime_timer_start(&soft_pwm_dev.chans[cnum].toggle_timer,
                                   now + soft_pwm_dev.chans[cnum].fraction);
        }
    }
    os_cputime_timer_start(&soft_pwm_dev.cycle_timer,
                           now + soft_pwm_dev.top_value);
}

/**
 * Channel output toggle callback
 *
 * Toggles a channel's output.
 */
static void toggle_cb(void* arg)
{
    struct soft_pwm_channel* chan = (struct soft_pwm_channel*) arg;
    hal_gpio_toggle(chan->pin);
}

/**
 * Open the Soft PWM device
 *
 * This function locks the device for access from other tasks.
 *
 * @param odev The OS device to open
 * @param wait The time in MS to wait.  If 0 specified, returns immediately
 *             if resource unavailable.  If OS_WAIT_FOREVER specified, blocks
 *             until resource is available.
 * @param arg
 *
 * @return 0 on success, non-zero on failure.
 */
static int
soft_pwm_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct pwm_dev *dev;
    int stat = 0;
    int cnum;

    dev = (struct pwm_dev *) odev;

    if (os_started()) {
        stat = os_mutex_pend(&dev->pwm_lock, wait);
        if (stat != OS_OK) {
            return (stat);
        }
    }

    if (odev->od_flags & OS_DEV_F_STATUS_OPEN) {
        os_mutex_release(&dev->pwm_lock);
        stat = OS_EBUSY;
        return (stat);
    }

    soft_pwm_dev.frequency = 100;
    soft_pwm_dev.top_value = BASE_FREQ / 100;
    os_cputime_timer_init(&soft_pwm_dev.cycle_timer,
                          cycle_cb,
                          NULL);

    for (cnum = 0; cnum < CHAN_COUNT; cnum++) {
        soft_pwm_dev.chans[cnum].pin = PIN_NOT_USED;
        soft_pwm_dev.chans[cnum].fraction = soft_pwm_dev.top_value / 2;
        soft_pwm_dev.chans[cnum].inverted = false;
        soft_pwm_dev.chans[cnum].playing = false;
        os_cputime_timer_init(&soft_pwm_dev.chans[cnum].toggle_timer,
                              toggle_cb,
                              &soft_pwm_dev.chans[cnum]);
    }

    /* Start */
    cycle_cb(NULL);

    return (0);
}

/**
 * Close the Soft PWM device.
 *
 * This function unlocks the device.
 *
 * @param odev The device to close.
 */
static int
soft_pwm_close(struct os_dev *odev)
{
    struct pwm_dev *dev;
    int cnum;

    dev = (struct pwm_dev *) odev;

    os_cputime_timer_stop(&soft_pwm_dev.cycle_timer);
    for (cnum = 0; cnum < CHAN_COUNT; cnum++) {
        os_cputime_timer_stop(&soft_pwm_dev.chans[cnum].toggle_timer);
    }

    if (os_started()) {
        os_mutex_release(&dev->pwm_lock);
    }

    return (0);
}

/**
 * Configure a channel on the PWM device.
 *
 * @param dev The device to configure.
 * @param cnum The channel number to configure.
 * @param cfg  A pwm_chan_cfg data structure.
 *
 * @return 0 on success, non-zero error code on failure.
 */
static int
soft_pwm_configure_channel(struct pwm_dev *dev,
                           uint8_t cnum,
                           struct pwm_chan_cfg *cfg)
{
    uint16_t last_pin = (soft_pwm_dev.chans[cnum].pin == PIN_NOT_USED) ?
        cfg->pin :
        soft_pwm_dev.chans[cnum].pin;

    /* Set the previously used pin to low */
    if (cfg->pin != last_pin) {
        if (soft_pwm_dev.chans[cnum].playing) {
            soft_pwm_dev.chans[cnum].playing = false;
            os_cputime_timer_stop(&soft_pwm_dev.chans[cnum].toggle_timer);
        }
        hal_gpio_write(last_pin, 0);
    }

    soft_pwm_dev.chans[cnum].pin = cfg->pin;
    soft_pwm_dev.chans[cnum].inverted = cfg->inverted;

    hal_gpio_init_out(cfg->pin, (cfg->inverted) ? 1 : 0);

    return (0);
}

/**
 * Enable the PWM with specified duty cycle.
 *
 * This duty cycle is a fractional duty cycle where 0 == off, clk_freq/pwm_freq=on,
 * and any value in between is on for fraction clocks and off
 * for 65535-fraction clocks.
 *
 * @param dev The device to configure.
 * @param cnum The channel number. The channel should be in use.
 * @param fraction The fraction value.
 *
 * @return 0 on success, negative on error.
 */
static int
soft_pwm_enable_duty_cycle(struct pwm_dev *dev, uint8_t cnum, uint16_t fraction)
{
    bool level;
    assert (soft_pwm_dev.chans[cnum].pin != PIN_NOT_USED);

    soft_pwm_dev.chans[cnum].fraction =
        (fraction <= soft_pwm_dev.top_value) ?
        fraction :
        soft_pwm_dev.top_value;

    /* Handling 0% and 100% duty cycles. */
    if ((fraction < soft_pwm_dev.top_value) && (fraction > 0)) {
        soft_pwm_dev.chans[cnum].playing = true;
    } else {
        soft_pwm_dev.chans[cnum].playing = false;
        os_cputime_timer_stop(&soft_pwm_dev.chans[cnum].toggle_timer);

        level = (soft_pwm_dev.chans[cnum].inverted && fraction == 0) ||
            (!soft_pwm_dev.chans[cnum].inverted && fraction != 0);
        hal_gpio_write(soft_pwm_dev.chans[cnum].pin,
                       (level) ? 1 : 0);
    }

    return (0);
}

/**
 * Disable the PWM channel, it will be marked as unconfigured.
 *
 * @param dev The device to configure.
 * @param cnum The channel number.
 *
 * @return 0 success, negative on error.
 */
static int
soft_pwm_disable(struct pwm_dev *dev, uint8_t cnum)
{
    soft_pwm_dev.chans[cnum].playing = false;
    os_cputime_timer_stop(&soft_pwm_dev.chans[cnum].toggle_timer);
    hal_gpio_write(soft_pwm_dev.chans[cnum].pin, 0);
    return (0);
}

/**
 * This frequency must be between 1/2 the clock frequency and
 * the clock divided by the resolution. NOTE: This may affect
 * other PWM channels.
 *
 * @param dev The device to configure.
 * @param freq_hz The frequency value in Hz.
 *
 * @return A value is in Hz on success, negative on error.
 */
static int
soft_pwm_set_frequency(struct pwm_dev *dev, uint32_t freq_hz)
{
    freq_hz = (freq_hz > MAX_FREQ) ? MAX_FREQ : freq_hz;
    freq_hz = (freq_hz < 2) ? 2 : freq_hz;

    soft_pwm_dev.top_value = BASE_FREQ / freq_hz;

    return BASE_FREQ;
}

/**
 * Get the underlying clock driving the PWM device.
 *
 * @param dev
 *
 * @return value is in Hz on success, negative on error.
 */
static int
soft_pwm_get_clock_freq(struct pwm_dev *dev)
{
    return BASE_FREQ;
}

/**
 * Get the resolution of the PWM in bits.
 *
 * @param dev The device to query.
 *
 * @return The value in bits on success, negative on error.
 */
static int
soft_pwm_get_resolution_bits(struct pwm_dev *dev)
{
    int shamt;
    uint16_t mask = 0x8000;

    for (shamt = 0; shamt < 15; shamt++) {
        if (soft_pwm_dev.top_value & mask) {
            break;
        }
        mask >>= 1;
    }

    return (16 - shamt - 1);
}

/**
 * Callback to initialize an adc_dev structure from the os device
 * initialization callback.  This sets up a soft_pwm_device(), so
 * that subsequent lookups to this device allow us to manipulate it.
 */
int
soft_pwm_dev_init(struct os_dev *odev, void *arg)
{
    struct pwm_dev *dev;
    struct pwm_driver_funcs *pwm_funcs;

    dev = (struct pwm_dev *) odev;
    dev->pwm_instance_id = 0;
    dev->pwm_chan_count = CHAN_COUNT;

    OS_DEV_SETHANDLERS(odev, soft_pwm_open, soft_pwm_close);

    pwm_funcs = &dev->pwm_funcs;
    pwm_funcs->pwm_configure_channel = soft_pwm_configure_channel;
    pwm_funcs->pwm_enable_duty_cycle = soft_pwm_enable_duty_cycle;
    pwm_funcs->pwm_set_frequency = soft_pwm_set_frequency;
    pwm_funcs->pwm_get_clock_freq = soft_pwm_get_clock_freq;
    pwm_funcs->pwm_get_resolution_bits = soft_pwm_get_resolution_bits;
    pwm_funcs->pwm_disable = soft_pwm_disable;

    return (0);
}
