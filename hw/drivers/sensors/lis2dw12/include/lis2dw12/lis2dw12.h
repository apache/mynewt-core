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

#define LIS2DW12_HPF_M_NORMAL0                  0x00
#define LIS2DW12_HPF_M_REF                      0x01
#define LIS2DW12_HPF_M_NORMAL1                  0x02
#define LIS2DW12_HPF_M_AIE                      0x03

#define LIS2DW12_FILTER_BW_ODR_DIV_2            0x00
#define LIS2DW12_FILTER_BW_ODR_DIV_4            0x40
#define LIS2DW12_FILTER_BW_ODR_DIV_10           0x80
#define LIS2DW12_FILTER_BW_ODR_DIV_20           0xC0

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
    
struct lis2dw12_tap_settings {
    uint8_t en_x;
    uint8_t en_y; 
    uint8_t en_z;
    
    uint8_t en_4d;
    enum lis2dw12_ths_6d ths_6d;
    enum lis2dw12_tap_priority tap_priority;

    /* ths data is 5 bits, fs = +-2g */
    int8_t tap_ths_x;
    int8_t tap_ths_y;
    int8_t tap_ths_z;

    /* latency is time between taps in double tap, 0 = 16 *1/ODR, LSB = 32*1/ODR */
    uint8_t latency;
    /* quiet is time after tap data bust be below threshold
       0 = 2*1/ODR, LSB = 4*1/ODR */
    uint8_t quiet;
    /* shock is maximum time data can be over threshold to register as tap
       0 = 2*1/ODR, LSB = 4*1/ODR */
    uint8_t shock;
    
};

    
struct lis2dw12_cfg {
    uint8_t rate;
    uint8_t fs;

    int8_t offset_x;
    int8_t offset_y;
    int8_t offset_z;
    uint8_t offset_weight;
    uint8_t offset_en;

    uint8_t filter_bw;
    uint8_t high_pass;

    struct lis2dw12_tap_settings tap_cfg;
    uint8_t double_tap_event_enable;
    
    uint8_t int1_pin_cfg;
    uint8_t int2_pin_cfg;
    uint8_t int_enable;

    enum lis2dw12_fifo_mode fifo_mode;
    uint8_t fifo_threshold;

    uint8_t wake_up_ths;
    uint8_t wake_up_dur;
    uint8_t sleep_duration;

    uint8_t stationary_detection_enable;

    uint8_t power_mode;
    uint8_t inactivity_sleep_enable;
    uint8_t low_noise_enable;
    
    enum lis2dw12_read_mode read_mode;
    uint8_t stream_read_interrupt;
    
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

    
struct lis2dw12_private_driver_data {
    struct sensor_notify_ev_ctx notify_ctx;
    uint8_t registered_mask;

    struct lis2dw12_int * interrupt;
    
    uint8_t int_num;
    uint8_t int_enable;
};
        
    
struct lis2dw12 {
    struct os_dev dev;
    struct sensor sensor;
    struct lis2dw12_cfg cfg;
    struct lis2dw12_int intr;

    struct lis2dw12_private_driver_data pdd;
};

/**
 * Reset lis2dw12
 *
 * @param The sensor interface
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_reset(struct sensor_itf *itf);

/**
 * Get chip ID
 *
 * @param sensor interface
 * @param ptr to chip id to be filled up
 */
int lis2dw12_get_chip_id(struct sensor_itf *itf, uint8_t *chip_id);
    
/**
 * Sets the full scale selection
 *
 * @param The sensor interface
 * @param fs to set
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_full_scale(struct sensor_itf *itf, uint8_t fs);

/**
 * Gets the full scale selection
 *
 * @param The sensor interface
 * @param ptr to fs
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_full_scale(struct sensor_itf *itf, uint8_t *fs);

/**
 * Sets the rate
 *
 * @param The sensor interface
 * @param The rate
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_rate(struct sensor_itf *itf, uint8_t rate);

/**
 * Gets the current rate
 *
 * @param The sensor interface
 * @param ptr to rate read from the sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_rate(struct sensor_itf *itf, uint8_t *rate);

/**
 * Sets the low noise enable
 *
 * @param The sensor interface
 * @param low noise enabled
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_low_noise(struct sensor_itf *itf, uint8_t en);

/**
 * Gets whether low noise is enabled
 *
 * @param The sensor interface
 * @param ptr to low noise settings read from sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_low_noise(struct sensor_itf *itf, uint8_t *en);

/**
 * Sets the power mode of the sensor
 *
 * @param The sensor interface
 * @param power mode
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_power_mode(struct sensor_itf *itf, uint8_t mode);

/**
 * Gets the power mode of the sensor
 *
 * @param The sensor interface
 * @param ptr to power mode setting read from sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_power_mode(struct sensor_itf *itf, uint8_t *mode);

/**
 * Sets the self test mode of the sensor
 *
 * @param The sensor interface
 * @param self test mode
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_self_test(struct sensor_itf *itf, uint8_t mode);

/**
 * Gets the power mode of the sensor
 *
 * @param The sensor interface
 * @param ptr to self test mode read from sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_self_test(struct sensor_itf *itf, uint8_t *mode);

/**
 * Set filter config
 *
 * @param the sensor interface
 * @param the filter bandwidth
 * @param filter type (1 = high pass, 0 = low pass)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_filter_cfg(struct sensor_itf *itf, uint8_t bw, uint8_t type);

/**
 * Get filter config
 *
 * @param the sensor interface
 * @param ptr to the filter bandwidth
 * @param ptr to filter type (1 = high pass, 0 = low pass)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_filter_cfg(struct sensor_itf *itf, uint8_t *bw, uint8_t *type);
    
/**
 * Sets new offsets in sensor
 *
 * @param The sensor interface
 * @param X offset
 * @param Y offset
 * @param Z offset
 * @param Value Weight (0 = 977ug/LSB, 1 = 15.6mg/LSB)
 *
 * @return 0 on success, non-zero error on failure.
 */
int lis2dw12_set_offsets(struct sensor_itf *itf, int8_t offset_x,
                         int8_t offset_y, int8_t offset_z, uint8_t weight);

/**
 * Gets the offsets currently set
 *
 * @param The sensor interface
 * @param Pointer to location to store X offset
 * @param Pointer to location to store Y offset
 * @param Pointer to location to store Z offset
 * @param Pointer to value weight
 *
 * @return 0 on success, non-zero error on failure.
 */
int lis2dw12_get_offsets(struct sensor_itf *itf, int8_t *offset_x,
                         int8_t *offset_y, int8_t *offset_z, uint8_t * weight);

/**
 * Sets whether offset are enabled (only work when low pass filtering is enabled)
 *
 * @param The sensor interface
 * @param value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero error on failure.
 */
int lis2dw12_set_offset_enable(struct sensor_itf *itf, uint8_t enabled);
   
/**
 * Set tap detection configuration
 *
 * @param the sensor interface
 * @param the tap settings
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_tap_cfg(struct sensor_itf *itf, struct lis2dw12_tap_settings *cfg);

/**
 * Get tap detection config
 *
 * @param the sensor interface
 * @param ptr to the tap settings
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_tap_cfg(struct sensor_itf *itf, struct lis2dw12_tap_settings *cfg);

/**
 * Set freefall detection configuration
 *
 * @param the sensor interface
 * @param freefall duration (5 bits LSB = 1/ODR)
 * @param freefall threshold (3 bits)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_freefall(struct sensor_itf *itf, uint8_t dur, uint8_t ths);

/**
 * Get freefall detection config
 *
 * @param the sensor interface
 * @param ptr to freefall duration
 * @param ptr to freefall threshold
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_freefall(struct sensor_itf *itf, uint8_t *dur, uint8_t *ths);
    
/**
 * Set interrupt pin configuration for interrupt 1
 *
 * @param the sensor interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_int1_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set interrupt pin configuration for interrupt 2
 *
 * @param the sensor interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_int2_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set whether interrupts are enabled
 *
 * @param the sensor interface
 * @param value to set (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_int_enable(struct sensor_itf *itf, uint8_t enabled);

/**
 * Clear interrupts
 *
 * @param the sensor interface
 * @param pointer to return interrupt source in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_clear_int(struct sensor_itf *itf, uint8_t *src);

/**
 * Get Interrupt Source
 *
 * @param the sensor interface
 * @param pointer to return interrupt source in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_int_src(struct sensor_itf *itf, uint8_t *status);

/**
 * Get Wake Up Source
 *
 * @param the sensor interface
 * @param pointer to return wake_up_src in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_src(struct sensor_itf *itf, uint8_t *status);

/**
 * Get Tap Source
 *
 * @param the sensor interface
 * @param pointer to return tap_src in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_tap_src(struct sensor_itf *itf, uint8_t *status);

/**
 * Get 6D Source
 *
 * @param the sensor interface
 * @param pointer to return sixd_src in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_sixd_src(struct sensor_itf *itf, uint8_t *status);

    
/**
 * Setup FIFO
 *
 * @param the sensor interface
 * @param FIFO mode to setup
 * @patam Threshold to set for FIFO
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_fifo_cfg(struct sensor_itf *itf, enum lis2dw12_fifo_mode mode, uint8_t fifo_ths);

/**
 * Get Number of Samples in FIFO
 *
 * @param the sensor interface
 * @patam Pointer to return number of samples in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_fifo_samples(struct sensor_itf *itf, uint8_t *samples);

/**
 * Set Wake Up Threshold configuration
 *
 * @param the sensor interface
 * @param wake_up_ths value to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_wake_up_ths(struct sensor_itf *itf, uint8_t reg);

/**
 * Get Wake Up Threshold config
 *
 * @param the sensor interface
 * @param ptr to store wake_up_ths value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_ths(struct sensor_itf *itf, uint8_t *reg);

/**
 * Set whether sleep on inactivity is enabled
 *
 * @param the sensor interface
 * @param value to set (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_inactivity_sleep_en(struct sensor_itf *itf, uint8_t en);

/**
 * Get whether sleep on inactivity is enabled
 *
 * @param the sensor interface
 * @param ptr to store read state (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_inactivity_sleep_en(struct sensor_itf *itf, uint8_t *en);

/**
 * Set whether double tap event is enabled
 *
 * @param the sensor interface
 * @param value to set (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_double_tap_event_en(struct sensor_itf *itf, uint8_t en);

/**
 * Get whether double tap event is enabled
 *
 * @param the sensor interface
 * @param ptr to store read state (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_double_tap_event_en(struct sensor_itf *itf, uint8_t *en);
    
/**
 * Set Wake Up Duration
 *
 * @param the sensor interface
 * @param duration to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_wake_up_dur(struct sensor_itf *itf, uint8_t reg);

/**
 * Get Wake Up Duration
 *
 * @param the sensor interface
 * @param ptr to store wake_up_dur value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_dur(struct sensor_itf *itf, uint8_t *reg);

/**
 * Set Sleep Duration
 *
 * @param the sensor interface
 * @param duration to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_sleep_dur(struct sensor_itf *itf, uint8_t reg);

/**
 * Get Sleep Duration
 *
 * @param the sensor interface
 * @param ptr to store sleep_dur value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_sleep_dur(struct sensor_itf *itf, uint8_t *reg);

/**
 * Set Stationary Detection Enable
 *
 * @param the sensor interface
 * @param value to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_stationary_en(struct sensor_itf *itf, uint8_t en);

/**
 * Get Stationary Detection Enable
 *
 * @param the sensor interface
 * @param ptr to store sleep_dur value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_stationary_en(struct sensor_itf *itf, uint8_t *en);
    
/**
 * Run Self test on sensor
 *
 * @param the sensor interface
 * @param pointer to return test result in (0 on pass, non-zero on failure)
 *
 * @return 0 on sucess, non-zero on failure
 */
int lis2dw12_run_self_test(struct sensor_itf *itf, int *result);

/**
 * Provide a continuous stream of accelerometer readings.
 *
 * @param The sensor ptr
 * @param The sensor type
 * @param The function pointer to invoke for each accelerometer reading.
 * @param The opaque pointer that will be passed in to the function.
 * @param If non-zero, how long the stream should run in milliseconds.
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
 * @param The sensor ptr
 * @param The sensor type
 * @param The function pointer to invoke for each accelerometer reading.
 * @param The opaque pointer that will be passed in to the function.
 * @param If non-zero, how long the stream should run in milliseconds.
 *
 * @return 0 on success, non-zero on failure.
 */
int lis2dw12_poll_read(struct sensor * sensor,
                       sensor_type_t sensor_type,
                       sensor_data_func_t data_func,
                       void * data_arg,
                       uint32_t timeout);

/**
 * Expects to be called back through os_dev_create().
 *
 * @param ptr to the device object associated with this accelerometer
 * @param argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int lis2dw12_init(struct os_dev *dev, void *arg);

/**
 * Configure the sensor
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 */
int lis2dw12_config(struct lis2dw12 *, struct lis2dw12_cfg *);

#if MYNEWT_VAL(LIS2DW12_CLI)
int lis2dw12_shell_init(void);
#endif
   
    
#ifdef __cplusplus
}
#endif

#endif /* __LIS2DW12_H__ */
