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

#ifndef __LIS2DW12_H__
#define __LIS2DW12_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LIS2DW12_PM_LP_MODE1                    0x00
#define LIS2DW12_PM_LP_MODE2                    0x01
#define LIS2DW12_PM_LP_MODE3                    0x02
#define LIS2DW12_PM_LP_MODE4                    0x03
#define LIS2DW12_PM_HIGH_PERF                   0x04
#define LIS2DW12_PM_ON_DEMAND                   0x08

#define LIS2DW12_DATA_RATE_0HZ                  0x00
#define LIS2DW12_DATA_RATE_1_6HZ                0x10
#define LIS2DW12_DATA_RATE_12_5HZ               0x20
#define LIS2DW12_DATA_RATE_25HZ                 0x30
#define LIS2DW12_DATA_RATE_50HZ                 0x40
#define LIS2DW12_DATA_RATE_100HZ                0x50
#define LIS2DW12_DATA_RATE_200HZ                0x60
#define LIS2DW12_DATA_RATE_400HZ                0x70
#define LIS2DW12_DATA_RATE_800HZ                0x80
#define LIS2DW12_DATA_RATE_1600HZ               0x90

#define LIS2DW12_ST_MODE_DISABLE                0x00
#define LIS2DW12_ST_MODE_MODE1                  0x40
#define LIS2DW12_ST_MODE_MODE2                  0x80

#define LIS2DW12_FILTER_BW_ODR_DIV_2            0x00
#define LIS2DW12_FILTER_BW_ODR_DIV_4            0x01
#define LIS2DW12_FILTER_BW_ODR_DIV_10           0x02
#define LIS2DW12_FILTER_BW_ODR_DIV_20           0x03

#define LIS2DW12_FS_2G                          0x00
#define LIS2DW12_FS_4G                          0x10
#define LIS2DW12_FS_8G                          0x20
#define LIS2DW12_FS_16G                         0x30

#define LIS2DW12_INT1_CFG_DRDY                  0x01
#define LIS2DW12_INT1_CFG_FTH                   0x02
#define LIS2DW12_INT1_CFG_DIFF5                 0x04
#define LIS2DW12_INT1_CFG_DOUBLE_TAP            0x08
#define LIS2DW12_INT1_CFG_FF                    0x10
#define LIS2DW12_INT1_CFG_WU                    0x20
#define LIS2DW12_INT1_CFG_SINGLE_TAP            0x40
#define LIS2DW12_INT1_CFG_6D                    0x80

#define LIS2DW12_INT2_CFG_DRDY                  0x01
#define LIS2DW12_INT2_CFG_FTH                   0x02
#define LIS2DW12_INT2_CFG_DIFF5                 0x04
#define LIS2DW12_INT2_CFG_OVR                   0x08
#define LIS2DW12_INT2_CFG_DRDY_T                0x10
#define LIS2DW12_INT2_CFG_BOOT                  0x20
#define LIS2DW12_INT2_CFG_SLEEP_CHG             0x40
#define LIS2DW12_INT2_CFG_SLEEP_STATE           0x80

#define LIS2DW12_INT_SRC_SLP_CHG                0x20
#define LIS2DW12_INT_SRC_6D_IA                  0x10
#define LIS2DW12_INT_SRC_DTAP                   0x08
#define LIS2DW12_INT_SRC_STAP                   0x04
#define LIS2DW12_INT_SRC_WU_IA                  0x02
#define LIS2DW12_INT_SRC_FF_IA                  0x01

#define LIS2DW12_WAKE_UP_SRC_FF_IA              0x20
#define LIS2DW12_WAKE_UP_SRC_SLEEP_STATE_IA     0x10
#define LIS2DW12_WAKE_UP_SRC_WU_IA              0x08
#define LIS2DW12_WAKE_UP_SRC_X_WU               0x04
#define LIS2DW12_WAKE_UP_SRC_Y_WU               0x02
#define LIS2DW12_WAKE_UP_SRC_Z_WU               0x01

#define LIS2DW12_TAP_SRC_TAP_IA                 0x40
#define LIS2DW12_TAP_SRC_SINGLE_TAP             0x20
#define LIS2DW12_TAP_SRC_DOUBLE_TAP             0x10
#define LIS2DW12_TAP_SRC_TAP_SIGN               0x08
#define LIS2DW12_TAP_SRC_X_TAP                  0x04
#define LIS2DW12_TAP_SRC_Y_TAP                  0x02
#define LIS2DW12_TAP_SRC_Z_TAP                  0x01

#define LIS2DW12_SIXD_SRC_6D_IA                 0x40
#define LIS2DW12_SIXD_SRC_ZH                    0x20
#define LIS2DW12_SIXD_SRC_ZL                    0x10
#define LIS2DW12_SIXD_SRC_YH                    0x08
#define LIS2DW12_SIXD_SRC_YL                    0x04
#define LIS2DW12_SIXD_SRC_XH                    0x02
#define LIS2DW12_SIXD_SRC_XL                    0x01
#define LIS2DW12_ST_MAX                         1500
#define LIS2DW12_ST_MIN                         70

enum lis2dw12_ths_6d {
    LIS2DW12_6D_THS_80_DEG = 0,
    LIS2DW12_6D_THS_70_DEG = 1,
    LIS2DW12_6D_THS_60_DEG = 2,
    LIS2DW12_6D_THS_50_DEG = 3
};

enum lis2dw12_tap_priority {
    LIS2DW12_TAP_PRIOR_XYZ = 0,
    LIS2DW12_TAP_PRIOR_YXZ = 1,
    LIS2DW12_TAP_PRIOR_XZY = 2,
    LIS2DW12_TAP_PRIOR_ZYX = 3,
    LIS2DW12_TAP_PRIOR_YZX = 5,
    LIS2DW12_TAP_PRIOR_ZXY = 6
};

enum lis2dw12_fifo_mode {
    LIS2DW12_FIFO_M_BYPASS               = 0,
    LIS2DW12_FIFO_M_FIFO                 = 1,
    LIS2DW12_FIFO_M_CONTINUOUS_TO_FIFO   = 3,
    LIS2DW12_FIFO_M_BYPASS_TO_CONTINUOUS = 4,
    LIS2DW12_FIFO_M_CONTINUOUS           = 6
};

enum lis2dw12_read_mode {
    LIS2DW12_READ_M_POLL = 0,
    LIS2DW12_READ_M_STREAM = 1,
};

struct lis2dw12_notif_cfg {
    sensor_event_type_t event;
    uint8_t int_num:1;
    uint8_t int_cfg;
};

struct lis2dw12_tap_settings {
    uint8_t en_x  : 1;
    uint8_t en_y  : 1;
    uint8_t en_z  : 1;
    uint8_t en_4d : 1;
    enum lis2dw12_tap_priority tap_priority;

    /* ths data is 5 bits, fs = +-2g */
    int8_t tap_ths_x;
    int8_t tap_ths_y;
    int8_t tap_ths_z;

    enum lis2dw12_ths_6d ths_6d;

    /* latency is time between taps in double tap, 0 = 16 *1/ODR, LSB = 32*1/ODR */
    uint8_t latency;
    /* quiet is time after tap data is just below threshold
       0 = 2*1/ODR, LSB = 4*1/ODR */
    uint8_t quiet;
    /* shock is maximum time data can be over threshold to register as tap
       0 = 2*1/ODR, LSB = 4*1/ODR */
    uint8_t shock;

};

/* Read mode configuration */
struct lis2dw12_read_mode_cfg {
    enum lis2dw12_read_mode mode;
    uint8_t int_num:1;
    uint8_t int_cfg;
};

struct lis2dw12_cfg {
    uint8_t rate;
    uint8_t fs;

    /* Offset config */
    int8_t offset_x;
    int8_t offset_y;
    int8_t offset_z;
    uint8_t offset_weight;

    /* Tap config */
    struct lis2dw12_tap_settings tap;

    /* Read mode config */
    struct lis2dw12_read_mode_cfg read_mode;

    /* Notif config */
    struct lis2dw12_notif_cfg *notif_cfg;
    uint8_t max_num_notif;

    /* Freefall config */
    uint8_t freefall_dur;
    uint8_t freefall_ths;

    /* interrupt config */
    uint8_t int1_pin_cfg;
    uint8_t int2_pin_cfg;
    uint8_t map_int2_to_int1 : 1;

    uint8_t offset_en   : 1;

    uint8_t filter_bw   : 2;
    uint8_t high_pass   : 1;

    uint8_t int_enable  : 1;
    uint8_t int_pp_od   : 1;
    uint8_t int_latched : 1;
    uint8_t int_active_low  : 1;
    uint8_t inactivity_sleep_enable     : 1;
    uint8_t low_noise_enable            : 1;
    uint8_t stationary_detection_enable : 1;
    uint8_t double_tap_event_enable     : 1;

    uint8_t slp_mode       : 1;
    uint8_t self_test_mode : 3;

    /* Power mode */
    uint8_t power_mode     : 4;

    /* fifo  config */
    enum lis2dw12_fifo_mode fifo_mode;
    uint8_t fifo_threshold;

    /* sleep/wakeup settings */
    uint8_t wake_up_ths;
    uint8_t wake_up_dur;
    uint8_t sleep_duration;

    /* Sensor type mask to track enabled sensors */
    sensor_type_t mask;
};

/* Used to track interrupt state to wake any present waiters */
struct lis2dw12_int {
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
struct lis2dw12_pdd {
    /* Notification event context */
    struct sensor_notify_ev_ctx notify_ctx;
    /* Inetrrupt state */
    struct lis2dw12_int *interrupt;
    /* Interrupt enabled flag */
    uint16_t int_enable;
};

struct lis2dw12 {
    struct os_dev dev;
    struct sensor sensor;
    struct lis2dw12_cfg cfg;
    struct lis2dw12_int intr;
    struct lis2dw12_pdd pdd;
};

/**
 * Reset lis2dw12
 *
 * @param itf The sensor interface
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_reset(struct sensor_itf *itf);

/**
 * Get chip ID
 *
 * @param itf The sensor interface
 * @param chip_id Ptr to chip id to be filled up
 */
int lis2dw12_get_chip_id(struct sensor_itf *itf, uint8_t *chip_id);

/**
 * Sets the full scale selection
 *
 * @param itf The sensor interface
 * @param fs The full scale value to set
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_full_scale(struct sensor_itf *itf, uint8_t fs);

/**
 * Gets the full scale selection
 *
 * @param itf The sensor interface
 * @param fs Ptr to full scale read from the sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_full_scale(struct sensor_itf *itf, uint8_t *fs);

/**
 * Sets the rate
 *
 * @param itf The sensor interface
 * @param rate The sampling rate of the sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_rate(struct sensor_itf *itf, uint8_t rate);

/**
 * Gets the current rate
 *
 * @param itf The sensor interface
 * @param rate Ptr to rate read from the sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_rate(struct sensor_itf *itf, uint8_t *rate);

/**
 * Sets the low noise enable
 *
 * @param itf The sensor interface
 * @param en Low noise enabled
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_low_noise(struct sensor_itf *itf, uint8_t en);

/**
 * Gets whether low noise is enabled
 *
 * @param itf The sensor interface
 * @param en Ptr to low noise settings read from sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_low_noise(struct sensor_itf *itf, uint8_t *en);

/**
 * Sets the power mode of the sensor
 *
 * @param itf The sensor interface
 * @param mode Power mode
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_power_mode(struct sensor_itf *itf, uint8_t mode);

/**
 * Gets the power mode of the sensor
 *
 * @param itf The sensor interface
 * @param mode Ptr to power mode setting read from sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_power_mode(struct sensor_itf *itf, uint8_t *mode);

/**
 * Sets the self test mode of the sensor
 *
 * @param itf The sensor interface
 * @param mode Self test mode
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_self_test(struct sensor_itf *itf, uint8_t mode);

/**
 * Gets the self test mode of the sensor
 *
 * @param itf The sensor interface
 * @param mode Ptr to self test mode read from sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_self_test(struct sensor_itf *itf, uint8_t *mode);

/**
 * Sets the interrupt push-pull/open-drain selection
 *
 * @param itf The sensor interface
 * @param mode Interrupt setting (0 = push-pull, 1 = open-drain)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_int_pp_od(struct sensor_itf *itf, uint8_t mode);

/**
 * Gets the interrupt push-pull/open-drain selection
 *
 * @param itf The sensor interface
 * @param mode Ptr to store setting (0 = push-pull, 1 = open-drain)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_int_pp_od(struct sensor_itf *itf, uint8_t *mode);

/**
 * Sets whether latched interrupts are enabled
 *
 * @param itf The sensor interface
 * @param en Value to set (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_latched_int(struct sensor_itf *itf, uint8_t en);

/**
 * Gets whether latched interrupts are enabled
 *
 * @param itf The sensor interface
 * @param en Ptr to store value (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_latched_int(struct sensor_itf *itf, uint8_t *en);

/**
 * Sets whether interrupts are active high or low
 *
 * @param itf The sensor interface
 * @param low Value to set (0 = active high, 1 = active low)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_int_active_low(struct sensor_itf *itf, uint8_t low);

/**
 * Gets whether interrupts are active high or low
 *
 * @param itf The sensor interface
 * @param low Ptr to store value (0 = active high, 1 = active low)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_int_active_low(struct sensor_itf *itf, uint8_t *low);

/**
 * Sets single data conversion mode
 *
 * @param itf The sensor interface
 * @param mode Value to set (0 = trigger on INT2 pin, 1 = trigger on write to SLP_MODE_1)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_slp_mode(struct sensor_itf *itf, uint8_t mode);

/**
 * Gets single data conversion mode
 *
 * @param itf The sensor interface
 * @param mode Ptr to store value (0 = trigger on INT2 pin, 1 = trigger on write to SLP_MODE_1)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_slp_mode(struct sensor_itf *itf, uint8_t *mode);

/**
 * Starts a data conversion in on demand mode
 *
 * @param itf The sensor interface
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_start_on_demand_conversion(struct sensor_itf *itf);

/**
 * Set filter config
 *
 * @param itf The sensor interface
 * @param bw The filter bandwidth
 * @param type The filter type (1 = high pass, 0 = low pass)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_filter_cfg(struct sensor_itf *itf, uint8_t bw, uint8_t type);

/**
 * Get filter config
 *
 * @param itf The sensor interface
 * @param bw Ptr to the filter bandwidth
 * @param type Ptr to filter type (1 = high pass, 0 = low pass)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_filter_cfg(struct sensor_itf *itf, uint8_t *bw, uint8_t *type);

/**
 * Sets new offsets in sensor
 *
 * @param itf The sensor interface
 * @param offset_x X offset
 * @param offset_y Y offset
 * @param offset_z Z offset
 * @param weight Value Weight (0 = 977ug/LSB, 1 = 15.6mg/LSB)
 *
 * @return 0 on success, non-zero error on failure.
 */
int lis2dw12_set_offsets(struct sensor_itf *itf, int8_t offset_x,
                         int8_t offset_y, int8_t offset_z, uint8_t weight);

/**
 * Gets the offsets currently set
 *
 * @param itf The sensor interface
 * @param offset_x Ptr to location to store X offset
 * @param offset_y Ptr to location to store Y offset
 * @param offset_z Ptr to location to store Z offset
 * @param weight Ptr to value weight
 *
 * @return 0 on success, non-zero error on failure.
 */
int lis2dw12_get_offsets(struct sensor_itf *itf, int8_t *offset_x,
                         int8_t *offset_y, int8_t *offset_z, uint8_t *weight);

/**
 * Sets whether offset are enabled (only work when low pass filtering is enabled)
 *
 * @param itf The sensor interface
 * @param enabled value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero error on failure.
 */
int lis2dw12_set_offset_enable(struct sensor_itf *itf, uint8_t enabled);

/**
 * Set tap detection configuration
 *
 * @param itf The sensor interface
 * @param cfg The tap settings config
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_tap_cfg(struct sensor_itf *itf, struct lis2dw12_tap_settings *cfg);

/**
 * Get tap detection config
 *
 * @param itf the sensor interface
 * @param cfg Ptr to the tap settings
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_tap_cfg(struct sensor_itf *itf, struct lis2dw12_tap_settings *cfg);

/**
 * Set freefall detection configuration
 *
 * @param itf The sensor interface
 * @param dur Freefall duration (5 bits LSB = 1/ODR)
 * @param ths Freefall threshold (3 bits)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_freefall(struct sensor_itf *itf, uint8_t dur, uint8_t ths);

/**
 * Get freefall detection config
 *
 * @param itf The sensor interface
 * @param dur Ptr to freefall duration
 * @param ths Ptr to freefall threshold
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_freefall(struct sensor_itf *itf, uint8_t *dur, uint8_t *ths);

/**
 * Set interrupt pin configuration for interrupt 1
 *
 * @param itf The sensor interface
 * @param cfg int1 config
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_int1_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set interrupt pin configuration for interrupt 2
 *
 * @param itf The sensor interface
 * @param cfg int2 config
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_int2_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Clear interrupt pin configuration for interrupt 1
 *
 * @param itf The sensor interface
 * @param cfg int1 config
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_clear_int1_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Clear interrupt pin configuration for interrupt 2
 *
 * @param itf The sensor interface
 * @param cfg int2 config
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_clear_int2_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set whether interrupts are enabled
 *
 * @param itf The sensor interface
 * @param enabled Value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_int_enable(struct sensor_itf *itf, uint8_t enabled);

/**
 * Clear interrupts
 *
 * @param itf The sensor interface
 * @param src Ptr to return interrupt source in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_clear_int(struct sensor_itf *itf, uint8_t *src);

/**
 * Get Interrupt Status
 *
 * @param itf The sensor interface
 * @param status Ptr to return interrupt status in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_int_status(struct sensor_itf *itf, uint8_t *status);

/**
 * Get Wake Up Source
 *
 * @param itf The sensor interface
 * @param Ptr to return wake_up_src in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_src(struct sensor_itf *itf, uint8_t *status);

/**
 * Get Tap Source
 *
 * @param itf The sensor interface
 * @param Ptr to return tap_src in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_tap_src(struct sensor_itf *itf, uint8_t *status);

/**
 * Get 6D Source
 *
 * @param itf The sensor interface
 * @param Ptr to return sixd_src in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_sixd_src(struct sensor_itf *itf, uint8_t *status);

/**
 * Setup FIFO
 *
 * @param itf The sensor interface
 * @param mode FIFO mode to setup
 * @patam fifo_ths Threshold to set for FIFO
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_fifo_cfg(struct sensor_itf *itf, enum lis2dw12_fifo_mode mode, uint8_t fifo_ths);

/**
 * Get Number of Samples in FIFO
 *
 * @param itf The sensor interface
 * @patam samples Ptr to return number of samples in
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_fifo_samples(struct sensor_itf *itf, uint8_t *samples);

/**
 * Set Wake Up Threshold configuration
 *
 * @param itf The sensor interface
 * @param reg wake_up_ths value to set
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_wake_up_ths(struct sensor_itf *itf, uint8_t reg);

/**
 * Get Wake Up Threshold config
 *
 * @param itf The sensor interface
 * @param reg Ptr to store wake_up_ths value
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_ths(struct sensor_itf *itf, uint8_t *reg);

/**
 * Set whether sleep on inactivity is enabled
 *
 * @param itf The sensor interface
 * @param en Value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_inactivity_sleep_en(struct sensor_itf *itf, uint8_t en);

/**
 * Get whether sleep on inactivity is enabled
 *
 * @param itf The sensor interface
 * @param en Ptr to store read state (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_inactivity_sleep_en(struct sensor_itf *itf, uint8_t *en);

/**
 * Set whether double tap event is enabled
 *
 * @param itf The sensor interface
 * @param en Value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_double_tap_event_en(struct sensor_itf *itf, uint8_t en);

/**
 * Get whether double tap event is enabled
 *
 * @param itf The sensor interface
 * @param Ptr to store read state (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_double_tap_event_en(struct sensor_itf *itf, uint8_t *en);

/**
 * Set Wake Up Duration
 *
 * @param itf The sensor interface
 * @param reg duration to set
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_wake_up_dur(struct sensor_itf *itf, uint8_t reg);

/**
 * Get Wake Up Duration
 *
 * @param itf The sensor interface
 * @param reg Ptr to store wake_up_dur value
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_dur(struct sensor_itf *itf, uint8_t *reg);

/**
 * Set Sleep Duration
 *
 * @param itf The sensor interface
 * @param reg Duration to set
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_sleep_dur(struct sensor_itf *itf, uint8_t reg);

/**
 * Get Sleep Duration
 *
 * @param itf The sensor interface
 * @param reg Ptr to store sleep_dur value
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_sleep_dur(struct sensor_itf *itf, uint8_t *reg);

/**
 * Set Stationary Detection Enable
 *
 * @param itf The sensor interface
 * @param en value to set
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_stationary_en(struct sensor_itf *itf, uint8_t en);

/**
 * Get Stationary Detection Enable
 *
 * @param itf The sensor interface
 * @param en ptr to store sleep_dur value
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_stationary_en(struct sensor_itf *itf, uint8_t *en);

/**
 * Set whether interrupts are enabled
 *
 * @param itf The sensor interface
 * @param enable Value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_int1_on_int2_map(struct sensor_itf *itf, bool enable);

/**
 * Get whether interrupt 1 signals is mapped onto interrupt 2 pin
 *
 * @param itf The sensor interface
 * @param val Value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_int1_on_int2_map(struct sensor_itf *itf, uint8_t *val);

/**
 * Run Self test on sensor
 *
 * @param itf The sensor interface
 * @param result Ptr to return test result in (0 on pass, non-zero on failure)
 *
 * @return 0 on sucess, non-zero on failure
 */
int lis2dw12_run_self_test(struct sensor_itf *itf, int *result);

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
int lis2dw12_stream_read(struct sensor *sensor,
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
int lis2dw12_poll_read(struct sensor *sensor,
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
int lis2dw12_init(struct os_dev *dev, void *arg);

/**
 * Configure the sensor
 *
 * @param lis2dw12 Ptr to sensor driver
 * @param cfg Ptr to sensor driver config
 */
int lis2dw12_config(struct lis2dw12 *lis2dw12, struct lis2dw12_cfg *cfg);

#if MYNEWT_VAL(LIS2DW12_CLI)
int lis2dw12_shell_init(void);
#endif


#ifdef __cplusplus
}
#endif

#endif /* __LIS2DW12_H__ */
