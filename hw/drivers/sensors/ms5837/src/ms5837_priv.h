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

#ifndef __MS5837_PRIV_H__
#define __MS5837_PRIV_H__

/* Commands */
#define	MS5837_CMD_RESET            0x1E
/* Max conversion time: 0.56 ms ; 0.11  mbar RMS resolution */
#define MS5837_CMD_D1_256           MS5837_CMD_PRESS|MS5837_RES_OSR_256
/* Max conversion time: 1.10 ms : 0.062 mbar RMS resolution */
#define MS5837_CMD_D1_512           MS5837_CMD_PRESS|MS5837_RES_OSR_512
/* Max conversion time: 2.17 ms : 0.039 mbar RMS resolution */
#define MS5837_CMD_D1_1024          MS5837_CMD_PRESS|MS5837_RES_OSR_1024
/* Max conversion time: 4.32 ms ; 0.028 mbar RMS resolution */
#define MS5837_CMD_D1_2048          MS5837_CMD_PRESS|MS5837_RES_OSR_2048
/* Max conversion time: 8.61 ms : 0.021 mbar RMS resolution */
#define MS5837_CMD_D1_4096          MS5837_CMD_PRESS|MS5837_RES_OSR_4096
/* Max conversion time: 17.2 ms : 0.016 mbar RMS resolution */
#define MS5837_CMD_D1_8192          MS5837_CMD_PRESS|MS5837_RES_OSR_8192
/* Max conversion time: 0.56 ms : 0.012   °C RMS resolution */
#define MS5837_CMD_D2_256           MS5837_CMD_TEMP|MS5837_RES_OSR_256
/* Max conversion time: 1.10 ms : 0.009   °C RMS resolution */
#define MS5837_CMD_D2_512           MS5837_CMD_TEMP|MS5837_RES_OSR_512
/* Max conversion time: 2.17 ms : 0.006   °C RMS resolution */
#define MS5837_CMD_D2_1024          MS5837_CMD_TEMP|MS5837_RES_OSR_1024
/* Max conversion time: 4.32 ms ; 0.004   °C RMS resolution */
#define MS5837_CMD_D2_2048          MS5837_CMD_TEMP|MS5837_RES_OSR_2048
/* Max conversion time: 8.61 ms : 0.003   °C RMS resolution */
#define MS5837_CMD_D2_4096          MS5837_CMD_TEMP|MS5837_RES_OSR_4096
/* Max conversion time: 17.2 ms : 0.002   °C RMS resolution */
#define MS5837_CMD_D2_8192          MS5837_CMD_TEMP|MS5837_RES_OSR_8192

#define	MS5837_CMD_ADC_READ         0x00
#define MS5837_CMD_PRESS            0x40
#define MS5837_CMD_TEMP             0x50
#define MS5837_CMD_PROM_READ        0xA0

/* PROM Address commands */
#define MS5837_CMD_PROM_READ_ADDR0  0xA0
#define MS5837_CMD_PROM_READ_ADDR1  0xA2
#define MS5837_CMD_PROM_READ_ADDR2  0xA4
#define MS5837_CMD_PROM_READ_ADDR3  0xA6
#define MS5837_CMD_PROM_READ_ADDR4  0xA8
#define MS5837_CMD_PROM_READ_ADDR5  0xAA
#define MS5837_CMD_PROM_READ_ADDR6  0xAC
#define MS5837_CMD_PROM_READ_ADDR7  0xAE

/* Conversion time in micro seconds */
#define MS5837_CNV_TIME_OSR_256      560
#define MS5837_CNV_TIME_OSR_512     1100
#define MS5837_CNV_TIME_OSR_1024    2170
#define MS5837_CNV_TIME_OSR_2048    4320
#define MS5837_CNV_TIME_OSR_4096    8610
#define MS5837_CNV_TIME_OSR_8192   17200

/* Coefficient EEPROM indexes for temperature and pressure compensation */
#define MS5837_IDX_CRC                      0
#define MS5837_IDX_PRESS_SENS               1
#define MS5837_IDX_PRESS_OFF                2
#define MS5837_IDX_TEMP_COEFF_PRESS_SENS    3
#define MS5837_IDX_TEMP_COEFF_PRESS_OFF     4
#define MS5837_IDX_REF_TEMP                 5
#define MS5837_IDX_TEMP_COEFF_TEMP          6
#define MS5837_NUMBER_COEFFS                7

#define MS5837_CNV_OSR_MASK         0x0F

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write multiple length data to MS5837 sensor over SPI
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 */
int ms5837_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload, uint8_t len);

/**
 * Read multiple length data from MS5837 sensor over SPI
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 */
int ms5837_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __MS5837_PRIV_H_ */
