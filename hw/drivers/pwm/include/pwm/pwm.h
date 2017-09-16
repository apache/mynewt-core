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

#ifndef __PWM_H__
#define __PWM_H__

#include <os/os_dev.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pwm_dev;
struct pwm_chan_cfg;

/**
 * Configure a channel on the PWM device.
 *
 * @param dev The device to configure.
 * @param cnum The channel number to configure.
 * @param cfg Configuration data for this channel.
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*pwm_configure_channel_func_t)(struct pwm_dev *,
                                            uint8_t,
                                            struct pwm_chan_cfg *);

 /**
 * Enable the PWM with specified duty cycle.
 *
 * This duty cycle is a fractional duty cycle where 0 == off, 65535=on,
 * and any value in between is on for fraction clocks and off
 * for 65535-fraction clocks.
 *
 * @param dev The device to configure.
 * @param cnum The channel to configure.
 * @param fraction The fraction value.
 *
 * @return 0 on success, negative on error.
 */
typedef int (*pwm_enable_duty_cycle_func_t)(struct pwm_dev *,
                                            uint8_t,
                                            uint16_t);

/**
 * Set the frequency for the device's clock.
 * This frequency must be between 1/2 the clock frequency and
 * the clock divided by the resolution.
 *
 * @param dev The device to configure.
 * @param freq_hz The frequency value in Hz.
 *
 * @return 0 on success, negative on error.
 */
typedef int (*pwm_set_frequency_func_t) (struct pwm_dev *, uint32_t);

/**
 * Get the underlying clock driving the PWM device.
 *
 * @param dev
 *
 * @return value is in Hz on success, negative on error.
 */
typedef int (*pwm_get_clock_freq_func_t) (struct pwm_dev *);

/**
 * Get the resolution of the PWM in bits.
 *
 * @param dev The device to query.
 *
 * @return The value in bits on success, negative on error.
 */
typedef int (*pwm_get_resolution_bits_func_t) (struct pwm_dev *);

/**
 * Disable the PWM channel, it will be marked as unconfigured.
 *
 * @param dev The device to configure.
 * @param cnum The channel number.
 *
 * @return 0 on success, negative on error.
 */
typedef int (*pwm_disable_func_t) (struct pwm_dev *, uint8_t);

struct pwm_driver_funcs {
    pwm_configure_channel_func_t pwm_configure_channel;
    pwm_enable_duty_cycle_func_t pwm_enable_duty_cycle;
    pwm_set_frequency_func_t pwm_set_frequency;
    pwm_get_clock_freq_func_t pwm_get_clock_freq;
    pwm_get_resolution_bits_func_t pwm_get_resolution_bits;
    pwm_disable_func_t pwm_disable;
};

struct pwm_dev {
    struct os_dev pwm_os_dev;
    struct os_mutex pwm_lock;
    struct pwm_driver_funcs pwm_funcs;
    uint32_t pwm_chan_count;
    int pwm_instance_id;
};

/**
 * PWM channel configuration data.
 *
 * pin - The pin to be assigned to this pwm channel.
 * inverted - Whether this channel's output polarity is inverted or not.
 * data - A pointer do a driver specific parameter.
 */
struct pwm_chan_cfg {
    uint8_t pin;
    bool inverted;
    void* data;
};

int pwm_chan_config(struct pwm_dev *dev, uint8_t cnum, struct pwm_chan_cfg *cfg);
int pwm_enable_duty_cycle(struct pwm_dev *pwm_d, uint8_t cnum, uint16_t fraction);
int pwm_set_frequency(struct pwm_dev *dev, uint32_t freq_hz);
int pwm_get_clock_freq(struct pwm_dev *dev);
int pwm_get_resolution_bits(struct pwm_dev *dev);
int pwm_disable(struct pwm_dev *dev, uint8_t cnum);

#ifdef __cplusplus
}
#endif

#endif /* __PWM_H__ */
