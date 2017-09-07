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

#ifndef __SENSOR_MPU6050_H__
#define __SENSOR_MPU6050_H__

#include "os/os.h"
#include "os/os_dev.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

enum mpu6050_gyro_range {
    MPU6050_GYRO_RANGE_250           = 0x00 << 3, /* +/- 250 Deg/s  */
    MPU6050_GYRO_RANGE_500           = 0x01 << 3, /* +/- 500 Deg/s  */
    MPU6050_GYRO_RANGE_1000          = 0x02 << 3, /* +/- 1000 Deg/s */
    MPU6050_GYRO_RANGE_2000          = 0x03 << 3, /* +/- 2000 Deg/s */
};

enum mpu6050_accel_range {
    MPU6050_ACCEL_RANGE_2            = 0x00 << 3, /* +/- 2g  */
    MPU6050_ACCEL_RANGE_4            = 0x01 << 3, /* +/- 4g  */
    MPU6050_ACCEL_RANGE_8            = 0x02 << 3, /* +/- 8g  */
    MPU6050_ACCEL_RANGE_16           = 0x03 << 3, /* +/- 16g */
};

#define MPU6050_I2C_ADDR (0xD0 >> 1)

struct mpu6050_cfg {
    enum mpu6050_accel_range accel_range;
    enum mpu6050_gyro_range gyro_range;
    uint8_t gyro_rate_div; /* Sample Rate = Gyroscope Output Rate /
            (1 + gyro_rate_div) */
    uint8_t lpf_cfg; /* See data sheet */
    sensor_type_t mask;
};

struct mpu6050 {
    struct os_dev dev;
    struct sensor sensor;
    struct mpu6050_cfg cfg;
    os_time_t last_read_time;
};

int mpu6050_init(struct os_dev *, void *);
int mpu6050_config(struct mpu6050 *, struct mpu6050_cfg *);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_MPU6050_H__ */
