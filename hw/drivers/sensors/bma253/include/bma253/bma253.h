/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * resarding copyright ownership.  The ASF licenses this file
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

#ifndef __BMA253_H__
#define __BMA253_H__

#include "os/os.h"
#include "os/os_dev.h"
#include "sensor/sensor.h"
#include "sensor/accel.h"

enum bma253_g_range {
	BMA253_G_RANGE_2  = 0,
	BMA253_G_RANGE_4  = 1,
	BMA253_G_RANGE_8  = 2,
	BMA253_G_RANGE_16 = 3,
};

enum bma253_filter_bandwidth {
	BMA253_FILTER_BANDWIDTH_7_81_HZ  = 0,
	BMA253_FILTER_BANDWIDTH_15_63_HZ = 1,
	BMA253_FILTER_BANDWIDTH_31_25_HZ = 2,
	BMA253_FILTER_BANDWIDTH_62_5_HZ  = 3,
	BMA253_FILTER_BANDWIDTH_125_HZ   = 4,
	BMA253_FILTER_BANDWIDTH_250_HZ   = 5,
	BMA253_FILTER_BANDWIDTH_500_HZ   = 6,
	BMA253_FILTER_BANDWIDTH_1000_HZ  = 7,
};

enum bma253_int_pin_output {
	BMA253_INT_PIN_OUTPUT_PUSH_PULL  = 0,
	BMA253_INT_PIN_OUTPUT_OPEN_DRAIN = 1,
};

enum bma253_int_pin_active {
	BMA253_INT_PIN_ACTIVE_LOW  = 0,
	BMA253_INT_PIN_ACTIVE_HIGH = 1,
};

enum bma253_tap_quiet {
	BMA253_TAP_QUIET_20_MS = 0,
	BMA253_TAP_QUIET_30_MS = 1,
};

enum bma253_tap_shock {
	BMA253_TAP_SHOCK_50_MS = 0,
	BMA253_TAP_SHOCK_75_MS = 1,
};

enum bma253_d_tap_window {
	BMA253_D_TAP_WINDOW_50_MS  = 0,
	BMA253_D_TAP_WINDOW_100_MS = 1,
	BMA253_D_TAP_WINDOW_150_MS = 2,
	BMA253_D_TAP_WINDOW_200_MS = 3,
	BMA253_D_TAP_WINDOW_250_MS = 4,
	BMA253_D_TAP_WINDOW_375_MS = 5,
	BMA253_D_TAP_WINDOW_500_MS = 6,
	BMA253_D_TAP_WINDOW_700_MS = 7,
};

enum bma253_tap_wake_samples {
	BMA253_TAP_WAKE_SAMPLES_2  = 0,
	BMA253_TAP_WAKE_SAMPLES_4  = 1,
	BMA253_TAP_WAKE_SAMPLES_8  = 2,
	BMA253_TAP_WAKE_SAMPLES_16 = 3,
};

enum bma253_i2c_watchdog {
	BMA253_I2C_WATCHDOG_DISABLED = 0,
	BMA253_I2C_WATCHDOG_1_MS     = 1,
	BMA253_I2C_WATCHDOG_50_MS    = 2,
};

enum bma253_power_mode {
	BMA253_POWER_MODE_NORMAL       = 0,
	BMA253_POWER_MODE_DEEP_SUSPEND = 1,
	BMA253_POWER_MODE_SUSPEND      = 2,
	BMA253_POWER_MODE_STANDBY      = 3,
	BMA253_POWER_MODE_LPM_1        = 4,
	BMA253_POWER_MODE_LPM_2        = 5,
};

enum bma253_sleep_duration {
	BMA253_SLEEP_DURATION_0_5_MS = 0,
	BMA253_SLEEP_DURATION_1_MS   = 1,
	BMA253_SLEEP_DURATION_2_MS   = 2,
	BMA253_SLEEP_DURATION_4_MS   = 3,
	BMA253_SLEEP_DURATION_6_MS   = 4,
	BMA253_SLEEP_DURATION_10_MS  = 5,
	BMA253_SLEEP_DURATION_25_MS  = 6,
	BMA253_SLEEP_DURATION_50_MS  = 7,
	BMA253_SLEEP_DURATION_100_MS = 8,
	BMA253_SLEEP_DURATION_500_MS = 9,
	BMA253_SLEEP_DURATION_1_S    = 10,
};

struct bma253_cfg {
	enum bma253_g_range g_range;
	enum bma253_filter_bandwidth filter_bandwidth;
	bool use_unfiltered_data;
	enum bma253_int_pin_output int_pin_output;
	enum bma253_int_pin_active int_pin_active;
	enum bma253_tap_quiet tap_quiet;
	enum bma253_tap_shock tap_shock;
	enum bma253_d_tap_window d_tap_window;
	enum bma253_tap_wake_samples tap_wake_samples;
	float tap_thresh_g;
	enum bma253_i2c_watchdog i2c_watchdog;
	float offset_x_g;
	float offset_y_g;
	float offset_z_g;
	enum bma253_power_mode power_mode;
	enum bma253_sleep_duration sleep_duration;
	int int_pin1_num;
	int int_pin2_num;
	sensor_type_t sensor_mask;
};

struct bma253_int {
	os_sr_t lock;
	struct os_sem wait;
	bool active;
	bool asleep;
	int pin;
	int pin_active;
};

enum bma253_int_pin {
	BMA253_INT_PIN_1   = 0,
	BMA253_INT_PIN_2   = 1,
	BMA253_INT_PIN_MAX = 2,
};

struct bma253 {
	struct os_dev dev;
	struct sensor sensor;
	struct bma253_cfg cfg;
	struct bma253_int ints[BMA253_INT_PIN_MAX];
};

enum bma253_offset_comp_target {
	BMA253_OFFSET_COMP_TARGET_0_G     = 0,
	BMA253_OFFSET_COMP_TARGET_NEG_1_G = 1,
	BMA253_OFFSET_COMP_TARGET_POS_1_G = 2,
};

enum bma253_tap_type {
	BMA253_TAP_TYPE_DOUBLE = 0,
	BMA253_TAP_TYPE_SINGLE = 1,
};

/**
 * Perform a self test of the device and report on its health.
 *
 * @param The device object.
 * @param Multiplier on the high amplitude self test minimums; 1.0 for default.
 * @param Multiplier on the low amplitude self test minimums; 0.0 for default.
 * @param The result of the self-test: false if passed, true if failed.
 *
 * @return 0 on success, non-zero on failure.
 */
int bma253_self_test(struct bma253 * bma253,
		     float delta_high_mult,
		     float delta_low_mult,
		     bool * self_test_fail);

/**
 * Perform an offset compensation and use the resulting offsets.
 *
 * @param The device object.
 * @param The correct target value for the X axis.
 * @param The correct target value for the Y axis.
 * @param The correct target value for the Z axis.
 *
 * @return 0 on success, non-zero on failure.
 */
int bma253_offset_compensation(struct bma253 * bma253,
			       enum bma253_offset_comp_target target_x,
			       enum bma253_offset_comp_target target_y,
			       enum bma253_offset_comp_target target_z);

/**
 * Callback for handling accelerometer sensor data.
 *
 * @param The opaque pointer passed in to the stream read function.
 * @param The accelerometer data provided by the sensor.
 *
 * @return true to stop streaming data, false to continue.
 */
typedef bool (*bma253_stream_read_func_t)(void *,
					  struct sensor_accel_data *);

/**
 * Provide a continuous stream of accelerometer readings.
 *
 * @param The device object.
 * @param The function pointer to invoke for each accelerometer reading.
 * @param The opaque pointer that will be passed in to the function.
 * @param If non-zero, how long the stream should run in milliseconds.
 *
 * @return 0 on success, non-zero on failure.
 */
int bma253_stream_read(struct bma253 * bma253,
		       bma253_stream_read_func_t read_func,
		       void * read_arg,
		       uint32_t time_ms);

/**
 * Block until a single or double tap event occurs.
 *
 * @param The device object.
 * @param The type of tap event to look for.
 *
 * @return 0 on success, non-zero on failure.
 */
int bma253_wait_for_tap(struct bma253 * bma253,
			enum bma253_tap_type tap_type);

/**
 * Configure the sensor.
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 *
 * @return 0 on success, non-zero on failure.
 */
int bma253_config(struct bma253 * bma253, struct bma253_cfg * cfg);

/**
 * Expects to be called back through os_dev_create().
 *
 * @param ptr to the device object associated with this accelerometer
 * @param argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int bma253_init(struct os_dev * dev, void * arg);

#endif
