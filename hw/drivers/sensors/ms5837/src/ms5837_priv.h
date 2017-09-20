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
#define	MS5837_CMD_RESET    0x1E
#define MS5837_CMD_D1_256   0x40			/* Max conversion time: 0.56 ms ; 0.11  mbar RMS resolution */
#define CMD_D1_512		      0x42			/* Max conversion time: 1.10 ms : 0.062 mbar RMS resolution */
#define CMD_D1_1024         0x44			/* Max conversion time: 2.17 ms : 0.039 mbar RMS resolution */
#define CMD_D1_2048				  0x46			/* Max conversion time: 4.32 ms ; 0.028 mbar RMS resolution */
#define CMD_D1_4096				  0x48			/* Max conversion time: 8.61 ms : 0.021 mbar RMS resolution */
#define CMD_D1_8192				  0x4A			/* Max conversion time: 17.2 ms : 0.016 mbar RMS resolution */
#define CMD_D2_256				  0x50			/* Max conversion time: 0.56 ms : 0.012   °C RMS resolution */
#define CMD_D2_512				  0x52			/* Max conversion time: 1.10 ms : 0.009   °C RMS resolution */
#define CMD_D2_1024				  0x54			/* Max conversion time: 2.17 ms : 0.006   °C RMS resolution */
#define CMD_D2_2048				  0x56			/* Max conversion time: 4.32 ms ; 0.004   °C RMS resolution */
#define CMD_D2_4096				  0x58			/* Max conversion time: 8.61 ms : 0.003   °C RMS resolution */
#define CMD_D2_8192				  0x5A			/* Max conversion time: 17.2 ms : 0.002   °C RMS resolution */
#define	CMD_ADC_READ			  0x00
#define CMD_PROM_READ			  0xA0

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
