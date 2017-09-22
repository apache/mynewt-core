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


#ifndef __MS5837_H__
#define __MS5837_H__

#include <os/os.h>
#include "os/os_dev.h"
#include "sensor/sensor.h"

#define MS5837_I2C_ADDRESS		0x76
#define MS5837_NUMBER_COEFFS     7

#ifdef __cplusplus
extern "C" {
#endif

struct ms5837_pdd {
    uint16_t eeprom_coeff[MS5837_NUMBER_COEFFS + 1];
};

struct ms5837_cfg {
    uint8_t mc_s_res_osr;
    sensor_type_t mc_s_mask;
};

struct ms5837 {
    struct os_dev dev;
    struct sensor sensor;
    struct ms5837_cfg cfg;
    struct ms5837_pdd pdd;
    os_time_t last_read_time;
};

#define MS5837_RES_OSR_256    0x0
#define MS5837_RES_OSR_512    0x2
#define MS5837_RES_OSR_1024   0x4
#define MS5837_RES_OSR_2048   0x6
#define MS5837_RES_OSR_4096   0x8
#define MS5837_RES_OSR_8192   0xA

/**
 * Initialize the ms5837.
 *
 * @param dev  Pointer to the ms5837_dev device descriptor
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
ms5837_init(struct os_dev *dev, void *arg);

/**
 * Reads the temperature ADC value and applies eeprom coefficients
 * for compensation
 *
 * @param the sensor interface
 * @param temperature in DegC
 * @param pressure in mBar
 * @param resolution osr
 *
 * @return 0 on success, non-zero on failure
 */
int
ms5837_get_temp_press(struct sensor_itf *itf, uint32_t *rawtemp,
                      uint32_t *rawpress, uint8_t res_osr);

/**
 * Resets the MS5837 chip
 *
 * @param The sensor interface
 * @return 0 on success, non-zero on failure
 */
int
ms5837_reset(struct sensor_itf *itf);

/**
 * Configure MS5837 sensor
 *
 * @param Sensor device MS5837 structure
 * @param Sensor device MS5837 config
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
ms5837_config(struct ms5837 *ms5837, struct ms5837_cfg *cfg);

/**
 * crc4 check for MS5837 EEPROM
 *
 * @param buffer containing EEPROM coefficients
 * @param crc to compare with
 *
 * return 0 on success (CRC is OK), non-zero on failure
 */
int
ms5837_crc_check(uint16_t *prom, uint8_t crc);

/**
 * Reads the ms5837 EEPROM coefficients for computation and
 * does a CRC check on them
 *
 * @param the sensor interface
 * @param buffer to fill up the coefficients
 *
 * @return 0 on success, non-zero on failure
 */
int
ms5837_read_eeprom(struct sensor_itf *itf, uint16_t *coeff);

/**
 * Compensate for temperature and pressure using coefficients from teh EEPROM
 *
 * @param ptr to coefficients
 * @param raw temperature
 * @param raw pressure
 * @param compensated temperature in DegC
 * @param compensated pressure in mBar
 */
void
ms5837_compensate_temp_press(uint16_t *coeffs, uint32_t rawtemp, uint32_t rawpress,
                             float *temperature, float *pressure);

#ifdef __cplusplus
}
#endif

#endif /* __MS5837_H__ */
