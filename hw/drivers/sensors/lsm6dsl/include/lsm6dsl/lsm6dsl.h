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

#ifndef __LSM6DSL_H__
#define __LSM6DSL_H__

#include <os/mynewt.h>
#include <sensor/sensor.h>
#include "../src/lsm6dsl_priv.h"

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#include "bus/drivers/spi_common.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Threshold for D4D/D6D function */
enum lsm6dsl_ths_6d {
    LSM6DSL_6D_THS_80_DEG = 0,
    LSM6DSL_6D_THS_70_DEG = 1,
    LSM6DSL_6D_THS_60_DEG = 2,
    LSM6DSL_6D_THS_50_DEG = 3
};

/* Sensors read mode */
enum lsm6dsl_read_mode {
    LSM6DSL_READ_POLL = 0,
    LSM6DSL_READ_STREAM = 1,
};

/* Threshold for Free Fall detection */
typedef enum {
    LSM6DSL_FF_THS_156_MG_VAL,
    LSM6DSL_FF_THS_219_MG_VAL,
    LSM6DSL_FF_THS_250_MG_VAL,
    LSM6DSL_FF_THS_312_MG_VAL,
    LSM6DSL_FF_THS_344_MG_VAL,
    LSM6DSL_FF_THS_406_MG_VAL,
    LSM6DSL_FF_THS_469_MG_VAL,
    LSM6DSL_FF_THS_500_MG_VAL,
} free_fall_threshold_t;

struct lsm6dsl_int {
    /* Sleep waiting for an interrupt to occur */
    struct os_sem wait;

    /* Is the interrupt currently active */
    bool active;

    /* Is there a waiter currently sleeping */
    bool asleep;

    /* Configured interrupts */
    struct sensor_int ints[1];
};

struct lsm6dsl_orientation_settings {
    uint8_t en_4d: 1;

    /* 6D/4D angle threshold */
    enum lsm6dsl_ths_6d ths_6d;
};

typedef enum {
    LSM6DSL_INACTIVITY_DISABLED,
    LSM6DSL_INACTIVITY_WITH_GYRO_UNCHANGED,
    LSM6DSL_INACTIVITY_WITH_GYRO_SLEEP,
    LSM6DSL_INACTIVITY_WITH_GYRO_POWER_DOWN,
} lsm6dsl_inactivity_t;

struct lsm6dsl_wk_settings {
    /* Wakeup threshold in FS_XL/(2^6)  */
    uint8_t wake_up_ths;
    /* Wake duration in ODR_time */
    uint8_t wake_up_dur;
    /* Inactivity time before going to sleep in 512 ODR_Time (0 = 16 ODR time) */
    uint8_t sleep_duration;
    /* hpf or slope, 1 = hpf, 0 = slope */
    uint8_t hpf_slope;
    /* Inactivity mode, when enabled, sleep interrupt will fire when changing to inactive mode */
    lsm6dsl_inactivity_t inactivity;
};

struct lsm6dsl_ff_settings {
    /* Free-fall configuration parameters 1LSB = 1 ORD time */
    uint8_t free_fall_dur;
    /* Free-fall threshold */
    free_fall_threshold_t free_fall_ths;
};

struct lsm6dsl_tilt_settings {
    uint8_t en_rel_tilt;
    uint8_t en_wrist_tilt;
    /* Timer for latency. 1LSB = 40ms */
    uint8_t tilt_lat;
    /* Threshold for Tilt function 1LSB = 15.626 mg*/
    uint8_t tilt_ths;
    /* A_WRIST_TILT_Mask reg (z pos and neg)*/
    uint8_t tilt_axis_mask;
};

struct lsm6dsl_tap_settings {
    /* Axis enabled bitmask */
    uint8_t en_x: 1;
    uint8_t en_y: 1;
    uint8_t en_z: 1;
    uint8_t en_dtap: 1;

    /* Threshold for tap recognition */
    int8_t tap_ths;

    /* Time duration of maximum time gap for double tap recognition */
    uint8_t dur;

    /* Expected quiet time after a tap detection */
    uint8_t quiet;

    /* Maximum duration of overthreshold event */
    uint8_t shock;
};

struct lsm6dsl_notif_cfg {
    /* Interrupt event registered */
    sensor_event_type_t event;

    /* Interrupt pin number (0/1) */
    uint8_t int_num: 1;

    /* Interrupt bit mask */
    uint8_t int_mask;

    /* Int enable bit */
    uint8_t int_en;
};

/* Read mode configuration */
struct lsm6dsl_read_mode_cfg {
    enum lsm6dsl_read_mode mode;
    uint8_t int_num: 1;
    uint8_t int_cfg;
    uint8_t int_reg;
};

typedef enum {
    /* Supported FIFO mode by driver */
    LSM6DSL_FIFO_MODE_BYPASS_VAL = 0x00,
    LSM6DSL_FIFO_MODE_CONTINUOUS_VAL = 0x06
} lsm6dsl_fifo_mode_t;

struct lsm6dsl_fifo_cfg {
    lsm6dsl_fifo_mode_t mode;
    uint16_t wtm;
};

struct int_src_regs {
    uint8_t wake_up_src;
    uint8_t tap_src;
    uint8_t d6d_src;
    uint8_t status_reg;
    uint8_t func_src1;
    uint8_t func_src2;
    uint8_t wrist_tilt_ia;
};

struct lsm6dsl_cfg {
    uint8_t acc_fs;
    uint8_t acc_rate;
    uint8_t gyro_fs;
    uint8_t gyro_rate;

    struct lsm6dsl_tap_settings tap;
    struct lsm6dsl_orientation_settings orientation;
    struct lsm6dsl_wk_settings wk;
    struct lsm6dsl_ff_settings ff;
    struct lsm6dsl_tilt_settings tilt;

    /* Event notification config */
    struct lsm6dsl_notif_cfg *notify_cfg;
    uint8_t notify_cfg_count;

    /* Read mode config */
    struct lsm6dsl_read_mode_cfg read;

    /* FIFO configuration */
    struct lsm6dsl_fifo_cfg fifo;

    /* Pin interrupt config */
    uint8_t int1_pin_cfg;
    uint8_t int2_pin_cfg;
    uint8_t map_int2_to_int1 : 1;
    uint8_t latched_int : 1;

    /* The sensors mask */
    sensor_type_t lc_s_mask;
};

/* Private per driver data */
struct lsm6dsl_pdd {
    /* Notification event context */
    struct sensor_notify_ev_ctx notify_ctx;

    /* Interrupt state */
    struct lsm6dsl_int *interrupt;

    /* Interrupt enabled flag */
    uint16_t int_enable;
};

struct lsm6dsl_create_dev_cfg {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    bool node_is_spi;
    union {
#if MYNEWT_VAL(LSM6DSL_SPI_SUPPORT)
        struct bus_spi_node_cfg spi_cfg;
#endif
#if MYNEWT_VAL(LSM6DSL_I2C_SUPPORT)
        struct bus_i2c_node_cfg i2c_cfg;
#endif
    };
#else
    struct sensor_itf itf;
#endif
    int16_t int_pin;
    bool int_active_level;
};

struct lsm6dsl {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    union {
        struct bus_i2c_node i2c_node;
        struct bus_spi_node spi_node;
    };
    bool node_is_spi;
#else
    struct os_dev dev;
#endif
    struct sensor sensor;
    struct lsm6dsl_cfg_regs1 cfg_regs1;
    struct lsm6dsl_cfg_regs2 cfg_regs2;
    struct lsm6dsl_cfg cfg;
    struct lsm6dsl_int intr;
    struct lsm6dsl_pdd pdd;
    float acc_mult;
    float gyro_mult;
};

/* Angular rate sensor self-test mode selection */
#define LSM6DSL_NORMAL_MODE_G_ST_VAL    0x00
#define LSM6DSL_POSITIVE_SIGN_G_ST_VAL    0x01
#define LSM6DSL_NEGATIVE_SIGN_G_ST_VAL    0x03

/* Linear acceleration sensor self-test mode selection */
#define LSM6DSL_NORMAL_MODE_XL_ST_VAL    0x00
#define LSM6DSL_POSITIVE_SIGN_XL_ST_VAL    0x01
#define LSM6DSL_NEGATIVE_SIGN_XL_ST_VAL    0x02

/* Accelerometer bandwidth configurations */
#define LSM6DSL_BW_LP_XL_ODR_2_VAL      0x00
#define LSM6DSL_BW_LP_XL_ODR_4_VAL      0x00
#define LSM6DSL_BW_LP_XL_ODR_10_VAL     0x01
#define LSM6DSL_BW_LP_XL_ODR_20_VAL     0x02
#define LSM6DSL_BW_LP_XL_ODR_45_VAL     0x03
#define LSM6DSL_BW_LP_XL_ODR_100_VAL    0x04
#define LSM6DSL_BW_LP_XL_ODR_200_VAL    0x05
#define LSM6DSL_BW_LP_XL_ODR_400_VAL    0x06
#define LSM6DSL_BW_LP_XL_ODR_800_VAL    0x07

#define LSM6DSL_BW_HP_XL_SLOPE_VAL      0x00
#define LSM6DSL_BW_HP_XL_ODR_10_VAL     0x01
#define LSM6DSL_BW_HP_XL_ODR_20_VAL     0x02
#define LSM6DSL_BW_HP_XL_ODR_45_VAL     0x03
#define LSM6DSL_BW_HP_XL_ODR_100_VAL    0x04
#define LSM6DSL_BW_HP_XL_ODR_200_VAL    0x05
#define LSM6DSL_BW_HP_XL_ODR_400_VAL    0x06
#define LSM6DSL_BW_HP_XL_ODR_800_VAL    0x07

/* TAP priority decoding */
#define LSM6DSL_TAP_PRIO_XYZ_VAL        0x00
#define LSM6DSL_TAP_PRIO_YXZ_VAL        0x01
#define LSM6DSL_TAP_PRIO_XZY_VAL        0x02
#define LSM6DSL_TAP_PRIO_ZYX_VAL        0x03

/* Accelerometer data rate */
typedef enum {
    LSM6DSL_ACCEL_OFF                   = 0x00,
    LSM6DSL_ACCEL_12_5HZ                = 0x01,
    LSM6DSL_ACCEL_26HZ                  = 0x02,
    LSM6DSL_ACCEL_52HZ                  = 0x03,
    LSM6DSL_ACCEL_104HZ                 = 0x04,
    LSM6DSL_ACCEL_208HZ                 = 0x05,
    LSM6DSL_ACCEL_416HZ                 = 0x06,
    LSM6DSL_ACCEL_833HZ                 = 0x07,
    LSM6DSL_ACCEL_1666HZ                = 0x08,
    LSM6DSL_ACCEL_3333HZ                = 0x09,
    LSM6DSL_ACCEL_6666HZ                = 0x0a,
} acc_data_rate_t;

/* Gyroscope data rate */
typedef enum {
    LSM6DSL_GYRO_OFF                    = 0x00,
    LSM6DSL_GYRO_12_5HZ                 = 0x01,
    LSM6DSL_GYRO_26HZ                   = 0x02,
    LSM6DSL_GYRO_52HZ                   = 0x03,
    LSM6DSL_GYRO_104HZ                  = 0x04,
    LSM6DSL_GYRO_208HZ                  = 0x05,
    LSM6DSL_GYRO_416HZ                  = 0x06,
    LSM6DSL_GYRO_833HZ                  = 0x07,
    LSM6DSL_GYRO_1666HZ                 = 0x08,
    LSM6DSL_GYRO_3333HZ                 = 0x09,
    LSM6DSL_GYRO_6666HZ                 = 0x0a,
} gyro_data_rate_t;

/* Accelerometer full scale range in G */
typedef enum {
    LSM6DSL_ACCEL_FS_2G                 = 0x00,
    LSM6DSL_ACCEL_FS_4G                 = 0x02,
    LSM6DSL_ACCEL_FS_8G                 = 0x03,
    LSM6DSL_ACCEL_FS_16G                = 0x01,
} accel_full_scale_t;

typedef enum {
    /* 2^-10 g/LSB */
    LSM6DSL_USER_WEIGHT_LO,
    /* 2^-6 g/LSB */
    LSM6DSL_USER_WEIGHT_HI,
} user_off_weight_t;

typedef enum {
    DATA_NOT_IN_FIFO                    = 0,
    NO_DECIMATION                       = 1,
    DECIMATION_FACTOR_2                 = 2,
    DECIMATION_FACTOR_3                 = 3,
    DECIMATION_FACTOR_4                 = 4,
    DECIMATION_FACTOR_8                 = 5,
    DECIMATION_FACTOR_16                = 6,
    DECIMATION_FACTOR_32                = 7,
} fifo_decimation_t;

#define LSM6DSL_ACCEL_FS_MIN_VAL	    2
#define LSM6DSL_ACCEL_FS_MAX_VAL	    16

/* Gyroscope full scale range in DPS */
typedef enum {
    LSM6DSL_GYRO_FS_125DPS              = 0x01,
    LSM6DSL_GYRO_FS_250DPS              = 0x00,
    LSM6DSL_GYRO_FS_500DPS              = 0x02,
    LSM6DSL_GYRO_FS_1000DPS             = 0x04,
    LSM6DSL_GYRO_FS_2000DPS             = 0x08,
} gyro_full_scale_t;

#define LSM6DSL_GYRO_FS_MIN_VAL		125
#define LSM6DSL_GYRO_FS_MAX_VAL		2000

/**
 * Expects to be called back through os_dev_create().
 *
 * @param ptr to the device object associated with this accelerometer
 * @param argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int lsm6dsl_init(struct os_dev *dev, void *arg);

/**
 * Configure the sensor
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 */
int lsm6dsl_config(struct lsm6dsl *lsm6dsl, struct lsm6dsl_cfg *cfg);

/**
 * Init shell
 *
 * @return 0 on success, non-zero on failure.
 */
int lsm6dsl_shell_init(void);

/**
 * Run Self test on sensor
 *
 * @param lsm6dsl The device
 * @param pointer to return test result in
 *        0 on pass
 *        1 on XL failure
 *        2 on Gyro failure
 *
 * @return 0 on sucess, non-zero on failure
 */
int lsm6dsl_run_self_test(struct lsm6dsl *lsm6dsl, int *result);

/**
 * Gets a new data sample from the acc/gyro/temp sensor.
 *
 * @param lsm6dsl The device
 * @param Sensor type
 * @param axis data
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_get_ag_data(struct lsm6dsl *lsm6dsl, sensor_type_t type, void *data);

/**
 * Create lsm6dsl device
 *
 * @param lsm6dsl device object to initialize
 * @param name    name of the device
 * @param cfg     device configuration
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_create_dev(struct lsm6dsl *lsm6dsl, const char *name,
                       const struct lsm6dsl_create_dev_cfg *cfg);

/**
 * Write number of data bytes to LSM6DSL sensor over interface
 *
 * @param lsm6dsl The device
 * @param reg    register address
 * @param buffer data buffer
 * @param len    number of bytes to write
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_write(struct lsm6dsl *lsm6dsl, uint8_t reg,
                  const uint8_t *buffer, uint8_t len);

/**
 * Write single byte to registers
 *
 * For configuration registers (that don't change by themselves) new value is only written
 * when change from locally stored cached value.
 *
 * @param lsm6dsl The device
 * @param reg    register address
 * @param val    new register value
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_write_reg(struct lsm6dsl *lsm6dsl, uint8_t reg, uint8_t val);

/**
 * Read number of data bytes from LSM6DSL sensor over interface
 *
 * @param lsm6dsl The device
 * @param reg    register address
 * @param buffer data buffer
 * @param len    number of bytes to read
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_read(struct lsm6dsl *lsm6dsl, uint8_t reg,
                 uint8_t *buffer, uint8_t len);

/**
 * Reads single byte from register
 *
 * Node: if register is cached no read is performed.
 *
 * @param lsm6dsl The device
 * @param reg    register address
 * @param val    buffer for return value
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_read_reg(struct lsm6dsl *lsm6dsl, uint8_t reg, uint8_t *val);

/**
 * Sets gyro full scale selection
 *
 * @param lsm6dsl The device
 * @param fs      full scale setting
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_gyro_full_scale(struct lsm6dsl *lsm6dsl, uint8_t fs);

/**
 * Sets accelerometer full scale selection
 *
 * @param lsm6dsl The device
 * @param The fs setting
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_acc_full_scale(struct lsm6dsl *lsm6dsl, uint8_t fs);

/**
 * Sets accelerometer rate
 *
 * @param lsm6dsl The device
 * @param rate data rate for accelerometer
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_acc_rate(struct lsm6dsl *lsm6dsl, acc_data_rate_t rate);

/**
 * Sets gyro rate
 *
 * @param lsm6dsl The device
 * @param rate data rage for gyro
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_gyro_rate(struct lsm6dsl *lsm6dsl, gyro_data_rate_t rate);

/**
 * Set FIFO mode
 *
 * @param lsm6dsl The device
 * @param mode
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_fifo_mode(struct lsm6dsl *lsm6dsl, lsm6dsl_fifo_mode_t mode);

/**
 * Set FIFO watermark
 *
 * @param lsm6dsl The device
 * @param wtm water mark for FIFO
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_fifo_watermark(struct lsm6dsl *lsm6dsl, uint16_t wtm);

/**
 * Get Number of Samples in FIFO
 *
 * @param lsm6dsl The device
 * @param Pointer to return number of samples in FIFO, 0 empty, 2048 for full
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_get_fifo_samples(struct lsm6dsl *lsm6dsl, uint16_t *samples);

/**
 * Get FIFO pattern
 *
 * @param lsm6dsl The device
 * @param Pointer to return number of samples in FIFO, 0 empty, 2048 for full
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_get_fifo_pattern(struct lsm6dsl *lsm6dsl, uint16_t *pattern);

/**
 * Sets accelerometer sensor user offsets
 *
 * Offset weight is 2^(-10) g/LSB or 2^(-6) g/LSB
 *
 * @param lsm6dsl The device
 * @param offset_x X offset
 * @param offset_y Y offset
 * @param offset_z Z offset
 * @param weight   LSB wight of offset values
 *
 * @return 0 on success, non-zero error on failure.
 */
int lsm6dsl_set_offsets(struct lsm6dsl *lsm6dsl, int8_t offset_x, int8_t offset_y,
                        int8_t offset_z, user_off_weight_t weight);

/**
 * Gets accelerometer sensor user offsets
 *
 * @param lsm6dsl The device
 * @param offset_x pointer to location to store X offset
 * @param offset_y pointer to location to store Y offset
 * @param offset_z pointer to location to store Z offset
 * @param weight   pointer to location to store weight
 *
 * @return 0 on success, non-zero error on failure.
 */
int lsm6dsl_get_offsets(struct lsm6dsl *lsm6dsl, int8_t *offset_x,
                        int8_t *offset_y, int8_t *offset_z, user_off_weight_t *weight);

/**
 * Sets push-pull/open-drain on INT1 and INT2 pins
 *
 * @param lsm6dsl The device
 * @param open_drain true INT pin is open drain, false push-pull
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_int_pp_od(struct lsm6dsl *lsm6dsl, bool open_drain);

/**
 * Gets push-pull/open-drain on INT1 and INT2 pins
 *
 * @param lsm6dsl The device
 * @param open_drain buffer for result
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_get_int_pp_od(struct lsm6dsl *lsm6dsl, bool *open_drain);

/**
 * Sets whether latched interrupts are enabled
 *
 * @param lsm6dsl The device
 * @param value to set (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_latched_int(struct lsm6dsl *lsm6dsl, bool en);

/**
 * Gets whether latched interrupts are enabled
 *
 * @param lsm6dsl The device
 * @param ptr to store value (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_get_latched_int(struct lsm6dsl *lsm6dsl, uint8_t *en);

/**
 * Redirects int2 to int1
 *
 * @param lsm6dsl The device
 * @param value to set (0 = int2 is separate, 1 = int2 activates int1 pin)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_map_int2_to_int1(struct lsm6dsl *lsm6dsl, bool en);

/**
 * Gets whether latched interrupts are enabled
 *
 * @param lsm6dsl The device
 * @param ptr to store value (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_get_map_int2_to_int1(struct lsm6dsl *lsm6dsl, uint8_t *en);

/**
 * Sets whether interrupts are active high or low
 *
 * @param lsm6dsl The device
 * @param level value to set (1 = active high, 0 = active low)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_int_level(struct lsm6dsl *lsm6dsl, uint8_t level);

/**
 * Gets whether interrupts are active high or low
 *
 * @param lsm6dsl The device
 * @param ptr to store value (1 = active high, 0 = active low)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_get_int_level(struct lsm6dsl *lsm6dsl, uint8_t *level);

/**
 * Clear interrupt pin configuration for interrupt
 *
 * @param lsm6dsl The device
 * @param interrupt pin
 * @param interrupt mask
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_clear_int_pin_cfg(struct lsm6dsl *lsm6dsl, uint8_t int_pin,
                              uint8_t int_mask);

/**
 * Clear all interrupts by reading all four interrupt registers status
 *
 * @param itf The sensor interface
 * @param src Ptr to return 4 interrupt sources in
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_clear_int(struct lsm6dsl *lsm6dsl, struct int_src_regs *int_src);

/**
 * Set interrupt pin configuration for interrupt
 *
 * @param lsm6dsl The device
 * @param interrupt pin
 * @param interrupt mask
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_int_pin_cfg(struct lsm6dsl *lsm6dsl, uint8_t int_pin,
                            uint8_t int_mask);

/**
 * Set orientation configuration
 *
 * @param lsm6dsl The device
 * @param cfg orientation settings to set
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_orientation(struct lsm6dsl *lsm6dsl,
                            const struct lsm6dsl_orientation_settings *cfg);

/**
 * Get orientation configuration
 *
 * @param lsm6dsl The device
 * @param cfg orientation settings to fill
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_get_orientation_cfg(struct lsm6dsl *lsm6dsl,
                                struct lsm6dsl_orientation_settings *cfg);

/**
 * Set tap detection configuration
 *
 * @param lsm6dsl The device
 * @param cfg new tap settings
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_tap_cfg(struct lsm6dsl *lsm6dsl,
                        const struct lsm6dsl_tap_settings *cfg);

/**
 * Get tap detection config
 *
 * @param lsm6dsl The device
 * @param ptr to the tap settings
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_get_tap_cfg(struct lsm6dsl *lsm6dsl,
                        struct lsm6dsl_tap_settings *cfg);

/**
 * Set free-fall detection configuration
 *
 * @param lsm6dsl The device
 * @param ff free fall settings
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_free_fall(struct lsm6dsl *lsm6dsl, struct lsm6dsl_ff_settings *ff);

/**
 * Get free-fall detection config
 *
 * @param lsm6dsl The device
 * @param ff ptr to free-fall configuration
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_get_free_fall(struct lsm6dsl *lsm6dsl, struct lsm6dsl_ff_settings *ff);

/**
 * Set Wake Up Duration
 *
 * @param lsm6dsl The device
 * @param wk configuration to set
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_set_wake_up(struct lsm6dsl *lsm6dsl, struct lsm6dsl_wk_settings *wk);

/**
 * Get Wake Up Duration
 *
 * @param lsm6dsl The device
 * @param ptr to wake up configuration
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dsl_get_wake_up(struct lsm6dsl *lsm6dsl, struct lsm6dsl_wk_settings *wk);

#ifdef __cplusplus
}
#endif

#endif /* __LSM6DSL_H__ */
