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
 * Configure a channel on the PWM device.
 *
 * @param dev The device to configure.
 * @param cnum The channel number to configure.
 * @param cfg Configuration data for this channel.
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
pwm_chan_config(struct pwm_dev *dev, uint8_t cnum, struct pwm_chan_cfg *cfg)
{
    assert(dev->pwm_funcs.pwm_configure_channel != NULL);

    if (cnum >= dev->pwm_chan_count) {
        return (EINVAL);
    }

    return (dev->pwm_funcs.pwm_configure_channel(dev, cnum, cfg));
}

/**
 * Enable the PWM with specified duty cycle.
 *
 * This duty cycle is a fractional duty cycle where 0 == off, 65535=on,
 * and any value in between is on for fraction clocks and off
 * for 65535-fraction clocks.
 *
 * @param dev The device to configure.
 * @param cnum The channel number.
 * @param fraction The fraction value.
 *
 * @return 0 on success, negative on error.
 */
int
pwm_enable_duty_cycle(struct pwm_dev *dev, uint8_t cnum, uint16_t fraction)
{
    assert(dev->pwm_funcs.pwm_enable_duty_cycle != NULL);
    if (cnum >= dev->pwm_chan_count) {
        return (EINVAL);
    }

    return (dev->pwm_funcs.pwm_enable_duty_cycle(dev, cnum, fraction));
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
    assert(dev->pwm_funcs.pwm_get_clock_freq != NULL);

    return (dev->pwm_funcs.pwm_get_clock_freq(dev));
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
    assert(dev->pwm_funcs.pwm_get_resolution_bits != NULL);

    return (dev->pwm_funcs.pwm_get_resolution_bits(dev));
}

/**
 * Disable the PWM channel, it will be marked as unconfigured.
 *
 * @param dev The device to configure.
 * @param cnum The channel number.
 *
 * @return 0 success, negative on error.
 */
int
pwm_disable(struct pwm_dev *dev, uint8_t cnum)
{
    assert(dev->pwm_funcs.pwm_disable != NULL);

    if (cnum > dev->pwm_chan_count) {
        return (EINVAL);
    }

    return (dev->pwm_funcs.pwm_disable(dev, cnum));
}
