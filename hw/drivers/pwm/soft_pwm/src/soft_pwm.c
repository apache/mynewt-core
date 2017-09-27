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

#include <hal/hal_bsp.h>
#include <assert.h>
#include <os/os.h>
#include <errno.h>
#include <pwm/pwm.h>
#include <string.h>
#include <hal/hal_timer.h>

#define BASE_FREQ MYNEWT_VAL(OS_CPUTIME_FREQ)

struct soft_pwm_channel {
    uint8_t pin;
    uint16_t fraction;
    uint32_t duty_ticks;
    bool inverted;
    bool playing;
    struct hal_timer toggle_timer;
}

struct soft_pwm_dev_global {
    uint32_t frequency;
    uint16_t top_value;
    uint16_t pwm_ticks;
    struct hal_timer start_timer;
    soft_pwm_channel chans[SPWM_CHANS];
};

static struct soft_pwm_dev_global soft_pwm_dev;

static void start_cb(void* arg)
{

}

static void toggle_cb(void* arg)
{

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
 * @param arg  Argument provided by higher layer to open, in this case
 *             it can be a nrf_drv_pwm_config_t, to override the default
 *             configuration.
 *
 * @return 0 on success, non-zero on failure.
 */
static int
soft_pwm_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct pwm_dev *dev;
    int stat = 0;
    int inst_id;
    int chan;

    dev = (struct pwm_dev *) odev;
    inst_id = dev->pwm_instance_id;

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

    soft_pwm_dev.frequency = 2;
    soft_pwm_dev.pwm_ticks = BASEFREQ / 2;
    os_cputime_timer_init(soft_pwm_dev.toggle_timer,
                          start_cb,
                          &soft_pwm_dev);

    for (chan = 0, chan < SPWM_CHANS; chan++) {
        soft_pwm_dev.chans[chan].pin = PIN_NOT_USED;
        soft_pwm_dev.chans[chan].fraction = ;
        soft_pwm_dev.chans[chan].duty_ticks = BASEFREQ / 4;
        soft_pwm_dev.chans[chan].inverted = false;
        soft_pwm_dev.chans[chan].playing = false;
        os_cputime_timer_init(soft_pwm_dev.chans[chan].toggle_timer,
                              toggle_cb,
                              &soft_pwm_dev.chans[chan]);
    }

    if (stat) {
        return (stat);
    }

    return (0);
}

/**
 * Close the SOFT PWM device.
 *
 * This function unlocks the device.
 *
 * @param odev The device to close.
 */
static int
soft_pwm_close(struct os_dev *odev)
{
    struct pwm_dev *dev;
    int inst_id;

    dev = (struct pwm_dev *) odev;
    inst_id = dev->pwm_instance_id;

    for (chan = 0, chan < SPWM_CHANS; chan++) {
        soft_pwm_dev.chans[chan].pin = NOT_USED;
        soft_pwm_dev.chans[chan].fraction = 833;
        soft_pwm_dev.chans[chan].duty_ticks = 1666;
        soft_pwm_dev.chans[chan].inverted = false;
        soft_pwm_dev.chans[chan].playing = false;
    }


    if (os_started()) {
        os_mutex_release(&dev->pwm_lock);
    }

    return (0);
}

/**
 * Play using current configuration.
 */
static void
play_current_config(struct soft_pwm_dev_global *instance)
{
    nrf_pwm_sequence_t const seq =
        {
            .values.p_individual = instance->duty_cycles,
            .length              = 4,
            .repeats             = 0,
            .end_delay           = 0
        };

    nrf_drv_pwm_simple_playback(&instances->drv_instance,
                                &seq,
                                1,
                                NRF_DRV_PWM_FLAG_LOOP);
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
    soft_pwm_dev.chans[chan].pin = cfg->pin;
    soft_pwm_dev.chans[chan].inverted = cfg->inverted;

    if (soft_pwm_dev.playing) {
        os_cputime_timer_stop(soft_pwm_dev.toggle_timer);
        //set pin to low
        play_current_config(&instances[inst_id]);
    }

    return (0);
}

/**
 * Enable the PWM with specified duty cycle.
 *
 * This duty cycle is a fractional duty cycle where 0 == off, 65535=on,
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
    int stat;
    assert (soft_pwm_dev.chans[cnum].pin != PIN_NOT_USED);

    soft_pwm_dev.chans[cnum].fraction =
        (fraction <= soft_pwm_dev.top_value) ?
        fraction :
        soft_pwm_dev.top_value;

    if (soft_pwm_dev.playing) {
        os_cputime_timer_stop(soft_pwm_dev.toggle_timer);
        play_current_config(&instances[inst_id]);
    } else {
        instances[inst_id].playing = true;
    }
    play_current_config(&instances[inst_id]);
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
    int inst_id = dev->pwm_instance_id;
    instances[inst_id].config.output_pins[cnum] = NRF_DRV_PWM_PIN_NOT_USED;

    nrf_drv_pwm_uninit(&instances[inst_id].drv_instance);
    nrf_drv_pwm_init(&instances[inst_id].drv_instance,
                     &instances[inst_id].config,
                     NULL);
    /* check if there is any channel in use */
    if (instances[inst_id].playing) {
        play_current_config(&instances[inst_id]);
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

    soft_pwm_dev.top_value = BASE_FREQ / freq_hz;
    soft_pwm_dev.pwm_ticks = 1 / freq_hz;

    if (instances[inst_id].playing) {
        nrf_drv_pwm_uninit(&instances[inst_id].drv_instance);
        nrf_drv_pwm_init(&instances[inst_id].drv_instance,
                         &instances[inst_id].config,
                         NULL);

        play_current_config(&instances[inst_id]);
    }

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
    return (-EINVAL);
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
