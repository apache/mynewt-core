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

struct bno055_cfg {
    uint8_t bc_mode;
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
 * Gets system status, test results and errors if any from the sensor
 *
 * @param ptr to system status
 * @param ptr to self test result
 * @param ptr to system error
 */
int
bno055_get_sys_status(uint8_t *system_status, uint8_t *self_test_result,
                      uint8_t *system_error);

int
bno055_get_chip_id(uint8_t *id);

/**
 * Read current operational mode of the sensor
 *
 * @return mode
 */
uint8_t
bno055_get_opr_mode(void);

/**
 * Read current power mode of the sensor
 *
 * @return mode
 */
uint8_t
bno055_get_pwr_mode(void);

/**
 * Set power mode for the sensor
 *
 * @return mode
 */
int
bno055_set_pwr_mode(uint8_t mode);

/**
 * Get power mode for the sensor
 *
 * @return mode
 */
uint8_t
bno055_get_pwr_mode(void);

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
 * @return mode
 */
uint8_t
bno055_get_units(void);

#ifdef __cplusplus
}
#endif

#endif /* __BNO055_H__ */
