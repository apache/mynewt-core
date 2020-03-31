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

#ifndef __LIS2DS12_H__
#define __LIS2DS12_H__

#include <os/os.h>
#include "os/os_dev.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LIS2DS12_DATA_RATE_OFF                  0x00
#define LIS2DS12_DATA_RATE_LP_10BIT_1HZ         0x80
#define LIS2DS12_DATA_RATE_LP_10BIT_12_5HZ      0x90
#define LIS2DS12_DATA_RATE_LP_10BIT_25HZ        0xA0
#define LIS2DS12_DATA_RATE_LP_10BIT_50HZ        0xB0
#define LIS2DS12_DATA_RATE_LP_10BIT_100HZ       0xC0
#define LIS2DS12_DATA_RATE_LP_10BIT_200HZ       0xD0
#define LIS2DS12_DATA_RATE_LP_10BIT_400HZ       0xE0
#define LIS2DS12_DATA_RATE_LP_10BIT_800HZ       0x70
#define LIS2DS12_DATA_RATE_HR_14BIT_12_5HZ      0x10
#define LIS2DS12_DATA_RATE_HR_14BIT_25HZ        0x20
#define LIS2DS12_DATA_RATE_HR_14BIT_50HZ        0x30
#define LIS2DS12_DATA_RATE_HR_14BIT_100HZ       0x40
#define LIS2DS12_DATA_RATE_HR_14BIT_200HZ       0x50
#define LIS2DS12_DATA_RATE_HR_14BIT_400HZ       0x60
#define LIS2DS12_DATA_RATE_HR_14BIT_800HZ       0x70
#define LIS2DS12_DATA_RATE_HR_12BIT_1600HZ      0x52
#define LIS2DS12_DATA_RATE_HR_12BIT_3200HZ      0x62
#define LIS2DS12_DATA_RATE_HR_12BIT_6400HZ      0x72

#define LIS2DS12_ST_MODE_DISABLE                0x00
#define LIS2DS12_ST_MODE_MODE1                  0x40
#define LIS2DS12_ST_MODE_MODE2                  0x80

#define LIS2DS12_FS_2G                          0x00
#define LIS2DS12_FS_4G                          0x08
#define LIS2DS12_FS_8G                          0x0C
#define LIS2DS12_FS_16G                         0x04

#define LIS2DS12_INT1_CFG_DRDY                  0x01
#define LIS2DS12_INT1_CFG_FTH                   0x02
#define LIS2DS12_INT1_CFG_6D                    0x04
#define LIS2DS12_INT1_CFG_DOUBLE_TAP            0x08
#define LIS2DS12_INT1_CFG_FF                    0x10
#define LIS2DS12_INT1_CFG_WU                    0x20
#define LIS2DS12_INT1_CFG_SINGLE_TAP            0x40
#define LIS2DS12_INT1_CFG_MASTER_DRDY           0x80

#define LIS2DS12_INT2_CFG_DRDY                  0x01
#define LIS2DS12_INT2_CFG_FTH                   0x02
#define LIS2DS12_INT2_CFG_STEP_DET              0x04
#define LIS2DS12_INT2_CFG_SIG_MOT               0x08
#define LIS2DS12_INT2_CFG_TILT                  0x10
#define LIS2DS12_INT2_CFG_ON_INT1               0x20
#define LIS2DS12_INT2_CFG_BOOT                  0x40
#define LIS2DS12_INT2_CFG_PULSED                0x80

#define LIS2DS12_STATUS_DRDY                    0x01
#define LIS2DS12_STATUS_FF_IA                   0x02
#define LIS2DS12_STATUS_6D_IA                   0x04
#define LIS2DS12_STATUS_STAP                    0x08
#define LIS2DS12_STATUS_DTAP                    0x10
#define LIS2DS12_STATUS_SLEEP_STATE             0x20
#define LIS2DS12_STATUS_WU_IA                   0x40
#define LIS2DS12_STATUS_FIFO_THS                0x80

#define LIS2DS12_WAKE_UP_SRC_FF_IA              0x20
#define LIS2DS12_WAKE_UP_SRC_SLEEP_STATE_IA     0x10
#define LIS2DS12_WAKE_UP_SRC_WU_IA              0x08
#define LIS2DS12_WAKE_UP_SRC_X_WU               0x04
#define LIS2DS12_WAKE_UP_SRC_Y_WU               0x02
#define LIS2DS12_WAKE_UP_SRC_Z_WU               0x01

#define LIS2DS12_TAP_SRC_TAP_IA                 0x40
#define LIS2DS12_TAP_SRC_SINGLE_TAP             0x20
#define LIS2DS12_TAP_SRC_DOUBLE_TAP             0x10
#define LIS2DS12_TAP_SRC_TAP_SIGN               0x08
#define LIS2DS12_TAP_SRC_X_TAP                  0x04
#define LIS2DS12_TAP_SRC_Y_TAP                  0x02
#define LIS2DS12_TAP_SRC_Z_TAP                  0x01

#define LIS2DS12_SIXD_SRC_6D_IA                 0x40
#define LIS2DS12_SIXD_SRC_ZH                    0x20
#define LIS2DS12_SIXD_SRC_ZL                    0x10
#define LIS2DS12_SIXD_SRC_YH                    0x08
#define LIS2DS12_SIXD_SRC_YL                    0x04
#define LIS2DS12_SIXD_SRC_XH                    0x02
#define LIS2DS12_SIXD_SRC_XL                    0x01
#define LIS2DS12_ST_MAX                         1500
#define LIS2DS12_ST_MIN                         70

enum lis2ds12_ths_6d {
    LIS2DS12_6D_THS_80_DEG = 0,
    LIS2DS12_6D_THS_70_DEG = 1,
    LIS2DS12_6D_THS_60_DEG = 2,
    LIS2DS12_6D_THS_50_DEG = 3
};

enum lis2ds12_fifo_mode {
    LIS2DS12_FIFO_M_BYPASS               = 0,
    LIS2DS12_FIFO_M_FIFO                 = 1,
    LIS2DS12_FIFO_M_CONTINUOUS_TO_FIFO   = 3,
    LIS2DS12_FIFO_M_BYPASS_TO_CONTINUOUS = 4,
    LIS2DS12_FIFO_M_CONTINUOUS           = 6
};
    
enum lis2ds12_read_mode {
    LIS2DS12_READ_M_POLL = 0,
    LIS2DS12_READ_M_STREAM = 1,
};

struct lis2ds12_notif_cfg {
    sensor_event_type_t event;
    uint8_t int_num:1;
    uint8_t int_cfg;
};

struct lis2ds12_tap_settings {
    uint8_t en_x  : 1;
    uint8_t en_y  : 1;
    uint8_t en_z  : 1;
    uint8_t en_4d : 1;
    enum lis2ds12_ths_6d ths_6d;

    /* Threshold for tap recognition
     * 5 bits, 1 LSB = FS/32
     */
    int8_t tap_ths;

    /*
     * Time between taps in double tap
     * 4 bits, 0 = 16/ODR, otherwise 1 LSB corresponds to 32/ODR
     */
    uint8_t latency;

    /*
     * Time after tap data is just below threshold
     * 2 bits, 0 = 2/ODR, otherwise 1 LSB corresponds to 4/ODR
     */
    uint8_t quiet;

    /*
     * Maximum time data can be over threshold to register as tap
     * 2 bits, 0 = 4/ODR, otherwise 1 LSB corresponds to 8/ODR
     */
    uint8_t shock;
};

/* Read mode configuration */
struct lis2ds12_read_mode_cfg {
    enum lis2ds12_read_mode mode;
    uint8_t int_num:1;
    uint8_t int_cfg;
};

struct lis2ds12_cfg {
    uint8_t rate;
    uint8_t fs;

    /* Tap config */
    struct lis2ds12_tap_settings tap;

    /* Read mode config */
    struct lis2ds12_read_mode_cfg read_mode;

    /* Notif config */
    struct lis2ds12_notif_cfg *notif_cfg;
    uint8_t max_num_notif;

    /* Freefall config */

    /* The minimum duration before freefall event to be recognized
     * 6 bits, 1 LSB = 1/ODR
     */
    uint8_t freefall_dur;

    /* Threshold for freefall event triggering
     * data is 3 bits
     * 0 =  5 * 31.25mg = 156.25mg
     * 1 =  7 * 31.25mg = 218.75mg
     * 2 =  8 * 31.25mg = 250mg
     * 3 = 10 * 31.25mg = 312.5mg
     * 4 = 11 * 31.25mg = 343.75mg
     * 5 = 13 * 31.25mg = 406.25mg
     * 6 = 15 * 31.25mg = 468.75mg
     * 7 = 16 * 31.25mg = 500mg
     */
    uint8_t freefall_ths;

    /* interrupt config */
    uint8_t int1_pin_cfg;
    uint8_t int2_pin_cfg;
    bool map_int2_to_int1;

    uint8_t high_pass : 1;

    uint8_t int_pp_od : 1;
    uint8_t int_latched : 1;
    uint8_t int_active_low : 1;
    uint8_t inactivity_sleep_enable : 1;
    uint8_t double_tap_event_enable : 1;

    /* fifo config */
    enum lis2ds12_fifo_mode fifo_mode;

    /* Defines the fifo high watermark level
     * number of FIFO samples (xyz = 1) at which point the FTH bit is set
     * 8 bits
     */
    uint8_t fifo_threshold;

    /* sleep/wakeup settings */
    /* Threshold (positive or negative) on at least one axis before triggering a wake up event
     * 6 bits unsigned, 1 LSB = FS/64
     */
    uint8_t wake_up_ths;

    /* The minimum duration of the wake-up event to be recognized
     * 2 bit, 1 LSB = 1 / ODR
     */
    uint8_t wake_up_dur;

    /* The minimum duration of the inactivity status to be recognized
     * 4 bits, 1 LSB = 512 / ODR
     */
    uint8_t sleep_duration;
    
    /* Sensor type mask to track enabled sensors */
    sensor_type_t mask;
};

/* Used to track interrupt state to wake any present waiters */
struct lis2ds12_int {
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

/* Private per driver data */
struct lis2ds12_pdd {
    /* Notification event context */
    struct sensor_notify_ev_ctx notify_ctx;
     /* Inetrrupt state */
    struct lis2ds12_int *interrupt;
    /* Interrupt enabled flag */
    uint16_t int_enable;
};
        
    
struct lis2ds12 {
    struct os_dev dev;
    struct sensor sensor;
    struct lis2ds12_cfg cfg;
    struct lis2ds12_int intr;

    struct lis2ds12_pdd pdd;
};

/**
 * Reset lis2ds12
 *
 * @param itf The sensor interface
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_reset(struct sensor_itf *itf);

/**
 * Get chip ID
 *
 * @param itf The sensor interface
 * @param chip_id Ptr to chip id to be filled up
 */
int lis2ds12_get_chip_id(struct sensor_itf *itf, uint8_t *chip_id);
    
/**
 * Sets the full scale selection
 *
 * @param itf The sensor interface
 * @param fs The full scale value to set
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_full_scale(struct sensor_itf *itf, uint8_t fs);

/**
 * Gets the full scale selection
 *
 * @param itf The sensor interface
 * @param fs Ptr to full scale read from the sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_full_scale(struct sensor_itf *itf, uint8_t *fs);

/**
 * Sets the rate
 *
 * @param itf The sensor interface
 * @param rate The sampling rate of the sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_rate(struct sensor_itf *itf, uint8_t rate);

/**
 * Gets the current rate
 *
 * @param itf The sensor interface
 * @param rate Ptr to rate read from the sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_rate(struct sensor_itf *itf, uint8_t *rate);

/**
 * Sets the self test mode of the sensor
 *
 * @param itf The sensor interface
 * @param mode Self test mode
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_self_test(struct sensor_itf *itf, uint8_t mode);

/**
 * Gets the power mode of the sensor
 *
 * @param itf The sensor interface
 * @param mode Ptr to self test mode read from sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_self_test(struct sensor_itf *itf, uint8_t *mode);

/**
 * Sets the interrupt push-pull/open-drain selection
 *
 * @param itf The sensor interface
 * @param mode Interrupt setting (0 = push-pull, 1 = open-drain)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_int_pp_od(struct sensor_itf *itf, uint8_t mode);

/**
 * Gets the interrupt push-pull/open-drain selection
 *
 * @param itf The sensor interface
 * @param mode Ptr to store setting (0 = push-pull, 1 = open-drain)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_int_pp_od(struct sensor_itf *itf, uint8_t *mode);

/**
 * Sets whether latched interrupts are enabled
 *
 * @param itf The sensor interface
 * @param en Value to set (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_latched_int(struct sensor_itf *itf, uint8_t en);

/**
 * Gets whether latched interrupts are enabled
 *
 * @param itf The sensor interface
 * @param en Ptr to store value (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_latched_int(struct sensor_itf *itf, uint8_t *en);

/**
 * Sets whether interrupts are active high or low
 *
 * @param itf The sensor interface
 * @param low Value to set (0 = active high, 1 = active low)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2sw12_set_int_active_low(struct sensor_itf *itf, uint8_t low);

/**
 * Gets whether interrupts are active high or low
 *
 * @param itf The sensor interface
 * @param low Ptr to store value (0 = active high, 1 = active low)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_int_active_low(struct sensor_itf *itf, uint8_t *low);

/**
 * Set filter config
 *
 * @param itf The sensor interface
 * @param type The filter type (1 = high pass, 0 = low pass)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_filter_cfg(struct sensor_itf *itf, uint8_t type);

/**
 * Get filter config
 *
 * @param itf The sensor interface
 * @param type Ptr to filter type (1 = high pass, 0 = low pass)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_filter_cfg(struct sensor_itf *itf, uint8_t *type);

/**
 * Set tap detection configuration
 *
 * @param itf The sensor interface
 * @param cfg The tap settings config
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_tap_cfg(struct sensor_itf *itf, struct lis2ds12_tap_settings *cfg);

/**
 * Get tap detection config
 *
 * @param itf The sensor interface
 * @param cfg Ptr to the tap settings
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_tap_cfg(struct sensor_itf *itf, struct lis2ds12_tap_settings *cfg);

/**
 * Set freefall detection configuration
 *
 * @param itf The sensor interface
 * @param dur Freefall duration (6 bits LSB = 1/ODR)
 * @param ths Freefall threshold (3 bits)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_freefall(struct sensor_itf *itf, uint8_t dur, uint8_t ths);

/**
 * Get freefall detection config
 *
 * @param itf The sensor interface
 * @param dur Ptr to freefall duration
 * @param ths Ptr to freefall threshold
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_freefall(struct sensor_itf *itf, uint8_t *dur, uint8_t *ths);
    
/**
 * Set interrupt pin configuration for interrupt 1
 *
 * @param itf The sensor interface
 * @param cfg int1 config
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2ds12_set_int1_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set interrupt pin configuration for interrupt 2
 *
 * @param itf The sensor interface
 * @param cfg int2 config
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_int2_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Clear interrupt pin configuration for interrupt 1
 *
 * @param itf The sensor interface
 * @param cfg int1 config
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2ds12_clear_int1_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Clear interrupt pin configuration for interrupt 2
 *
 * @param itf The sensor interface
 * @param cfg int2 config
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2ds12_clear_int2_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set whether interrupts are enabled
 *
 * @param itf The sensor interface
 * @param enabled Value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 *
 */
int lis2ds12_set_int_enable(struct sensor_itf *itf, uint8_t enabled);

/**
 * Clear all interrupts by reading all four interrupt registers status
 *
 * @param itf The sensor interface
 * @param src Ptr to return 4 interrupt sources in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_clear_int(struct sensor_itf *itf, uint8_t *int_src);

/**
 * Get Interrupt Status
 *
 * @param itf The sensor interface
 * @param status Ptr to return interrupt status in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_int_status(struct sensor_itf *itf, uint8_t *status);

/**
 * Get Wake Up Source
 *
 * @param itf The sensor interface
 * @param Ptr to return wake_up_src in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_wake_up_src(struct sensor_itf *itf, uint8_t *status);

/**
 * Get Tap Source
 *
 * @param itf The sensor interface
 * @param Ptr to return tap_src in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_tap_src(struct sensor_itf *itf, uint8_t *status);

/**
 * Get 6D Source
 *
 * @param itf The sensor interface
 * @param Ptr to return sixd_src in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_sixd_src(struct sensor_itf *itf, uint8_t *status);

/**
 * Setup FIFO
 *
 * @param itf The sensor interface
 * @param mode FIFO mode to setup
 * @param fifo_ths Threshold to set for FIFO
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_fifo_cfg(struct sensor_itf *itf, enum lis2ds12_fifo_mode mode, uint8_t fifo_ths);

/**
 * Get Number of Samples in FIFO
 *
 * @param itf The sensor interface
 * @param samples Ptr to return number of samples in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_fifo_samples(struct sensor_itf *itf, uint16_t *samples);

/**
 * Set Wake Up Threshold configuration
 *
 * @param itf The sensor interface
 * @param reg wake_up_ths value to set
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_wake_up_ths(struct sensor_itf *itf, uint8_t reg);

/**
 * Get Wake Up Threshold config
 *
 * @param itf The sensor interface
 * @param reg Ptr to store wake_up_ths value
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_wake_up_ths(struct sensor_itf *itf, uint8_t *reg);

/**
 * Set whether sleep on inactivity is enabled
 *
 * @param itf The sensor interface
 * @param en Value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_inactivity_sleep_en(struct sensor_itf *itf, uint8_t en);

/**
 * Get whether sleep on inactivity is enabled
 *
 * @param itf The sensor interface
 * @param en Ptr to store read state (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_inactivity_sleep_en(struct sensor_itf *itf, uint8_t *en);

/**
 * Set whether double tap event is enabled
 *
 * @param itf The sensor interface
 * @param en Value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_double_tap_event_en(struct sensor_itf *itf, uint8_t en);

/**
 * Get whether double tap event is enabled
 *
 * @param itf The sensor interface
 * @param Ptr to store read state (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_double_tap_event_en(struct sensor_itf *itf, uint8_t *en);

/**
 * Set Wake Up Duration
 *
 * @param itf The sensor interface
 * @param reg duration to set
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_wake_up_dur(struct sensor_itf *itf, uint8_t reg);

/**
 * Get Wake Up Duration
 *
 * @param itf The sensor interface
 * @param reg Ptr to store wake_up_dur value
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_wake_up_dur(struct sensor_itf *itf, uint8_t *reg);

/**
 * Set Sleep Duration
 *
 * @param itf The sensor interface
 * @param reg Duration to set
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_set_sleep_dur(struct sensor_itf *itf, uint8_t reg);

/**
 * Get Sleep Duration
 *
 * @param itf The sensor interface
 * @param reg Ptr to store sleep_dur value
 *
 * @return 0 on success, non-zero on failure
 */
int lis2ds12_get_sleep_dur(struct sensor_itf *itf, uint8_t *reg);

/**
* Set whether interrupt 2 signals is mapped onto interrupt 1 pin
*
* @param itf The sensor interface
* @param enable Value to set (0 = disabled, 1 = enabled
*
* @return 0 on success, non-zero on failure
*/
int lis2ds12_set_int2_on_int1_map(struct sensor_itf *itf, bool enable);

/**
* Get whether interrupt 2 signals is mapped onto interrupt 1 pin
*
* @param itf The sensor interface
* @param val Value to set (0 = disabled, 1 = enabled)
*
* @return 0 on success, non-zero on failure
*/
int lis2ds12_get_int2_on_int1_map(struct sensor_itf *itf, uint8_t *val);

/**
 * Run Self test on sensor
 *
 * @param itf The sensor interface
 * @param result Ptr to return test result in (0 on pass, non-zero on failure)
 *
 * @return 0 on sucess, non-zero on failure
 */
int lis2ds12_run_self_test(struct sensor_itf *itf, int *result);

/**
 * Provide a continuous stream of accelerometer readings.
 *
 * @param sensor The sensor ptr
 * @param type The sensor type
 * @param read_func The function pointer to invoke for each accelerometer reading.
 * @param read_arg The opaque pointer that will be passed in to the function.
 * @param time_ms If non-zero, how long the stream should run in milliseconds.
 *
 * @return 0 on success, non-zero on failure.
 */
int lis2ds12_stream_read(struct sensor *sensor,
                         sensor_type_t sensor_type,
                         sensor_data_func_t read_func,
                         void *read_arg,
                         uint32_t time_ms);

/**
 * Do accelerometer polling reads
 *
 * @param sensor The sensor ptr
 * @param sensor_type The sensor type
 * @param data_func The function pointer to invoke for each accelerometer reading.
 * @param data_arg The opaque pointer that will be passed in to the function.
 * @param timeout If non-zero, how long the stream should run in milliseconds.
 *
 * @return 0 on success, non-zero on failure.
 */
int lis2ds12_poll_read(struct sensor *sensor,
                       sensor_type_t sensor_type,
                       sensor_data_func_t data_func,
                       void *data_arg,
                       uint32_t timeout);

/**
 * Expects to be called back through os_dev_create().
 *
 * @param dev Ptr to the device object associated with this accelerometer
 * @param arg Argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int lis2ds12_init(struct os_dev *dev, void *arg);

/**
 * Configure the sensor
 *
 * @param lis2ds12 Ptr to sensor driver
 * @param cfg Ptr to sensor driver config
 */
int lis2ds12_config(struct lis2ds12 *lis2ds12, struct lis2ds12_cfg *cfg);

#if MYNEWT_VAL(LIS2DS12_CLI)
int lis2ds12_shell_init(void);
#endif

    
#ifdef __cplusplus
}
#endif

#endif /* __LIS2DS12_H__ */
