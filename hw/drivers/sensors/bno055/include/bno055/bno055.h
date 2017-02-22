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

#ifndef __BNO055_H__
#define __BNO055_H__

#include <os/os.h>
#include "os/os_dev.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Power modes */
#define BNO055_PWR_MODE_NORMAL                                  0X00
#define BNO055_PWR_MODE_LOWPOWER                                0X01
#define BNO055_PWR_MODE_SUSPEND                                 0X02

/* Operation Modes */
#define BNO055_OPR_MODE_CONFIG                                  0X00
#define BNO055_OPR_MODE_ACCONLY                                 0X01
#define BNO055_OPR_MODE_MAGONLY                                 0X02
#define BNO055_OPR_MODE_GYRONLY                                 0X03
#define BNO055_OPR_MODE_ACCMAG                                  0X04
#define BNO055_OPR_MODE_ACCGYRO                                 0X05
#define BNO055_OPR_MODE_MAGGYRO                                 0X06
#define BNO055_OPR_MODE_AMG                                     0X07
#define BNO055_OPR_MODE_IMUPLUS                                 0X08
#define BNO055_OPR_MODE_COMPASS                                 0X09
#define BNO055_OPR_MODE_M4G                                     0X0A
#define BNO055_OPR_MODE_NDOF_FMC_OFF                            0X0B
#define BNO055_OPR_MODE_NDOF                                    0X0C

#define BNO055_ACC_UNIT_MS2                                      0x0
#define BNO055_ACC_UNIT_MG                                       0x1
#define BNO055_ANGRATE_UNIT_DPS                             (0 << 1)
#define BNO055_ANGRATE_UNIT_RPS                             (1 << 1)
#define BNO055_EULER_UNIT_DEG                               (0 << 2)
#define BNO055_EULER_UNIT_RAD                               (1 << 2)
#define BNO055_TEMP_UNIT_DEGC                               (0 << 4)
#define BNO055_TEMP_UNIT_DEGF                               (1 << 4)
#define BNO055_DO_FORMAT_WINDOWS                            (0 << 7)
#define BNO055_DO_FORMAT_ANDROID                            (1 << 7)

/* Accelerometer config */
#define BNO055_ACC_CFG_2G                                        0x0
#define BNO055_ACC_CFG_4G                                        0x1
#define BNO055_ACC_CFG_8G                                        0x2
#define BNO055_ACC_CFG_16G                                       0x3

#define BNO055_ACC_CFG_BW_7_81HZ                          (0x0 << 2)
#define BNO055_ACC_CFG_BW_15_63HZ                         (0x1 << 2)
#define BNO055_ACC_CFG_BW_31_25HZ                         (0x2 << 2)
#define BNO055_ACC_CFG_BW_6_25HZ                          (0x3 << 2)
#define BNO055_ACC_CFG_BW_125HZ                           (0x4 << 2)
#define BNO055_ACC_CFG_BW_250HZ                           (0x5 << 2)
#define BNO055_ACC_CFG_BW_500HZ                           (0x6 << 2)
#define BNO055_ACC_CFG_BW_1000HZ                          (0x7 << 2)

#define BNO055_ACC_CFG_OPR_MODE_NORMAL                    (0x0 << 5)
#define BNO055_ACC_CFG_OPR_MODE_SUSPEND                   (0x1 << 5)
#define BNO055_ACC_CFG_OPR_MODE_LOWPWR1                   (0x2 << 5)
#define BNO055_ACC_CFG_OPR_MODE_STD                       (0x3 << 5)
#define BNO055_ACC_CFG_OPR_MODE_LOWPWR2                   (0x4 << 5)
#define BNO055_ACC_CFG_OPR_MODE_DSUSPEND                  (0x5 << 5)

/* Gyroscope config */
#define BNO055_GYR_CFG_RNG_2000DPS                               0x0
#define BNO055_GYR_CFG_RNG_1000DPS                               0x1
#define BNO055_GYR_CFG_RNG_500DPS                                0x2
#define BNO055_GYR_CFG_RNG_250DPS                                0x3
#define BNO055_GYR_CFG_RNG_125DPS                                0x4

#define BNO055_GYR_CFG_BW_523HZ                           (0x0 << 3)
#define BNO055_GYR_CFG_BW_230HZ                           (0x1 << 3)
#define BNO055_GYR_CFG_BW_116HZ                           (0x2 << 3)
#define BNO055_GYR_CFG_BW_47HZ                            (0x3 << 3)
#define BNO055_GYR_CFG_BW_23HZ                            (0x4 << 3)
#define BNO055_GYR_CFG_BW_12HZ                            (0x5 << 3)
#define BNO055_GYR_CFG_BW_64HZ                            (0x6 << 3)
#define BNO055_GYR_CFG_BW_32HZ                            (0x7 << 3)

#define BNO055_GYR_CFG_OPR_MODE_NORMAL                    (0x0 << 5)
#define BNO055_GYR_CFG_OPR_MODE_FAST_PWR_UP               (0x1 << 5)
#define BNO055_GYR_CFG_OPR_MODE_DSUSPEND                  (0x2 << 5)
#define BNO055_GYR_CFG_OPR_MODE_SUSPEND                   (0x3 << 5)
#define BNO055_GYR_CFG_OPR_MODE_ADV_PWR_SAVE              (0x4 << 5)

/* Magnetometer config */
#define BNO055_MAG_CFG_BW_2HZ                                    0x0
#define BNO055_MAG_CFG_BW_6HZ                                    0x1
#define BNO055_MAG_CFG_BW_8HZ                                    0x2
#define BNO055_MAG_CFG_BW_10HZ                                   0x3
#define BNO055_MAG_CFG_BW_15HZ                                   0x4
#define BNO055_MAG_CFG_BW_20HZ                                   0x5
#define BNO055_MAG_CFG_BW_25HZ                                   0x6
#define BNO055_MAG_CFG_BW_30HZ                                   0x7

#define BNO055_MAG_CFG_OPR_MODE_LOWPWR                    (0x0 << 3)
#define BNO055_MAG_CFG_OPR_MODE_REG                       (0x1 << 3)
#define BNO055_MAG_CFG_OPR_MODE_EREG                      (0x2 << 3)
#define BNO055_MAG_CFG_OPR_MODE_HIGHACC                   (0x3 << 3)

#define BNO055_MAG_CFG_PWR_MODE_NORMAL                    (0x0 << 5)
#define BNO055_MAG_CFG_PWR_MODE_SLEEP                     (0x1 << 5)
#define BNO055_MAG_CFG_PWR_MODE_SUSPEND                   (0x2 << 5)
#define BNO055_MAG_CFG_PWR_MODE_FORCE_MODE                (0x3 << 5)


struct bno055_cfg {
    uint8_t bc_opr_mode;
    uint8_t bc_pwr_mode;
    uint8_t bc_units;
};

struct bno055 {
    struct os_dev dev;
    struct sensor sensor;
    struct bno055_cfg cfg;
    os_time_t last_read_time;
};

struct bno055_rev_info {
     uint8_t bri_accel_rev;
     uint8_t bri_mag_rev;
     uint8_t bri_gyro_rev;
     uint8_t bri_bl_rev;
     uint16_t bri_sw_rev;
};

/**
 * Initialize the bno055. This function is normally called by sysinit.
 *
 * @param dev  Pointer to the bno055_dev device descriptor
 */
int bno055_init(struct os_dev *dev, void *arg);

int bno055_config(struct bno055 *, struct bno055_cfg *);

int
bno055_get_vector_data(void *datastruct, int type);

/**
 * Get quat data from sensor
 *
 * @param sensor quat data to be filled up
 * @return 0 on success, non-zero on error
 */
int
bno055_get_quat_data(void *sqd);

/**
 * Get temperature from bno055 sensor
 *
 * @return temperature in degree celcius
 */
int
bno055_get_temp(int8_t *temp);

/**
 * Reads a single byte from the specified register
 *
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
bno055_read8(uint8_t reg, uint8_t *value);

/**
 * Writes a single byte to the specified register
 *
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
bno055_write8(uint8_t reg, uint8_t value);

/**
 * Set operation mode for the bno055 sensor
 *
 * @param Operation mode for the sensor
 * @return 0 on success, non-zero on failure
 */
int
bno055_set_opr_mode(uint8_t mode);

#if MYNEWT_VAL(BNO055_CLI)
int bno055_shell_init(void);
#endif

/**
 * Get Revision info for different sensors in the bno055
 *
 * @param pass the pointer to the revision structure
 * @return 0 on success, non-zero on error
 */
int
bno055_get_rev_info(struct bno055_rev_info *ri);

/**
 * Set power mode for the sensor
 *
 * @return mode
 */
int
bno055_set_pwr_mode(uint8_t mode);

/**
 * Setting units for the bno055 sensor
 *
 * @param power mode for the sensor
 * @return 0 on success, non-zero on failure
 */
int
bno055_set_units(uint8_t val);

/**
 * Read current power mode of the sensor
 *
 * @param ptr to mode variableto fill up
 * @return 0 on success, non-zero on failure
 */
int
bno055_get_pwr_mode(uint8_t *mode);

/**
 * Read current operational mode of the sensor
 *
 * @param ptr to mode variable to fill up
 * @return 0 on success, non-zero on failure
 */
int
bno055_get_opr_mode(uint8_t *mode);

/**
 * Get units of the sensor
 *
 * @param ptr to the units variable
 * @return 0 on success, non-zero on failure
 */
int
bno055_get_units(uint8_t *units);

/**
 * Gets system status, test results and errors if any from the sensor
 *
 * @param ptr to system status
 * @param ptr to self test result
 * @param ptr to system error
 */

int
bno055_get_sys_status(uint8_t *system_status, uint8_t *self_test_result,
                      uint8_t *system_error);

/**
 * Get chip ID from the sensor
 *
 * @param Pointer to the variable to fill up chip ID in
 * @return 0 on success, non-zero on failure
 */
int
bno055_get_chip_id(uint8_t *id);


#ifdef __cplusplus
}
#endif

#endif /* __BNO055_H__ */
