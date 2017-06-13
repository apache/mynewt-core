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

#ifndef __TCS34725_PRIV_H__
#define __TCS34725_PRIV_H__

#define TCS34725_ID               0x44

/* COMMAND Register - Specific register address */
#define TCS34725_COMMAND_BIT      (1 << 7)
#define TCS34725_CMD_TYPE         (3 << 5)
#define TCS34725_CMD_ADDR         (6 << 1)

/* Enable register */
#define TCS34725_REG_ENABLE       0x00
#define TCS34725_ENABLE_AIEN      0x10    /* enable/disable RGBC interrupt */
#define TCS34725_ENABLE_WEN       0x08    /* enable/disable wait timer */
#define TCS34725_ENABLE_AEN       0x02    /* enable/disable ADC */
#define TCS34725_ENABLE_PON       0x01    /* enable/disable int oscillator */

/* Integration time */
#define TCS34725_REG_ATIME        0x01

/* Wait time register (if TCS34725_ENABLE_WEN is asserted) */
#define TCS34725_REG_WTIME        0x03
#define TCS34725_WTIME_2_4MS      0xFF    /* WLONG0 = 2.4ms WLONG1 = 0.029s */
#define TCS34725_WTIME_204MS      0xAB    /* WLONG0 = 204ms WLONG1 = 2.45s  */
#define TCS34725_WTIME_614MS      0x00    /* WLONG0 = 614ms WLONG1 = 7.4s   */

/* Clear channel lower interrupt threshold register */
#define TCS34725_REG_AILTL        0x04
#define TCS34725_REG_AILTH        0x05

/* Clear channel upper interrupt threshold register */
#define TCS34725_REG_AIHTL        0x06
#define TCS34725_REG_AIHTH        0x07

/* Persistence register */
#define TCS34725_REG_PERS         0x0C
#define TCS34725_PERS_NONE        0x00    /* interrupt for each RGBC cycle */
#define TCS34725_PERS_1_CYCLE     0x01    /* interrupt for 1 RGBC cycle    */
#define TCS34725_PERS_2_CYCLE     0x02    /* interrupt for 2 RGBC cycle    */
#define TCS34725_PERS_3_CYCLE     0x03    /* interrupt for 3 RGBC cycle    */
#define TCS34725_PERS_5_CYCLE     0x04    /* interrupt for 5 RGBC cycle    */
#define TCS34725_PERS_10_CYCLE    0x05    /* interrupt for 10 RGBC cycle   */
#define TCS34725_PERS_15_CYCLE    0x06    /* interrupt for 15 RGBC cycle   */
#define TCS34725_PERS_20_CYCLE    0x07    /* interrupt for 20 RGBC cycle   */
#define TCS34725_PERS_25_CYCLE    0x08    /* interrupt for 25 RGBC cycle   */
#define TCS34725_PERS_30_CYCLE    0x09    /* interrupt for 30 RGBC cycle   */
#define TCS34725_PERS_35_CYCLE    0x0A    /* interrupt for 35 RGBC cycle   */
#define TCS34725_PERS_40_CYCLE    0x0B    /* interrupt for 40 RGBC cycle   */
#define TCS34725_PERS_45_CYCLE    0x0C    /* interrupt for 45 RGBC cycle   */
#define TCS34725_PERS_50_CYCLE    0x0D    /* interrupt for 50 RGBC cycle   */
#define TCS34725_PERS_55_CYCLE    0x0E    /* interrupt for 55 RGBC cycle   */
#define TCS34725_PERS_60_CYCLE    0x0F    /* interrupt for 60 RGBC cycle   */

/* Configuration register */
#define TCS34725_REG_CONFIG       0x0D
#define TCS34725_CONFIG_WLONG     0x02    /* 0x or 12x TCS34725_WTIME */

/* Set the gain level for the sensor */
#define TCS34725_REG_CONTROL      0x0F

/* Should be 0x44 for TCS34725 */
#define TCS34725_REG_ID           0x12

/* Device status register */
#define TCS34725_REG_STATUS       0x13
#define TCS34725_STATUS_AINT      0x10    /* RGBC Clean channel interrupt  */
#define TCS34725_STATUS_AVALID    0x01    /* RGBC completed an integ cycle */

/* Clear channel data */
#define TCS34725_REG_CDATAL       0x14
#define TCS34725_REG_CDATAH       0x15

/* Red channel data */
#define TCS34725_REG_RDATAL       0x16
#define TCS34725_REG_RDATAH       0x17

/* Green channel data */
#define TCS34725_REG_GDATAL       0x18
#define TCS34725_REG_GDATAH       0x19

/* Blue channel data */
#define TCS34725_REG_BDATAL       0x1A
#define TCS34725_REG_BDATAH       0x1B

#define TCS34725_R_COEF         0.136F
#define TCS34725_G_COEF         1.000F
#define TCS34725_B_COEF        -0.444F
#define TCS34725_GA               1.0F
#define TCS34725_DF             310.0F
#define TCS34725_CT_COEF       3810.0F
#define TCS34725_CT_OFFSET     1391.0F

#ifdef __cplusplus
extern "C" {
#endif

struct tcs_agc {
    uint8_t ta_gain;
    uint8_t ta_time;
    uint16_t min_cnt;
    uint16_t max_cnt;
};

/**
 * Writes a single byte to the specified register
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */

int tcs34725_write8(struct sensor_itf *itf,
                    uint8_t reg, uint32_t value);

/**
 * Writes multiple bytes to the specified register
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The data buffer to write from
 *
 * @return 0 on success, non-zero error on failure.
 */
int
tcs34725_writelen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len);


/**
 * Reads a single byte from the specified register
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
tcs34725_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value);

/**
 * Read data from the sensor of variable length (MAX: 8 bytes)
 *
 *
 * @param The sensor interface
 * @param Register to read from
 * @param Bufer to read into
 * @param Length of the buffer
 *
 * @return 0 on success and non-zero on failure
 */
int
tcs34725_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* TCS34725_PRIV_H_ */
