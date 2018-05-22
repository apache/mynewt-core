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


#include <pwm/pwm.h>
#include <errno.h>
#include <assert.h>

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
pwm_configure_device(struct pwm_dev *dev, struct pwm_dev_cfg *cfg)
{
    assert(dev);
    assert(dev->pwm_funcs.pwm_configure_device != NULL);

    return (dev->pwm_funcs.pwm_configure_device(dev, cfg));
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
int
pwm_configure_channel(struct pwm_dev *dev,
                      uint8_t cnum,
                      struct pwm_chan_cfg *cfg)
{
    assert(dev);
    assert(dev->pwm_funcs.pwm_configure_channel != NULL);
    if (cnum >= dev->pwm_chan_count) {
        return (EINVAL);
    }

    return (dev->pwm_funcs.pwm_configure_channel(dev, cnum, cfg));
}

/**
 * Set the specified duty cycle on a PWM Channel.
 *
 * This duty cycle is a fractional duty cycle where 0 == off,
 * base_freq / pwm_freq == 100% and any value in between is on for fraction clock
 * cycles and off for (base_freq / pwm_freq)-fraction clock cycles.
 *
 * @param dev The device which the channel belongs to.
 * @param cnum The channel number. This channel should be already configured.
 * @param fraction The fraction value.
 *
 * @return 0 on success, negative on error.
 */
int
pwm_set_duty_cycle(struct pwm_dev *dev, uint8_t cnum, uint16_t fraction)
{
    assert(dev);
    assert(dev->pwm_funcs.pwm_set_duty_cycle != NULL);
    if (cnum >= dev->pwm_chan_count) {
        return (EINVAL);
    }

    return (dev->pwm_funcs.pwm_set_duty_cycle(dev, cnum, fraction));
}

/**
 * Enable a given PWM device.
 * This device should start playing on its previously configured channels.
 *
 * @param dev The PWM device to be enabled.
 *
 * @return 0 on success, negative on error.
 */
int
pwm_enable(struct pwm_dev *dev)
{
    assert(dev);
    assert(dev->pwm_funcs.pwm_enable != NULL);

    return (dev->pwm_funcs.pwm_enable(dev));
}

/**
 * Check whether a PWM device is enabled.
 *
 * @param dev The device which the channel belongs to.
 *
 * @return true if enabled, false if not.
 */
bool
pwm_is_enabled(struct pwm_dev *dev)
{
    assert(dev);
    assert(dev->pwm_funcs.pwm_is_enabled != NULL);

    return (dev->pwm_funcs.pwm_is_enabled(dev));
}

/**
 * Set the frequency for the device's clock.
 * This frequency must be between 1/2 the clock frequency and
 * the clock divided by the resolution.
 *
 * @param dev The device to configure.
 * @param freq_hz The frequency value in Hz.
 *
 * @return A value is in Hz on success, negative on error.
 */
int
pwm_set_frequency(struct pwm_dev *dev, uint32_t freq_hz)
{
    assert(dev);
    assert(dev->pwm_funcs.pwm_set_frequency != NULL);

    return (dev->pwm_funcs.pwm_set_frequency(dev, freq_hz));
}

/**
 * Get the underlying clock driving the PWM device.
 *
 * @param dev
 *
 * @return value is in Hz on success, negative on error.
 */
int
pwm_get_clock_freq(struct pwm_dev *dev)
{
    assert(dev);
    assert(dev->pwm_funcs.pwm_get_clock_freq != NULL);

    return (dev->pwm_funcs.pwm_get_clock_freq(dev));
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
pwm_get_top_value(struct pwm_dev *dev)
{
    assert(dev);
    assert(dev->pwm_funcs.pwm_get_top_value != NULL);

    return (dev->pwm_funcs.pwm_get_top_value(dev));
}

/**
 * Get the resolution of the PWM in bits.
 *
 * @param dev The device to query.
 *
 * @return The value in bits on success, negative on error.
 */
int
pwm_get_resolution_bits(struct pwm_dev *dev)
{
    assert(dev);
    assert(dev->pwm_funcs.pwm_get_resolution_bits != NULL);

    return (dev->pwm_funcs.pwm_get_resolution_bits(dev));
}

/**
 * Disable the PWM device, the device will stop playing while remaining configured.
 *
 * @param dev The device to disable.
 *
 * @return 0 on success, negative on error.
 */
int
pwm_disable(struct pwm_dev *dev)
{
    assert(dev);
    assert(dev->pwm_funcs.pwm_disable != NULL);

    return (dev->pwm_funcs.pwm_disable(dev));
}
