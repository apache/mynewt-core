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

#ifndef __ADXL345_PRIV_H__
#define __ADXL345_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

enum adxl345_registers {
    ADXL345_DEVID          = 0x00, /* R */
    ADXL345_THRESH_TAP     = 0x1D, /* R/W */
    ADXL345_OFSX           = 0x1E, /* R/W */
    ADXL345_OFSY           = 0x1F, /* R/W */
    ADXL345_OFSZ           = 0x20, /* R/W */
    ADXL345_DUR            = 0x21, /* R/W */
    ADXL345_LATENT         = 0x22, /* R/W */
    ADXL345_WINDOW         = 0x23, /* R/W */
    ADXL345_THRESH_ACT     = 0x24, /* R/W */
    ADXL345_THRESH_INACT   = 0x25, /* R/W */
    ADXL345_TIME_INACT     = 0x26, /* R/W */
    ADXL345_ACT_INACT_CTL  = 0x27, /* R/W */
    ADXL345_THRESH_FF      = 0x28, /* R/W */
    ADXL345_TIME_FF        = 0x29, /* R/W */
    ADXL345_TAP_AXES       = 0x2A, /* R/W */
    ADXL345_ACT_TAP_STATUS = 0x2B, /* R */
    ADXL345_BW_RATE        = 0x2C, /* R/W */
    ADXL345_POWER_CTL      = 0x2D, /* R/W */
    ADXL345_INT_ENABLE     = 0x2E, /* R/W */
    ADXL345_INT_MAP        = 0x2F, /* R/W */
    ADXL345_INT_SOURCE     = 0x30, /* R */
    ADXL345_DATA_FORMAT    = 0x31, /* R/W */
    ADXL345_DATAX0         = 0x32, /* R */
    ADXL345_DATAX1         = 0x33, /* R */
    ADXL345_DATAY0         = 0x34, /* R */
    ADXL345_DATAY1         = 0x35, /* R */
    ADXL345_DATAZ0         = 0x36, /* R */
    ADXL345_DATAZ1         = 0x37, /* R */
    ADXL345_FIFO_CTL       = 0x38, /* R/W */
    ADXL345_FIFO_STATUS    = 0x39, /* R */
};

    
#define ADXL345_DEVID_VAL (0xE5)

#define ADXL345_SPI_READ_CMD_BIT      (0x80)
#define ADXL345_SPI_MULTIBYTE_CMD_BIT (0x40)

    
int adxl345_i2c_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value);
int adxl345_i2c_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value);
int adxl345_i2c_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len);

int adxl345_spi_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value);
int adxl345_spi_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value);
int adxl345_spi_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len);

int adxl345_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value);
int adxl345_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value);
int adxl345_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len);
 
#ifdef __cplusplus
}
#endif

#endif /* __ADXL345_PRIV_H__ */
