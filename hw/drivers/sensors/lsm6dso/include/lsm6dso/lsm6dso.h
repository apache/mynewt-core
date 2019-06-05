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

#ifndef __LSM6DSO_H__
#define __LSM6DSO_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#include "bus/drivers/spi_common.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Threshold for D4D/D6D function */
enum lsm6dso_ths_6d {
    LSM6DSO_6D_THS_80_DEG = 0,
    LSM6DSO_6D_THS_70_DEG = 1,
    LSM6DSO_6D_THS_60_DEG = 2,
    LSM6DSO_6D_THS_50_DEG = 3
};

/* Sensors read mode */
enum lsm6dso_read_mode {
    LSM6DSO_READ_POLL = 0,
    LSM6DSO_READ_STREAM = 1,
};

struct lsm6dso_int {
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

struct lsm6dso_orientation_settings {
    uint8_t en_4d : 1;

    /* 6D/4D angle threshold */
    enum lsm6dso_ths_6d ths_6d;
};

struct lsm6dso_wk_settings {
    /* sleep/wakeup configuration parameters  */
    uint8_t wake_up_ths;
    uint8_t wake_up_dur;
    uint8_t sleep_duration;
    uint8_t hpf_slope;
};

struct lsm6dso_ff_settings {
    /* Freefall configuration parameters */
    uint8_t freefall_dur;
    uint8_t freefall_ths;
};

struct lsm6dso_tap_settings {
    /* Axis enabled bitmask */
    uint8_t en_x  : 1;
    uint8_t en_y  : 1;
    uint8_t en_z  : 1;
    uint8_t en_dtap : 1;
    uint8_t tap_prio : 3;

    /* Threshold for tap recognition */
    int8_t tap_ths;

    /* Time duration of maximum time gap for double tap recognition */
    uint8_t dur;

    /* Expected quiet time after a tap detection */
    uint8_t quiet;

    /* Maximum duration of overthreshold event */
    uint8_t shock;
};

struct lsm6dso_notif_cfg {
    /* Interrupt event registered */
    sensor_event_type_t event;

    /* Interrupt pin number (0/1) */
    uint8_t int_num:1;

    /* Interrupt bit mask */
    uint8_t int_mask;

    /* Int enable bit */
    uint8_t int_en;
};

/* Read mode configuration */
struct lsm6dso_read_mode_cfg {
    enum lsm6dso_read_mode mode;
    uint8_t int_num:1;
    uint8_t int_cfg;
    uint8_t int_reg;
};

enum lsm6dso_fifo_mode {
    /* Supported FIFO mode by driver */
    LSM6DSO_FIFO_MODE_BYPASS_VAL = 0x00,
    LSM6DSO_FIFO_MODE_CONTINUOUS_VAL = 0x06
};

struct lsm6dso_fifo_cfg {
    enum lsm6dso_fifo_mode mode;
    uint16_t wtm;
};

struct lsm6dso_cfg {
    uint8_t acc_fs;
    uint8_t acc_rate;
    int acc_sensitivity;
    uint8_t gyro_fs;
    uint8_t gyro_rate;
    int gyro_sensitivity;

    struct lsm6dso_tap_settings tap;
    struct lsm6dso_orientation_settings orientation;
    struct lsm6dso_wk_settings wk;
    struct lsm6dso_ff_settings ff;

    /* Event notification config */
    struct lsm6dso_notif_cfg *notif_cfg;
    uint8_t max_num_notif;

     /* Read mode config */
    struct lsm6dso_read_mode_cfg read;

     /* FIFO configuration */
    struct lsm6dso_fifo_cfg fifo;

    /* Pin interrupt config */
    uint8_t int1_pin_cfg;
    uint8_t int2_pin_cfg;
    bool map_int2_to_int1;

    /* The sensors mask */
    sensor_type_t lc_s_mask;
};

/* Private per driver data */
struct lsm6dso_pdd {
    /* Notification event context */
    struct sensor_notify_ev_ctx notify_ctx;

     /* Inetrrupt state */
    struct lsm6dso_int *interrupt;

    /* Interrupt enabled flag */
    uint16_t int_enable;
};

struct lsm6dso {
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
    struct lsm6dso_cfg cfg;
    struct lsm6dso_int intr;
    struct lsm6dso_pdd pdd;
};

/* Angular rate sensor self-test mode selection */
#define LSM6DSO_NORMAL_MODE_G_ST_VAL	0x00
#define LSM6DSO_POSITIVE_SIGN_G_ST_VAL	0x01
#define LSM6DSO_NEGATIVE_SIGN_G_ST_VAL	0x03

/* Linear acceleration sensor self-test mode selection */
#define LSM6DSO_NORMAL_MODE_XL_ST_VAL	0x00
#define LSM6DSO_POSITIVE_SIGN_XL_ST_VAL	0x01
#define LSM6DSO_NEGATIVE_SIGN_XL_ST_VAL	0x02

/* Accelerometer bandwidth configurations */
#define LSM6DSO_BW_LP_XL_ODR_2_VAL	0x00
#define LSM6DSO_BW_LP_XL_ODR_4_VAL	0x00
#define LSM6DSO_BW_LP_XL_ODR_10_VAL	0x01
#define LSM6DSO_BW_LP_XL_ODR_20_VAL	0x02
#define LSM6DSO_BW_LP_XL_ODR_45_VAL	0x03
#define LSM6DSO_BW_LP_XL_ODR_100_VAL	0x04
#define LSM6DSO_BW_LP_XL_ODR_200_VAL	0x05
#define LSM6DSO_BW_LP_XL_ODR_400_VAL	0x06
#define LSM6DSO_BW_LP_XL_ODR_800_VAL	0x07

#define LSM6DSO_BW_HP_XL_SLOPE_VAL	0x00
#define LSM6DSO_BW_HP_XL_ODR_10_VAL	0x01
#define LSM6DSO_BW_HP_XL_ODR_20_VAL	0x02
#define LSM6DSO_BW_HP_XL_ODR_45_VAL	0x03
#define LSM6DSO_BW_HP_XL_ODR_100_VAL	0x04
#define LSM6DSO_BW_HP_XL_ODR_200_VAL	0x05
#define LSM6DSO_BW_HP_XL_ODR_400_VAL	0x06
#define LSM6DSO_BW_HP_XL_ODR_800_VAL	0x07

/* TAP priority decoding */
#define LSM6DSO_TAP_PRIO_XYZ_VAL	0x00
#define LSM6DSO_TAP_PRIO_YXZ_VAL	0x01
#define LSM6DSO_TAP_PRIO_XZY_VAL	0x02
#define LSM6DSO_TAP_PRIO_ZYX_VAL	0x03

/* Accelerometer data rate */
#define LSM6DSO_ACCEL_OFF_VAL		0x00
#define LSM6DSO_ACCEL_12_5HZ_VAL	0x01
#define LSM6DSO_ACCEL_26HZ_VAL		0x02
#define LSM6DSO_ACCEL_52HZ_VAL		0x03
#define LSM6DSO_ACCEL_104HZ_VAL		0x04
#define LSM6DSO_ACCEL_208HZ_VAL		0x05
#define LSM6DSO_ACCEL_416HZ_VAL		0x06
#define LSM6DSO_ACCEL_833HZ_VAL		0x07
#define LSM6DSO_ACCEL_1666HZ_VAL	0x08
#define LSM6DSO_ACCEL_3333HZ_VAL	0x09
#define LSM6DSO_ACCEL_6666HZ_VAL	0x0a

/* Gyroscope data rate */
#define LSM6DSO_GYRO_OFF_VAL		0x00
#define LSM6DSO_GYRO_12_5HZ_VAL		0x01
#define LSM6DSO_GYRO_26HZ_VAL		0x02
#define LSM6DSO_GYRO_52HZ_VAL		0x03
#define LSM6DSO_GYRO_104HZ_VAL		0x04
#define LSM6DSO_GYRO_208HZ_VAL		0x05
#define LSM6DSO_GYRO_416HZ_VAL		0x06
#define LSM6DSO_GYRO_833HZ_VAL		0x07
#define LSM6DSO_GYRO_1666HZ_VAL		0x08
#define LSM6DSO_GYRO_3333HZ_VAL		0x09
#define LSM6DSO_GYRO_6666HZ_VAL		0x0a

/* Accelerometer full scale range in G */
#define LSM6DSO_ACCEL_FS_2G_VAL		0x00
#define LSM6DSO_ACCEL_FS_4G_VAL		0x02
#define LSM6DSO_ACCEL_FS_8G_VAL		0x03
#define LSM6DSO_ACCEL_FS_16G_VAL	0x01

#define LSM6DSO_ACCEL_FS_MIN_VAL	2
#define LSM6DSO_ACCEL_FS_MAX_VAL	16

/* Gyroscope full scale range in DPS */
#define LSM6DSO_GYRO_FS_250DPS_VAL	0x00
#define LSM6DSO_GYRO_FS_500DPS_VAL	0x01
#define LSM6DSO_GYRO_FS_1000DPS_VAL	0x02
#define LSM6DSO_GYRO_FS_2000DPS_VAL	0x03

#define LSM6DSO_GYRO_FS_MIN_VAL		250
#define LSM6DSO_GYRO_FS_MAX_VAL		2000

/* Threshold for Free Fall detection */
#define LSM6DSO_FF_THS_156_MG_VAL	0x00
#define LSM6DSO_FF_THS_219_MG_VAL	0x01
#define LSM6DSO_FF_THS_250_MG_VAL	0x02
#define LSM6DSO_FF_THS_312_MG_VAL	0x03
#define LSM6DSO_FF_THS_344_MG_VAL	0x04
#define LSM6DSO_FF_THS_406_MG_VAL	0x05
#define LSM6DSO_FF_THS_469_MG_VAL	0x06
#define LSM6DSO_FF_THS_500_MG_VAL	0x07

/* Interrupt notification mask */
#define LSM6DSO_INT_FF                  0x01
#define LSM6DSO_INT_WU                  0x02
#define LSM6DSO_INT_SINGLE_TAP          0x04
#define LSM6DSO_INT_DOUBLE_TAP          0x08
#define LSM6DSO_INT_6D                  0x10
#define LSM6DSO_INT_SLEEP_CHANGE        0x20

/**
 * Expects to be called back through os_dev_create().
 *
 * @param ptr to the device object associated with this accelerometer
 * @param argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int lsm6dso_init(struct os_dev *dev, void *arg);

/**
 * Configure the sensor
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 */
int lsm6dso_config(struct lsm6dso *, struct lsm6dso_cfg *);

/**
 * Init shell
 *
 * @return 0 on success, non-zero on failure.
 */
int lsm6dso_shell_init(void);

/**
 * Read acc/gyro data
 *
 * @param itf interface pointer
 * @param type sensor type
 * @param data data buffer
 * @param cfg configuration data pointer
 *
 * @return 0 on success, non-zero on failure.
 */
int
lsm6dso_get_ag_data(struct sensor_itf *itf, sensor_type_t type, void *data,
                    struct lsm6dso_cfg *cfg);

/**
 * Run Self test on sensor
 *
 * @param the sensor interface
 * @param pointer to return test result in
 *        0 on pass
 *        1 on XL failure
 *        2 on Gyro failure
 *
 * @return 0 on sucess, non-zero on failure
 */
int lsm6dso_run_self_test(struct sensor_itf *itf, int *result);

#ifdef __cplusplus
}
#endif

#endif /* __LSM6DSO_H__ */
