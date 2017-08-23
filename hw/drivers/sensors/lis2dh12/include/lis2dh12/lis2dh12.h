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

#ifndef __LIS2DH12_H__
#define __LIS2DH12_H__

#include <os/os.h>
#include "os/os_dev.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LIS2DH12_OM_LOW_POWER                   0x80
#define LIS2DH12_OM_NORMAL                      0x00
#define LIS2DH12_OM_HIGH_RESOLUTION             0x08
#define LIS2DH12_OM_NOT_ALLOWED                 0x88

#define LIS2DH12_DATA_RATE_0HZ                  0x00
#define LIS2DH12_DATA_RATE_1HZ                  0x10
#define LIS2DH12_DATA_RATE_10HZ                 0x20
#define LIS2DH12_DATA_RATE_25HZ                 0x30
#define LIS2DH12_DATA_RATE_50HZ                 0x40
#define LIS2DH12_DATA_RATE_100HZ                0x50
#define LIS2DH12_DATA_RATE_200HZ                0x60
#define LIS2DH12_DATA_RATE_400HZ                0x70
#define LIS2DH12_DATA_RATE_L_1620HZ             0x80
#define LIS2DH12_DATA_RATE_HN_1344HZ_L_5376HZ   0x90

#define LIS2DH12_ST_MODE_DISABLE                0x00
#define LIS2DH12_ST_MODE_MODE1                  0x01
#define LIS2DH12_ST_MODE_MODE2                  0x02

#define LIS2DH12_HPF_M_NORMAL0                  0x00
#define LIS2DH12_HPF_M_REF                      0x01
#define LIS2DH12_HPF_M_NORMAL1                  0x02
#define LIS2DH12_HPF_M_AIE                      0x03

#define LIS2DH12_HPCF_0                         0x00
#define LIS2DH12_HPCF_1                         0x01
#define LIS2DH12_HPCF_2                         0x02
#define LIS2DH12_HPCF_3                         0x03

#define LIS2DH12_FIFO_M_BYPASS                  0x00
#define LIS2DH12_FIFO_M_FIFO                    0x01
#define LIS2DH12_FIFO_M_STREAM                  0x02
#define LIS2DH12_FIFO_M_STREAM_FIFO             0x03

#define LIS2DH12_INT1_CFG_M_OR                   0x0
#define LIS2DH12_INT1_CFG_M_6DM                  0x1
#define LIS2DH12_INT1_CFG_M_AND                  0x2
#define LIS2DH12_INT1_CFG_M_6PR                  0x3

#define LIS2DH12_INT2_CFG_M_OR                   0x0
#define LIS2DH12_INT2_CFG_M_6DM                  0x1
#define LIS2DH12_INT2_CFG_M_AND                  0x2
#define LIS2DH12_INT2_CFG_M_6PR                  0x3

#define LIS2DH12_FS_2G                          0x00
#define LIS2DH12_FS_4G                          0x10
#define LIS2DH12_FS_8G                          0x20
#define LIS2DH12_FS_16G                         0x30

struct lis2dh12_cfg {
    uint8_t lc_rate;
    uint8_t lc_fs;
    uint8_t lc_pull_up_disc;
    sensor_type_t lc_s_mask;
};

struct lis2dh12 {
    struct os_dev dev;
    struct sensor sensor;
    struct lis2dh12_cfg cfg;
    os_time_t last_read_time;
};

/**
 * Expects to be called back through os_dev_create().
 *
 * @param ptr to the device object associated with this accelerometer
 * @param argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int lis2dh12_init(struct os_dev *dev, void *arg);

/**
 * Sets the full scale selection
 *
 * @param The sensor interface
 * @param The rate
 *
 * @return 0 on success, non-zero on failure
 */

int
lis2dh12_set_full_scale(struct sensor_itf *itf, uint8_t fs);

/**
 * Gets the full scale selection
 *
 * @param The sensor interface
 * @param ptr to fs
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_get_full_scale(struct sensor_itf *itf, uint8_t *fs);

/**
 * Sets the rate
 *
 * @param The sensor interface
 * @param The rate
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dh12_set_rate(struct sensor_itf *itf, uint8_t rate);

/**
 * Gets the current rate
 *
 * @param The sensor interface
 * @param ptr to rate read from the sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dh12_get_rate(struct sensor_itf *itf, uint8_t *rate);

/**
 * Enable channels
 *
 * @param sensor interface
 * @param chan
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_chan_enable(struct sensor_itf *itf, uint8_t chan);

/**
 * Get chip ID
 *
 * @param sensor interface
 * @param ptr to chip id to be filled up
 */
int
lis2dh12_get_chip_id(struct sensor_itf *itf, uint8_t *chip_id);

/**
 * Configure the sensor
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 */
int lis2dh12_config(struct lis2dh12 *, struct lis2dh12_cfg *);

/**
 * Calculates the acceleration in m/s^2 from mg
 *
 * @param raw acc value
 * @param float ptr to return calculated value
 * @param scale to calculate against
 */
void
lis2dh12_calc_acc_ms2(int16_t raw_acc, float *facc);

/**
 * Pull up disconnect
 *
 * @param The sensor interface
 * @param disconnect pull up
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_pull_up_disc(struct sensor_itf *itf, uint8_t disconnect);

/**
 * Reset lis2dh12
 *
 * @param The sensor interface
 */
int
lis2dh12_reset(struct sensor_itf *itf);

/**
 * Enable interrupt 2
 *
 * @param the sensor interface
 * @param events to enable int for
 */
int
lis2dh12_enable_int2(struct sensor_itf *itf, uint8_t *reg);

/**
 * Enable interrupt 1
 *
 * @param the sensor interface
 * @param events to enable int for
 */
int
lis2dh12_enable_int1(struct sensor_itf *itf, uint8_t *reg);

/**
 * Set interrupt threshold for int 1
 *
 * @param the sensor interface
 * @param threshold
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int1_thresh(struct sensor_itf *itf, uint8_t ths);

/**
 * Set interrupt threshold for int 2
 *
 * @param the sensor interface
 * @param threshold
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int2_thresh(struct sensor_itf *itf, uint8_t ths);

/**
 * Clear interrupt 1
 *
 * @param the sensor interface
 */
int
lis2dh12_clear_int1(struct sensor_itf *itf);

/**
 * Clear interrupt 2
 *
 * @param the sensor interface
 */
int
lis2dh12_clear_int2(struct sensor_itf *itf);

/**
 * Set interrupt pin configuration for interrupt 1
 *
 * @param the sensor interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int1_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set interrupt pin configuration for interrupt 2
 *
 * @param the sensor interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int2_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set interrupt 1 duration
 *
 * @param duration in N/ODR units
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int1_duration(struct sensor_itf *itf, uint8_t dur);

/**
 * Set interrupt 2 duration
 *
 * @param duration in N/ODR units
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int2_duration(struct sensor_itf *itf, uint8_t dur);

/**
 * Set high pass filter cfg
 *
 * @param the sensor interface
 * @param filter register settings
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_hpf_cfg(struct sensor_itf *itf, uint8_t reg);

#ifdef __cplusplus
}
#endif

#endif /* __LIS2DH12_H__ */
