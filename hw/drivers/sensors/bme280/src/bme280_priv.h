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

#ifndef __BME280_PRIV_H__
#define __BME280_PRIV_H__

#define BME280_REG_ADDR_DIG_T1            0x88
#define BME280_REG_ADDR_DIG_T2            0x8A
#define BME280_REG_ADDR_DIG_T3            0x8C

#define BME280_REG_ADDR_DIG_P1            0x8E
#define BME280_REG_ADDR_DIG_P2            0x90
#define BME280_REG_ADDR_DIG_P3            0x92
#define BME280_REG_ADDR_DIG_P4            0x94
#define BME280_REG_ADDR_DIG_P5            0x96
#define BME280_REG_ADDR_DIG_P6            0x98
#define BME280_REG_ADDR_DIG_P7            0x9A
#define BME280_REG_ADDR_DIG_P8            0x9C
#define BME280_REG_ADDR_DIG_P9            0x9E

#define BME280_REG_ADDR_DIG_H1            0xA1
#define BME280_REG_ADDR_DIG_H2            0xE1
#define BME280_REG_ADDR_DIG_H3            0xE3
#define BME280_REG_ADDR_DIG_H4            0xE4
#define BME280_REG_ADDR_DIG_H5            0xE5
#define BME280_REG_ADDR_DIG_H6            0xE7

#define BME280_REG_ADDR_CHIPID            0xD0
#define BME280_REG_ADDR_VERSION           0xD1
#define BME280_REG_ADDR_SOFTRESET         0xE0

#define BME280_REG_ADDR_CAL26             0xE1  /* R calibration stored in 0xE1-0xF0 */

#define BME280_REG_ADDR_CTRL_HUM          0xF2
#define BME280_REG_CTRL_HUM_HOVER        (0x7)

#define BME280_REG_ADDR_STATUS            0XF3
#define BME280_REG_STATUS_MEAS            0x04
#define BME280_REG_STATUS_IM_UP           0x01

#define BME280_REG_ADDR_CTRL_MEAS         0xF4
#define BME280_REG_CTRL_MEAS_TOVER  (0x7 << 5)
#define BME280_REG_CTRL_MEAS_POVER  (0x7 << 2)
#define BME280_REG_CTRL_MEAS_MODE        (0x3)

#define BME280_REG_ADDR_CONFIG            0xF5
#define BME280_REG_CONFIG_STANDBY   (0x7 << 5)
#define BME280_REG_CONFIG_FILTER    (0x7 << 3)
#define BME280_REG_CONFIG_SPI3_EN        (0x1)

#define BME280_REG_ADDR_PRESS             0xF7
#define BME280_REG_ADDR_PRESS_MSB         0xF7
#define BME280_REG_ADDR_PRESS_LSB         0xF8
#define BME280_REG_ADDR_PRESS_XLSB        0xF9

#define BME280_REG_ADDR_TEMP              0xFA
#define BME280_REG_ADDR_TEMP_MSB          0xFA
#define BME280_REG_ADDR_TEMP_LSB          0xFB
#define BME280_REG_ADDR_TEMP_XLSB         0xFC

#define BME280_REG_ADDR_HUM               0xFD

#define BME280_REG_ADDR_RESET             0xE0

#define BME280_CHIPID                     0x60

#define BMP280_CHIPID                     0x58

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write multiple length data to BME280 sensor over SPI
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 */
int bme280_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload, uint8_t len);

/**
 * Read multiple length data from BME280 sensor over SPI
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 */
int bme280_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BME280_PRIV_H_ */
