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
#include <errno.h>
#include <string.h>
#include "os/mynewt.h"
#include <pwm/pwm.h>
#include <hal/hal_bsp.h>
#include <hal/hal_gpio.h>

#define BASE_FREQ MYNEWT_VAL(OS_CPUTIME_FREQ)
#define MAX_FREQ BASE_FREQ / 2
#define DEV_COUNT MYNEWT_VAL(SOFT_PWM_DEVS)
#define CHAN_COUNT MYNEWT_VAL(SOFT_PWM_CHANS)
#define NO_PIN 0xff

struct soft_pwm_channel {
    uint8_t pin;
    bool inverted;
    uint16_t fraction;
    bool running;
    struct hal_timer toggle_timer;
};

struct soft_pwm_dev {
    bool playing;
    uint32_t frequency;
    uint16_t top_value;
    uint32_t n_cycles;
    uint32_t cycle_cnt;
    user_handler_t cycle_handler;
    user_handler_t seq_end_handler;
    void* cycle_data;
    void* seq_end_data;
    struct hal_timer cycle_timer;
    struct soft_pwm_channel chans[CHAN_COUNT];
};

static struct soft_pwm_dev instances[DEV_COUNT];

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
    struct soft_pwm_dev* instance = (struct soft_pwm_dev *) arg;
    struct soft_pwm_channel* chans = instance->chans;
    uint32_t now = os_cputime_get32();

    if (instance->n_cycles) {
        instance->cycle_cnt++;
        instance->playing = (instance->cycle_cnt < instance->n_cycles);
    }

    if (instance->playing) {
        for (cnum = 0; cnum < CHAN_COUNT; cnum++) {
            if (chans[cnum].running) {
                inverted = chans[cnum].inverted;
                hal_gpio_write(chans[cnum].pin, (inverted) ? 0 : 1);

                os_cputime_timer_start(&chans[cnum].toggle_timer,
                                   now + chans[cnum].fraction);
            }
        }
        os_cputime_timer_start(&instance->cycle_timer,
                               now + instance->top_value);

        if (instance->cycle_handler) {
            instance->cycle_handler(instance->cycle_data);
        }

    } else if (instance->seq_end_handler) {
        instance->seq_end_handler(instance->seq_end_data);
    }
}

/**
 * Channel output toggle callback
 *
 * Toggles a channel's output.
 */
static void toggle_cb(void* arg)
{
    uint32_t *pin = (uint32_t *) arg;
    hal_gpio_toggle(*pin);
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
 * @return 0 on success, non-zero on failure.
 */
static int
soft_pwm_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    uint32_t cnum;
    uint32_t stat = 0;
    struct pwm_dev *dev = (struct pwm_dev *) odev;
    struct soft_pwm_dev *instance = &instances[dev->pwm_instance_id];

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

    instance->frequency = 100;
    instance->top_value = BASE_FREQ / 100;
    os_cputime_timer_init(&instance->cycle_timer,
                          cycle_cb,
                          &instances[dev->pwm_instance_id]);

    instance->playing = false;
    for (cnum = 0; cnum < CHAN_COUNT; cnum++) {
        instance->chans[cnum].pin = NO_PIN;
        instance->chans[cnum].fraction = 0;
        instance->chans[cnum].inverted = false;
        instance->chans[cnum].running = false;
        os_cputime_timer_init(&instance->chans[cnum].toggle_timer,
                              toggle_cb,
                              &instance->chans[cnum].pin);
    }

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
    uint8_t cnum;
    struct pwm_dev *dev = (struct pwm_dev *) odev;
    struct soft_pwm_dev *instance = &instances[dev->pwm_instance_id];

    os_cputime_timer_stop(&instance->cycle_timer);
    for (cnum = 0; cnum < CHAN_COUNT; cnum++) {
        os_cputime_timer_stop(&instance->chans[cnum].toggle_timer);
    }

    if (os_started()) {
        os_mutex_release(&dev->pwm_lock);
    }

    return (0);
}

/**
 * Configure a PWM device.
 *
 * @param dev The device to configure.
 * @param cfg Configuration data for this device. If NULL the device will be
 * given default configuration values.
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
soft_pwm_configure_device(struct pwm_dev *dev, struct pwm_dev_cfg *cfg)
{
    struct soft_pwm_dev *instance = &instances[dev->pwm_instance_id];

    if (!cfg) {
        instance->cycle_handler = NULL;
        instance->seq_end_handler = NULL;
        instance->cycle_data = NULL;
        instance->seq_end_data = NULL;
        cfg->n_cycles = 0;
        return (0);
    }

    instance->n_cycles = (cfg->n_cycles) ? cfg->n_cycles : 0;

    /* Configure Interrupts  */
    if (cfg->cycle_handler || cfg->seq_end_handler) {
        instance->cycle_handler = (user_handler_t) cfg->cycle_handler;
        instance->seq_end_handler = (user_handler_t) cfg->seq_end_handler;
        instance->cycle_data = cfg->cycle_data;
        instance->seq_end_data = cfg->seq_end_data;
    }

    return (0);
}

/**
 * Configure a channel on the PWM device.
 *
 * @param dev The device to configure.
 * @param cnum The channel number to configure.
 * @param cfg Configuration data for this channel. If NULL the channel will be
 * disabled or given default configuration values.
 *
 * @return 0 on success, non-zero error code on failure.
 */
static int
soft_pwm_configure_channel(struct pwm_dev *dev,
                           uint8_t cnum,
                           struct pwm_chan_cfg *cfg)
{
    uint32_t last_pin;
    struct soft_pwm_dev *instance = &instances[dev->pwm_instance_id];
    struct soft_pwm_channel *chan = &instance->chans[cnum];

    last_pin = (chan->pin == NO_PIN) ?
        cfg->pin :
        chan->pin;

    /* Set the previously used pin to low */
    if (cfg->pin != last_pin) {
        if (chan->running) {
            chan->running = false;
            os_cputime_timer_stop(&chan->toggle_timer);
        }
        hal_gpio_write(last_pin, 0);
    }

    if (cfg) {
        chan->pin = cfg->pin;
        chan->inverted = cfg->inverted;
        hal_gpio_init_out(cfg->pin, (cfg->inverted) ? 1 : 0);
    } else { /* unconfigure the channel */
        chan->pin = NO_PIN;
        chan->inverted = false;
        chan->fraction = 0;
        chan->running = false;
    }

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
soft_pwm_enable(struct pwm_dev *dev)
{
    uint8_t cnum;
    bool level;
    struct soft_pwm_dev* instance = &instances[dev->pwm_instance_id];
    struct soft_pwm_channel* chans = instance->chans;

    /* Handling 0% and 100% duty cycle channels. */
    for (cnum = 0; cnum < CHAN_COUNT; cnum++) {
        if ((chans[cnum].pin != NO_PIN) && (!chans[cnum].running)) {
            level = (chans[cnum].inverted && chans[cnum].fraction == 0) ||
                (!chans[cnum].inverted && chans[cnum].fraction != 0);
            hal_gpio_write(chans[cnum].pin, (level) ? 1 : 0);
        }
    }

    if (instance->n_cycles) {
        instance->cycle_cnt = 0;
    }

    instance->playing = true;
    cycle_cb(instance);
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
soft_pwm_set_duty_cycle(struct pwm_dev *dev, uint8_t cnum, uint16_t fraction)
{
    bool level;
    struct soft_pwm_dev* instance = &instances[dev->pwm_instance_id];
    struct soft_pwm_channel* chan = &instance->chans[cnum];
    assert (chan->pin != NO_PIN);

    chan->fraction = (fraction <= instance->top_value) ?
        fraction :
        instance->top_value;

    /* Handling 0% and 100% duty cycles. */
    if ((fraction < instance->top_value) && (fraction > 0)) {
        chan->running = true;
    } else {
        chan->running = false;
        os_cputime_timer_stop(&chan->toggle_timer);
        if (instance->playing) {
            level = (chan->inverted && fraction == 0) ||
                (!chan->inverted && fraction != 0);
            hal_gpio_write(chan->pin, (level) ? 1 : 0);
        }
    }
    return (0);
}

/**
 * Check whether a PWM channel is enabled on a given device.
 *
 * @param dev The device which the channel belongs to.
 * @param cnum The channel being queried.
 *
 * @return true if enabled, false if not.
 */
static bool
soft_pwm_is_enabled(struct pwm_dev *dev)
{
    return instances[dev->pwm_instance_id].playing;
}

/**
 * Disable the PWM device, the device will stop playing while
 * remaining configured.
 *
 * @param dev The device to disable.
 *
 * @return 0 on success, negative on error.
 */
static int
soft_pwm_disable(struct pwm_dev *dev)
{
    uint8_t cnum;
    struct soft_pwm_dev* instance = &instances[dev->pwm_instance_id];
    instance->playing = false;
    os_cputime_timer_stop(&instance->cycle_timer);

    for (cnum = 0; cnum < CHAN_COUNT; cnum++) {
        if (instance->chans[cnum].pin != NO_PIN) {
            os_cputime_timer_stop(&instance->chans[cnum].toggle_timer);
            hal_gpio_write(instance->chans[cnum].pin, 0);
        }
    }
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

    instances[dev->pwm_instance_id].top_value = BASE_FREQ / freq_hz;

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
 * Get the top value for the cycle counter, i.e. the value which sets
 * the duty cycle to 100%.
 *
 * @param dev
 *
 * @return value in cycles on success, negative on error.
 */
int
soft_pwm_get_top_value(struct pwm_dev *dev)
{
    return (instances[dev->pwm_instance_id].top_value);
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
    uint16_t top_val = instances[dev->pwm_instance_id].top_value;

    for (shamt = 0; shamt < 15; shamt++) {
        if (top_val & mask) {
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

    assert(odev);
    dev = (struct pwm_dev *) odev;

    assert(arg);
    dev->pwm_instance_id = *((uint8_t *) arg);
    assert(dev->pwm_instance_id < DEV_COUNT);

    dev->pwm_chan_count = CHAN_COUNT;
    os_mutex_init(&dev->pwm_lock);

    OS_DEV_SETHANDLERS(odev, soft_pwm_open, soft_pwm_close);

    pwm_funcs = &dev->pwm_funcs;
    pwm_funcs->pwm_configure_device = soft_pwm_configure_device;
    pwm_funcs->pwm_configure_channel = soft_pwm_configure_channel;
    pwm_funcs->pwm_set_duty_cycle = soft_pwm_set_duty_cycle;
    pwm_funcs->pwm_enable = soft_pwm_enable;
    pwm_funcs->pwm_is_enabled = soft_pwm_is_enabled;
    pwm_funcs->pwm_set_frequency = soft_pwm_set_frequency;
    pwm_funcs->pwm_get_clock_freq = soft_pwm_get_clock_freq;
    pwm_funcs->pwm_get_top_value = soft_pwm_get_top_value;
    pwm_funcs->pwm_get_resolution_bits = soft_pwm_get_resolution_bits;
    pwm_funcs->pwm_disable = soft_pwm_disable;

    return (0);
}
