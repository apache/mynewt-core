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


#ifndef __BME280_H__
#define __BME280_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#define BME280_SPI_READ_CMD_BIT 0x80

/* Sampling */
#define BME280_SAMPLING_NONE     0x0
#define BME280_SAMPLING_X1       0x1
#define BME280_SAMPLING_X2       0x2
#define BME280_SAMPLING_X4       0x3
#define BME280_SAMPLING_X8       0x4
#define BME280_SAMPLING_X16      0x5

/* Operating modes */
#define BME280_MODE_SLEEP        0x0
#define BME280_MODE_FORCED       0x1
#define BME280_MODE_NORMAL       0x3

/* Filter settings */
#define BME280_FILTER_OFF        0x0
#define BME280_FILTER_X2         0x1
#define BME280_FILTER_X4         0x2
#define BME280_FILTER_X8         0x3
#define BME280_FILTER_X16        0x4

/* Standby durations in ms */
#define BME280_STANDBY_MS_0_5    0x0
#define BME280_STANDBY_MS_10     0x6
#define BME280_STANDBY_MS_20     0x7
#define BME280_STANDBY_MS_62_5   0x1
#define BME280_STANDBY_MS_125    0x2
#define BME280_STANDBY_MS_250    0x3
#define BME280_STANDBY_MS_500    0x4
#define BME280_STANDBY_MS_1000   0x5

#ifdef __cplusplus
extern "C" {
#endif

struct bme280_calib_data {
    uint16_t bcd_dig_T1;
    int16_t  bcd_dig_T2;
    int16_t  bcd_dig_T3;

    uint16_t bcd_dig_P1;
    int16_t  bcd_dig_P2;
    int16_t  bcd_dig_P3;
    int16_t  bcd_dig_P4;
    int16_t  bcd_dig_P5;
    int16_t  bcd_dig_P6;
    int16_t  bcd_dig_P7;
    int16_t  bcd_dig_P8;
    int16_t  bcd_dig_P9;

    uint8_t  bcd_dig_H1;
    int16_t  bcd_dig_H2;
    uint8_t  bcd_dig_H3;
    int16_t  bcd_dig_H4;
    int16_t  bcd_dig_H5;
    int8_t   bcd_dig_H6;
};

struct bme280_over_cfg {
    sensor_type_t boc_type;
    uint8_t boc_oversample;
};

struct bme280_cfg {
    uint8_t bc_iir;
    struct bme280_over_cfg bc_boc[3];
    uint8_t bc_mode;
    uint8_t bc_sby_dur;
    sensor_type_t bc_s_mask;
};

struct bme280_pdd {
    struct bme280_calib_data bcd;
    int32_t t_fine;
};

struct bme280 {
    struct os_dev dev;
    struct sensor sensor;
    struct bme280_cfg cfg;
    struct bme280_pdd pdd;
    os_time_t last_read_time;
};

/**
 * Initialize the bme280.
 *
 * @param dev  Pointer to the bme280_dev device descriptor
 *
 * @return 0 on success, and non-zero error code on failure
 */
int bme280_init(struct os_dev *dev, void *arg);

/**
 * Sets IIR filter
 *
 * @param The sensor interface
 * @param filter setting
 *
 * @return 0 on success, and non-zero error code on failure
 */
int bme280_set_iir(struct sensor_itf *itf, uint8_t iir);

/**
 * Get IIR filter setting
 *
 * @param The sensor interface
 * @param ptr to fill up iir setting into
 *
 * @return 0 on success, and non-zero error code on failure
 */
int bme280_get_iir(struct sensor_itf *itf, uint8_t *iir);

/**
 * Gets temperature
 *
 * @param The sensor interface
 * @param temperature
 *
 * @return 0 on success, and non-zero error code on failure
 */
int bme280_get_temperature(struct sensor_itf *itf, int32_t *temp);

/**
 * Gets pressure
 *
 * @param The sensor interface
 * @param pressure
 *
 * @return 0 on success, and non-zero error code on failure
 */
int bme280_get_pressure(struct sensor_itf *itf, int32_t *press);

/**
 * Gets humidity
 *
 * @param The sensor interface
 * @param humidity
 *
 * @return 0 on success, and non-zero error code on failure
 */
int bme280_get_humidity(struct sensor_itf *itf, int32_t *humid);

/**
 * Sets the sampling rate
 *
 * @param The sensor interface
 * @param sensor type
 * @param sampling rate
 *
 * @return 0 on success, and non-zero error code on failure
 */
int bme280_set_oversample(struct sensor_itf *itf, sensor_type_t type, uint8_t rate);

/**
 * Gets the current sampling rate for the type of sensor
 *
 * @param The sensor interface
 * @param Type of sensor to return sampling rate
 *
 * @return 0 on success, non-zero on failure
 */
int bme280_get_oversample(struct sensor_itf *itf, sensor_type_t type, uint8_t *rate);

/**
 * Sets the operating mode
 *
 * @param The sensor interface
 * @param mode
 *
 * @return 0 on success, and non-zero error code on failure
 */
int bme280_set_mode(struct sensor_itf *itf, uint8_t mode);

/**
 * Gets the operating mode
 *
 * @param The sensor interface
 * @param ptr to the mode variable to be filled up
 *
 * @return 0 on success, and non-zero error code on failure
 */
int bme280_get_mode(struct sensor_itf *itf, uint8_t *mode);

/**
 * Resets the BME280 chip
 *
 * @param The sensor interface
 * @return 0 on success, non-zero on failure
 */
int
bme280_reset(struct sensor_itf *itf);

/**
 * Configure BME280 sensor
 *
 * @param Sensor device BME280 structure
 * @param Sensor device BME280 config
 *
 * @return 0 on success, and non-zero error code on failure
 */
int bme280_config(struct bme280 *bme280, struct bme280_cfg *cfg);

/**
 * Get the chip id
 *
 * @param The sensor interface
 * @param ptr to fill up the chip id
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bme280_get_chipid(struct sensor_itf *itf, uint8_t *chipid);

/**
 * Set the standy duration setting
 *
 * @param The sensor interface
 * @param duration
 * @return 0 on success, non-zero on failure
 */
int
bme280_set_sby_duration(struct sensor_itf *itf, uint8_t dur);

/**
 * Get the standy duration setting
 *
 * @param The sensor interface
 * @param ptr to duration
 * @return 0 on success, non-zero on failure
 */
int
bme280_get_sby_duration(struct sensor_itf *itf, uint8_t *dur);

/**
 * Take forced measurement
 *
 * @param The sensor interface
 * @return 0 on success, non-zero on failure
 */
int
bme280_forced_mode_measurement(struct sensor_itf *itf);

#if MYNEWT_VAL(BME280_CLI)
int bme280_shell_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BME280_H__ */
