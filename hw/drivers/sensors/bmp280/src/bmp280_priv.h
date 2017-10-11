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

#define BMP280_REG_ADDR_DIG_T1            0x88
#define BMP280_REG_ADDR_DIG_T2            0x8A
#define BMP280_REG_ADDR_DIG_T3            0x8C

#define BMP280_REG_ADDR_DIG_P1            0x8E
#define BMP280_REG_ADDR_DIG_P2            0x90
#define BMP280_REG_ADDR_DIG_P3            0x92
#define BMP280_REG_ADDR_DIG_P4            0x94
#define BMP280_REG_ADDR_DIG_P5            0x96
#define BMP280_REG_ADDR_DIG_P6            0x98
#define BMP280_REG_ADDR_DIG_P7            0x9A
#define BMP280_REG_ADDR_DIG_P8            0x9C
#define BMP280_REG_ADDR_DIG_P9            0x9E

#define BMP280_REG_ADDR_CHIPID            0xD0
#define BMP280_REG_ADDR_VERSION           0xD1
#define BMP280_REG_ADDR_SOFTRESET         0xE0

#define BMP280_REG_ADDR_CAL26             0xE1  /* R calibration stored in 0xE1-0xF0 */

#define BMP280_REG_ADDR_STATUS            0XF3
#define BMP280_REG_STATUS_MEAS            0x04
#define BMP280_REG_STATUS_IM_UP           0x01

#define BMP280_REG_ADDR_CTRL_MEAS         0xF4
#define BMP280_REG_CTRL_MEAS_TOVER  (0x7 << 5)
#define BMP280_REG_CTRL_MEAS_POVER  (0x7 << 2)
#define BMP280_REG_CTRL_MEAS_MODE        (0x3)

#define BMP280_REG_ADDR_CONFIG            0xF5
#define BMP280_REG_CONFIG_STANDBY   (0x7 << 5)
#define BMP280_REG_CONFIG_FILTER    (0x7 << 3)
#define BMP280_REG_CONFIG_SPI3_EN        (0x1)

#define BMP280_REG_ADDR_PRESS             0xF7
#define BMP280_REG_ADDR_PRESS_MSB         0xF7
#define BMP280_REG_ADDR_PRESS_LSB         0xF8
#define BMP280_REG_ADDR_PRESS_XLSB        0xF9

#define BMP280_REG_ADDR_TEMP              0xFA
#define BMP280_REG_ADDR_TEMP_MSB          0xFA
#define BMP280_REG_ADDR_TEMP_LSB          0xFB
#define BMP280_REG_ADDR_TEMP_XLSB         0xFC

#define BMP280_REG_ADDR_RESET             0xE0

#define BMP280_CHIPID                     0x58

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write multiple length data to BMP280 sensor over SPI
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 */
int bmp280_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload, uint8_t len);

/**
 * Read multiple length data from BMP280 sensor over SPI
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 */
int bmp280_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BMP280_PRIV_H_ */
