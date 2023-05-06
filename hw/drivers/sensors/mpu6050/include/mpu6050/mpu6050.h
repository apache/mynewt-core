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

#include "os/mynewt.h"
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

enum mpu6050_clock_select {
    MPU6050_CLK_8MHZ_INTERNAL        = 0x00, /* Internal 8MHz oscillator */
    MPU6050_CLK_GYRO_X               = 0x01, /* PLL with X axis gyroscope reference  */
    MPU6050_CLK_GYRO_Y               = 0x02, /* PLL with Y axis gyroscope reference */
    MPU6050_CLK_GYRO_Z               = 0x03, /* PLL with Z axis gyroscope reference */
    MPU6050_CLK_EXTERNAL_LS          = 0x04, /* PLL with external 32.768kHz reference */
    MPU6050_CLK_EXTERNAL_HS          = 0x05, /* PLL with external 19.2MHz reference */
};

enum mpu6050_dlfp_cfg {
    /* Acc(Bandwith, Delay) Gyr(Bandwith, Delay) Sampling Frequency */
    MPU6050_DLPF_0 = 0x00, /* Acc(260Hz, 0ms)    Gyr(256Hz, 0.98ms) Fs=8kHz */
    MPU6050_DLPF_1 = 0x01, /* Acc(184Hz, 2.0ms)  Gyr(188Hz, 1.9ms)  Fs=1kHz */
    MPU6050_DLPF_2 = 0x02, /* Acc(94Hz,  3.0ms)  Gyr(98Hz,  2.8ms)  Fs=1kHz */
    MPU6050_DLPF_3 = 0x03, /* Acc(44Hz,  4.9ms)  Gyr(42Hz,  4.8ms)  Fs=1kHz */
    MPU6050_DLPF_4 = 0x04, /* Acc(21Hz,  8.5ms)  Gyr(20Hz,  8.3ms)  Fs=1kHz */
    MPU6050_DLPF_5 = 0x05, /* Acc(10Hz,  13.8ms) Gyr(10Hz,  13.4ms) Fs=1kHz */
    MPU6050_DLPF_6 = 0x06, /* Acc(5Hz,   19.0ms) Gyr(5Hz,   18.6ms) Fs=1kHz */
};

#define MPU6050_I2C_ADDR (0xD0 >> 1)

#define MPU6050_INT_LEVEL (0x80)
#define MPU6050_INT_OPEN (0x40)
#define MPU6050_INT_LATCH_EN (0x20)
#define MPU6050_INT_RD_CLEAR (0x10)

struct mpu6050_cfg {
    enum mpu6050_accel_range accel_range;
    enum mpu6050_gyro_range gyro_range;
    enum mpu6050_clock_select clock_source;
    uint8_t sample_rate_div; /* Sample Rate = Gyroscope Output Rate /
            (1 + sample_rate_div) */
    uint8_t lpf_cfg;            /* See mpu6050_dlfp_cfg and data sheet */
    uint8_t int_enable;
    uint8_t int_cfg;
    sensor_type_t mask;
    int16_t accel_offset[3];    /* x, y and z accelerometer offsets */
    int16_t gyro_offset[3];     /* x, y and z gyroscope offsets */
};

struct mpu6050 {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_i2c_node i2c_node;
#else
    struct os_dev dev;
#endif
    struct sensor sensor;
    struct mpu6050_cfg cfg;
    os_time_t last_read_time;
};

int mpu6050_reset(struct sensor_itf *itf);
int mpu6050_sleep(struct sensor_itf *itf, uint8_t enable);
int mpu6050_set_clock_source(struct sensor_itf *itf,
    enum mpu6050_clock_select source);
int mpu6050_get_clock_source(struct sensor_itf *itf,
    enum mpu6050_clock_select *source);
int mpu6050_set_lpf(struct sensor_itf *itf, uint8_t cfg);
int mpu6050_get_lpf(struct sensor_itf *itf, uint8_t *cfg);
int mpu6050_set_sample_rate(struct sensor_itf *itf, uint8_t rate_div);
int mpu6050_get_sample_rate(struct sensor_itf *itf, uint8_t *rate_div);
int mpu6050_set_gyro_range(struct sensor_itf *itf,
    enum mpu6050_gyro_range range);
int mpu6050_get_gyro_range(struct sensor_itf *itf,
    enum mpu6050_gyro_range *range);
int mpu6050_set_accel_range(struct sensor_itf *itf,
    enum mpu6050_accel_range range);
int mpu6050_get_accel_range(struct sensor_itf *itf,
    enum mpu6050_accel_range *range);
int mpu6050_enable_interrupt(struct sensor_itf *itf, uint8_t enable);
int mpu6050_config_interrupt(struct sensor_itf *itf, uint8_t cfg);
int mpu6050_set_x_accel_offset(struct sensor_itf *itf, int16_t offset);
int mpu6050_get_x_accel_offset(struct sensor_itf *itf, int16_t *offset);
int mpu6050_set_y_accel_offset(struct sensor_itf *itf, int16_t offset);
int mpu6050_get_y_accel_offset(struct sensor_itf *itf, int16_t *offset);
int mpu6050_set_z_accel_offset(struct sensor_itf *itf, int16_t offset);
int mpu6050_get_z_accel_offset(struct sensor_itf *itf, int16_t *offset);
int mpu6050_set_x_gyro_offset(struct sensor_itf *itf, int16_t offset);
int mpu6050_get_x_gyro_offset(struct sensor_itf *itf, int16_t *offset);
int mpu6050_set_y_gyro_offset(struct sensor_itf *itf, int16_t offset);
int mpu6050_get_y_gyro_offset(struct sensor_itf *itf, int16_t *offset);
int mpu6050_set_z_gyro_offset(struct sensor_itf *itf, int16_t offset);
int mpu6050_get_z_gyro_offset(struct sensor_itf *itf, int16_t *offset);
int mpu6050_set_offsets(struct mpu6050 *mpu, struct mpu6050_cfg *cfg);

int mpu6050_get_accel(struct mpu6050 *mpu6050, struct sensor_accel_data *sad);
int mpu6050_get_gyro(struct mpu6050 *mpu6050, struct sensor_gyro_data *sgd);

int mpu6050_init(struct os_dev *, void *);
int mpu6050_config(struct mpu6050 *, struct mpu6050_cfg *);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
 * Create I2C bus node for MPU6050 sensor
 *
 * @param node        Bus node
 * @param name        Device name
 * @param i2c_cfg     I2C node configuration
 * @param sensor_itf  Sensors interface
 *
 * @return 0 on success, non-zero on failure
 */
int
mpu6050_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                              const struct bus_i2c_node_cfg *i2c_cfg,
                              struct sensor_itf *sensor_itf);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_MPU6050_H__ */
