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

#ifndef __SENSOR_LSM303DLHC_H__
#define __SENSOR_LSM303DLHC_H__

#include "os/os.h"
#include "os/os_dev.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

enum lsm303dlhc_accel_range {
    LSM303DLHC_ACCEL_RANGE_2            = 0x00 << 4, /* +/- 2g  */
    LSM303DLHC_ACCEL_RANGE_4            = 0x01 << 4, /* +/- 4g  */
    LSM303DLHC_ACCEL_RANGE_8            = 0x02 << 4, /* +/- 8g  */
    LSM303DLHC_ACCEL_RANGE_16           = 0x03 << 4, /* +/- 16g */
};

enum lsm303dlhc_accel_rate {
    LSM303DLHC_ACCEL_RATE_POWER_DOWN    = 0x00 << 4,
    LSM303DLHC_ACCEL_RATE_1             = 0x01 << 4,
    LSM303DLHC_ACCEL_RATE_10            = 0x02 << 4,
    LSM303DLHC_ACCEL_RATE_25            = 0x03 << 4,
    LSM303DLHC_ACCEL_RATE_50            = 0x04 << 4,
    LSM303DLHC_ACCEL_RATE_100           = 0x05 << 4,
    LSM303DLHC_ACCEL_RATE_200           = 0x06 << 4,
    LSM303DLHC_ACCEL_RATE_400           = 0x07 << 4,
    LSM303DLHC_ACCEL_RATE_1620          = 0x08 << 4
};

enum lsm303dlhc_mag_gain {
    LSM303DLHC_MAG_GAIN_1_3             = 0x20 << 5, /* +/- 1.3 gauss */
    LSM303DLHC_MAG_GAIN_1_9             = 0x40 << 5, /* +/- 1.9 gauss */
    LSM303DLHC_MAG_GAIN_2_5             = 0x60 << 5, /* +/- 2.5 gauss */
    LSM303DLHC_MAG_GAIN_4_0             = 0x80 << 5, /* +/- 4.0 gauss */
    LSM303DLHC_MAG_GAIN_4_7             = 0xA0 << 5, /* +/- 4.7 gauss */
    LSM303DLHC_MAG_GAIN_5_6             = 0xC0 << 5, /* +/- 5.6 gauss */
    LSM303DLHC_MAG_GAIN_8_1             = 0xE0 << 5  /* +/- 8.1 gauss */
};

enum lsm303dlhc_mag_rate {
    LSM303DLHC_MAG_RATE_0_7             = 0x00 << 2,  /* 0.75 Hz */
    LSM303DLHC_MAG_RATE_1_5             = 0x01 << 2,  /* 1.5 Hz  */
    LSM303DLHC_MAG_RATE_3_0             = 0x02 << 2,  /* 3.0 Hz  */
    LSM303DLHC_MAG_RATE_7_5             = 0x03 << 2,  /* 7.5 Hz  */
    LSM303DLHC_MAG_RATE_15              = 0x04 << 2,  /* 15 Hz   */
    LSM303DLHC_MAG_RATE_30              = 0x05 << 2,  /* 30 Hz   */
    LSM303DLHC_MAG_RATE_75              = 0x06 << 2,  /* 75 Hz   */
    LSM303DLHC_MAG_RATE_220             = 0x07 << 2   /* 220 Hz  */
};

#define LSM303DLHC_ADDR_ACCEL                  0x19  /* 0011001 */
#define LSM303DLHC_ADDR_MAG                    0x1E  /* 0011110 */

struct lsm303dlhc_cfg {
    enum lsm303dlhc_accel_range accel_range;
    enum lsm303dlhc_accel_rate accel_rate;
    enum lsm303dlhc_mag_gain mag_gain;
    enum lsm303dlhc_mag_rate mag_rate;
    uint8_t mag_addr;
    uint8_t acc_addr;
    sensor_type_t mask;
};

struct lsm303dlhc {
    struct os_dev dev;
    struct sensor sensor;
    struct lsm303dlhc_cfg cfg;
    os_time_t last_read_time;
};

int lsm303dlhc_init(struct os_dev *, void *);
int lsm303dlhc_config(struct lsm303dlhc *, struct lsm303dlhc_cfg *);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_LSM303DLHC_H__ */
