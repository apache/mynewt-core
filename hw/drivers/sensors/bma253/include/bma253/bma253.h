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
#include "sensor/temperature.h"

#ifdef __cplusplus
#extern "C" {
#endif

/* XXX use some better defaults. For now it is min */
#define BMA253_LOW_G_DELAY_MS_DEFAULT       2
#define BMA253_HIGH_G_DELAY_MS_DEFAULT      2

/* Range of acceleration measurements */
enum bma253_g_range {
    BMA253_G_RANGE_2  = 0,
    BMA253_G_RANGE_4  = 1,
    BMA253_G_RANGE_8  = 2,
    BMA253_G_RANGE_16 = 3,
};

/* How often acceleration measurements are taken */
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

/* Quiet time after a double/single tap */
enum bma253_tap_quiet {
    BMA253_TAP_QUIET_20_MS = 0,
    BMA253_TAP_QUIET_30_MS = 1,
};

/* Settling time after a double/single tap */
enum bma253_tap_shock {
    BMA253_TAP_SHOCK_50_MS = 0,
    BMA253_TAP_SHOCK_75_MS = 1,
};

/* How long to wait for the next tap in a double tap scenario */
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

/* How many samples to use after a wake up from a low power mode to determine
 * whether a tap occurred */
enum bma253_tap_wake_samples {
    BMA253_TAP_WAKE_SAMPLES_2  = 0,
    BMA253_TAP_WAKE_SAMPLES_4  = 1,
    BMA253_TAP_WAKE_SAMPLES_8  = 2,
    BMA253_TAP_WAKE_SAMPLES_16 = 3,
};

/* Block generation of orientation events based on given criteria */
enum bma253_orient_blocking {
    BMA253_ORIENT_BLOCKING_NONE                       = 0,
    BMA253_ORIENT_BLOCKING_ACCEL_ONLY                 = 1,
    BMA253_ORIENT_BLOCKING_ACCEL_AND_SLOPE            = 2,
    BMA253_ORIENT_BLOCKING_ACCEL_AND_SLOPE_AND_STABLE = 3,
};

/* Orientation mode configuration, used to determine thresholds for
 * transitions between different orientations */
enum bma253_orient_mode {
    BMA253_ORIENT_MODE_SYMMETRICAL       = 0,
    BMA253_ORIENT_MODE_HIGH_ASYMMETRICAL = 1,
    BMA253_ORIENT_MODE_LOW_ASYMMETRICAL  = 2,
};

/* Power mode for the device */
enum bma253_power_mode {
    BMA253_POWER_MODE_NORMAL       = 0,
    BMA253_POWER_MODE_DEEP_SUSPEND = 1,
    BMA253_POWER_MODE_SUSPEND      = 2,
    BMA253_POWER_MODE_STANDBY      = 3,
    BMA253_POWER_MODE_LPM_1        = 4,
    BMA253_POWER_MODE_LPM_2        = 5,
};

/* Duration of sleep whenever the device is in a power mode that alternates
 * between wake and sleep (LPM 1 & 2) */
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

/* Default configuration values to use with the device */
struct bma253_cfg {
    /* Accelerometer configuration */
    enum bma253_g_range g_range;
    enum bma253_filter_bandwidth filter_bandwidth;
    /* Whether to use data that has not been filtered at all */
    bool use_unfiltered_data;
    /* Low-g event configuration */
    uint16_t low_g_delay_ms;
    float low_g_thresh_g;
    float low_g_hyster_g;
    /* High-g event configuration */
    float high_g_hyster_g;
    uint16_t high_g_delay_ms;
    float high_g_thresh_g;
    /* Tap (double & single) event configuration */
    enum bma253_tap_quiet tap_quiet;
    enum bma253_tap_shock tap_shock;
    enum bma253_d_tap_window d_tap_window;
    enum bma253_tap_wake_samples tap_wake_samples;
    float tap_thresh_g;
    /* Orientation event configuration */
    float orient_hyster_g;
    enum bma253_orient_blocking orient_blocking;
    enum bma253_orient_mode orient_mode;
    bool orient_signal_ud;
    /* Offsets for acceleration measurements, by axis */
    float offset_x_g;
    float offset_y_g;
    float offset_z_g;
    /* Power management configuration */
    enum bma253_power_mode power_mode;
    enum bma253_sleep_duration sleep_duration;
    /* Applicable sensor types supported */
    sensor_type_t sensor_mask;
};

/* Used to track interrupt state to wake any present waiters */
struct bma253_int {
    /* Synchronize access to this structure */
    os_sr_t lock;
    /* Sleep waiting for an interrupt to occur */
    struct os_sem wait;
    /* Is the interrupt currently active */
    bool active;
    /* Is there a waiter currently sleeping */
    bool asleep;
    /* Configured interrupts */
    struct sensor_int *ints;
};

/* Device private data */
struct bma253_private_driver_data {
    struct bma253_int * interrupt;
    struct sensor_notify_ev_ctx notify_ctx;
    struct sensor_read_ev_ctx read_ctx;
    uint8_t registered_mask;

    uint8_t int_num;
    uint8_t int_route;
    uint8_t int_ref_cnt;
};

/* The device itself */
struct bma253 {
    /* Underlying OS device */
    struct os_dev dev;
    /* The sensor infrastructure */
    struct sensor sensor;
    /* Default configuration values */
    struct bma253_cfg cfg;
    /* Active interrupt state */
    struct bma253_int intr;
    /* Active power mode, could be different from default configured
     * power mode if a function that requires a higher power mode is
     * currently running. */
    enum bma253_power_mode power;

    /* Private driver data */
    struct bma253_private_driver_data pdd;
};

/* Offset compensation is performed to target this given value, by axis */
enum bma253_offset_comp_target {
    BMA253_OFFSET_COMP_TARGET_0_G     = 0,
    BMA253_OFFSET_COMP_TARGET_NEG_1_G = 1,
    BMA253_OFFSET_COMP_TARGET_POS_1_G = 2,
};

/* The device's X/Y orientation, in form of rotation */
enum bma253_orient_xy {
    BMA253_ORIENT_XY_PORTRAIT_UPRIGHT     = 0,
    BMA253_ORIENT_XY_PORTRAIT_UPSIDE_DOWN = 1,
    BMA253_ORIENT_XY_LANDSCAPE_LEFT       = 2,
    BMA253_ORIENT_XY_LANDSCAPE_RIGHT      = 3,
};

/* The device's full orientation */
struct bma253_orient_xyz {
    /* The X/Y orientation */
    enum bma253_orient_xy orient_xy;
    /* Is device facing upward or downward */
    bool downward_z;
};

/* Type of tap event to look for */
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
int
bma253_self_test(struct bma253 * bma253,
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
int
bma253_offset_compensation(struct bma253 * bma253,
                           enum bma253_offset_comp_target target_x,
                           enum bma253_offset_comp_target target_y,
                           enum bma253_offset_comp_target target_z);

/**
 * Return the current compensation offsets in use.
 *
 * @param The device object.
 * @param The offset for the X axis, filled in by this function.
 * @param The offset for the Y axis, filled in by this function.
 * @param The offset for the Z axis, filled in by this function.
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma253_query_offsets(struct bma253 * bma253,
                     float * offset_x_g,
                     float * offset_y_g,
                     float * offset_z_g);

/**
 * Store and use the provided compensation offsets.
 *
 * @param The device object.
 * @param The offset for the X axis.
 * @param The offset for the Y axis.
 * @param the offset for the Z axis.
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma253_write_offsets(struct bma253 * bma253,
                     float offset_x_g,
                     float offset_y_g,
                     float offset_z_g);

/**
 * Callback for handling accelerometer sensor data.
 *
 * @param The opaque pointer passed in to the stream read function.
 * @param The accelerometer data provided by the sensor.
 *
 * @return true to stop streaming data, false to continue.
 */
typedef bool
(*bma253_stream_read_func_t)(void *,
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
int
bma253_stream_read(struct bma253 * bma253,
                   bma253_stream_read_func_t read_func,
                   void * read_arg,
                   uint32_t time_ms);

/**
 * Get the current temperature at the device.
 *
 * @param The device object.
 * @param The current temperature in celsius, filled in by this function.
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma253_current_temp(struct bma253 * bma253,
                    float * temp_c);

/**
 * Get the current device orientation.
 *
 * @param The device object.
 * @param The current orientation, filled in by this function.
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma253_current_orient(struct bma253 * bma253,
                      struct bma253_orient_xyz * orient_xyz);

/**
 * Block until the device orientation changes.
 *
 * @param The device object.
 * @param The new orientation, filled in by this function.
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma253_wait_for_orient(struct bma253 * bma253,
                       struct bma253_orient_xyz * orient_xyz);

/**
 * Block until a high-g event occurs.
 *
 * @param The device object.
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma253_wait_for_high_g(struct bma253 * bma253);

/**
 * Block until a low-g event occurs.
 *
 * @param The device object.
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma253_wait_for_low_g(struct bma253 * bma253);

/**
 * Block until a single or double tap event occurs.
 *
 * @param The device object.
 * @param The type of tap event to look for.
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma253_wait_for_tap(struct bma253 * bma253,
                    enum bma253_tap_type tap_type);

/**
 * Change the default power settings.
 *
 * @param The device object.
 * @param The new power mode, which the device resides in unless a different
 *        and more aggressive power mode is required to satisfy a running API
 *        function.
 * @param The new sleep duration, used whenever device is in LPM_1/2 mode
 *        either due to default power mode or due to being in a temporarily
 *        overridden power mode.
 */
int
bma253_power_settings(struct bma253 * bma253,
                      enum bma253_power_mode power_mode,
                      enum bma253_sleep_duration sleep_duration);

/**
 * Configure the sensor.
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma253_config(struct bma253 * bma253, struct bma253_cfg * cfg);

/**
 * Expects to be called back through os_dev_create().
 *
 * @param ptr to the device object associated with this accelerometer
 * @param argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma253_init(struct os_dev * dev, void * arg);

#if MYNEWT_VAL(BMA253_CLI)
/**
 * Initialize the BMA253 shell extensions.
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma253_shell_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
