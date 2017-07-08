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
 * @param data Driver specific configuration data for this channel.
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
pwm_chan_config(struct pwm_dev *dev, uint8_t cnum, void *data)
{
    assert(dev->pwm_funcs.pwm_configure_channel != NULL);

    if (cnum > dev->pwm_chan_count) {
        return (EINVAL);
    }

    return (dev->pwm_funcs.pwm_configure_channel(dev, cnum, data));
}

/**
 * Enable the PWM with specified duty cycle.
 *
 * This duty cycle is a fractional duty cycle where 0 == off, 65535=on,
 * and any value in between is on for fraction clocks and off
 * for 65535-fraction clocks.
 *
 * @param dev The device to configure.
 * @param fraction The fraction value.
 *
 * @return 0 on success, negative on error.
 */
int
pwm_enable_duty_cycle(struct pwm_dev *pwm_d, uint16_t fraction)
{
    assert(dev->pwm_funcs.pwm_enable_duty_cycle != NULL);

    return (dev->pwm_funcs.pwm_enable_duty_cycle(dev, cnum, data));
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
    assert(dev->pwm_funcs.pwm_disable_channel != NULL);

    if (cnum > dev->pwm_chan_count) {
        return (EINVAL);
    }

    if (!dev->pwm_chans[cnum].c_configured) {
        return (EINVAL);
    }

    return (dev->pwm_funcs.pwm_disable_channel(dev, cnum));
}

/* /\** */
/*  * Set an event handler.  This handler is called for all ADC events. */
/*  * */
/*  * @param dev The PWM device to set the event handler for */
/*  * @param func The event handler function to call */
/*  * @param arg The argument to pass the event handler function */
/*  * */
/*  * @return 0 on success, non-zero on failure */
/*  *\/ */
/* int */
/* adc_event_handler_set(struct adc_dev *dev, adc_event_handler_func_t func, */
/*         void *arg) */
/* { */
/*     dev->ad_event_handler_func = func; */
/*     dev->ad_event_handler_arg = arg; */

/*     return (0); */
/* } */
