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

#ifndef __ICP101XX_PRIV_H__
#define __ICP101XX_PRIV_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#define ICP101XX_ID                         0x08
#define ICP101XX_PRODUCT_SPECIFIC_BITMASK   0x003F

#define ICP101XX_I2C_ADDR                   0x63
#define ICP10114_I2C_ADDR                   0x64

#define ICP101XX_ODR_MIN_DELAY              25000 /* us => 40 Hz */

#define ICP101XX_CMD_MEAS_LOW_POWER_T_FIRST          0x609C
#define ICP101XX_CMD_MEAS_LOW_POWER_P_FIRST          0x401A
#define ICP101XX_CMD_MEAS_NORMAL_T_FIRST             0x6825
#define ICP101XX_CMD_MEAS_NORMAL_P_FIRST             0x48A3
#define ICP101XX_CMD_MEAS_LOW_NOISE_T_FIRST          0x70DF
#define ICP101XX_CMD_MEAS_LOW_NOISE_P_FIRST          0x5059
#define ICP101XX_CMD_MEAS_ULTRA_LOW_NOISE_T_FIRST    0x7866
#define ICP101XX_CMD_MEAS_ULTRA_LOW_NOISE_P_FIRST    0x58E0

#define ICP101XX_CMD_SOFT_RESET    0x805D
#define ICP101XX_CMD_READ_ID       0xEFC8
#define ICP101XX_CMD_SET_CAL_PTR   0xC595
#define ICP101XX_CMD_INC_CAL_PTR   0xC7F7

#define ICP101XX_OTP_READ_ADDR     0x0066

#define ICP101XX_CRC8_INIT 0xFF
#define ICP101XX_RESP_DWORD_LEN 2
#define ICP101XX_RESP_CRC_LEN 1
#define ICP101XX_RESP_FRAME_LEN (ICP101XX_RESP_DWORD_LEN + ICP101XX_RESP_CRC_LEN)
#define ICP101XX_CRC8_POLYNOM 0x31

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write multiple length data to ICP101xx sensor register over sensor interface
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 * @return 0 on success, non-zero error on failure.
 */
int icp101xx_write_reg(struct sensor_itf *itf, uint16_t reg, uint8_t *payload, uint8_t len);

/**
 * Read multiple length data from ICP101xx register sensor over sensor interface
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 * @return 0 on success, non-zero error on failure.
 */
int icp101xx_read_reg(struct sensor_itf *itf, uint16_t reg, uint8_t *payload, uint8_t len);

/**
 * Read multiple length data from ICP101xx sensor over sensor interface
 *
 * @param The Sensor interface
 * @param variable length payload
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero error on failure.
 */
int icp101xx_read(struct sensor_itf *itf, uint8_t * payload, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __ICP101XX_PRIV_H_ */