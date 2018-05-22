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

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pwm_dev;
struct pwm_dev_cfg;
struct pwm_chan_cfg;

/**
 * Configure a PWM device.
 *
 * @param dev The device to configure.
 * @param cfg Configuration data for this device. If NULL the device will be
 * given default configuration values.
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*pwm_config_device_func_t)(struct pwm_dev *,
                                        struct pwm_dev_cfg *);

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
typedef int (*pwm_config_channel_func_t)(struct pwm_dev *,
                                         uint8_t,
                                         struct pwm_chan_cfg *);

/**
 * Set the specified duty cycle on a PWM Channel.
 *
 * This duty cycle is a fractional duty cycle where 0 == off,
 * base_freq / pwm_freq == 100% and any value in between is on for fraction clock
 * cycles and off for (base_freq / pwm_freq)-fraction clock cycles.
 *
 * @param dev The device which the channel belongs to.
 * @param cnum The channel to configure.
 * @param fraction The fraction value.
 *
 * @return 0 on success, negative on error.
 */
typedef int (*pwm_set_duty_cycle_func_t)(struct pwm_dev *,
                                         uint8_t,
                                         uint16_t);

/**
 * Enable a given PWM device.
 * This device should start playing on its previously configured channels.
 *
 * @param dev The PWM device to be enabled.
 *
 * @return 0 on success, negative on error.
 */
typedef int (*pwm_enable_func_t)(struct pwm_dev *dev);

/**
 * Check whether a PWM device is enabled.
 *
 * @param dev The device which the channel belongs to.
 *
 * @return true if enabled, false if not.
 */
typedef bool (*pwm_is_enabled_func_t)(struct pwm_dev *);

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
 * Get the top value for the cycle counter, i.e. the value which sets
 * the duty cycle to 100%.
 *
 * @param dev
 *
 * @return value in cycles on success, negative on error.
 */
typedef int (*pwm_get_top_value_funct_t) (struct pwm_dev *dev);

/**
 * Get the resolution of the PWM in bits.
 *
 * @param dev The device to query.
 *
 * @return The value in bits on success, negative on error.
 */
typedef int (*pwm_get_resolution_bits_func_t) (struct pwm_dev *);

/**
 * Disable the PWM device, the device will stop playing while remaining configured.
 *
 * @param dev The device to disable.
 *
 * @return 0 on success, negative on error.
 */
typedef int (*pwm_disable_func_t) (struct pwm_dev *);

typedef void (*user_handler_t) (void*);

struct pwm_driver_funcs {
    pwm_config_device_func_t pwm_configure_device;
    pwm_config_channel_func_t pwm_configure_channel;
    pwm_set_duty_cycle_func_t pwm_set_duty_cycle;
    pwm_enable_func_t pwm_enable;
    pwm_is_enabled_func_t pwm_is_enabled;
    pwm_set_frequency_func_t pwm_set_frequency;
    pwm_get_clock_freq_func_t pwm_get_clock_freq;
    pwm_get_top_value_funct_t pwm_get_top_value;
    pwm_get_resolution_bits_func_t pwm_get_resolution_bits;
    pwm_disable_func_t pwm_disable;
};

struct pwm_dev {
    struct os_dev pwm_os_dev;
    struct os_mutex pwm_lock;
    struct pwm_driver_funcs pwm_funcs;
    uint32_t pwm_chan_count;
    uint8_t pwm_instance_id;
};

/**
 * PWM channel configuration data.
 *
 * pin - The pin to be assigned to this pwm channel.
 * inverted - Whether this channel's output polarity is inverted or not.
 * n_cycles - The number of cycles for the channel to play. 0 for loop mode.
 * interrupts - Whether interruptions are to be configured or not.
 * data - Driver specific data.
 */
struct pwm_chan_cfg {
    uint32_t pin;
    bool inverted;
    void* data;
};

/**
 * PWM device interrupt configuration data.
 *
 * int_prio - Driver Interrupts priority.
 * cycle_handler - A pointer to a user_handler_t function for a cycle interrupt,
 * if NULL the interrupt will be disabled.
 * seq_end_handler - A pointer to a user_handler_t function for
 * a end-of-sequence interrupt, if NULL the interrupt will be disabled.
 * cycle_data - User data to be passed to cycle_handler as a parameter.
 * seq_end_data - User data to be passed to seq_end_handler as a parameter.
 * data - Driver specific device configuration data.
 *
 */
struct pwm_dev_cfg {
    uint32_t n_cycles;
    uint32_t int_prio;
    user_handler_t cycle_handler;
    user_handler_t seq_end_handler;
    void* cycle_data;
    void* seq_end_data;
    void* data;
};

int pwm_configure_device(struct pwm_dev *dev, struct pwm_dev_cfg *cfg);
int pwm_configure_channel(struct pwm_dev *dev, uint8_t cnum, struct pwm_chan_cfg *cfg);
int pwm_set_duty_cycle(struct pwm_dev *pwm_d, uint8_t cnum, uint16_t fraction);
int pwm_enable(struct pwm_dev *dev);
bool pwm_is_enabled(struct pwm_dev *pwm_d);
int pwm_set_frequency(struct pwm_dev *dev, uint32_t freq_hz);
int pwm_get_clock_freq(struct pwm_dev *dev);
int pwm_get_top_value(struct pwm_dev *dev);
int pwm_get_resolution_bits(struct pwm_dev *dev);
int pwm_disable(struct pwm_dev *dev);

#ifdef __cplusplus
}
#endif

#endif /* __PWM_H__ */
