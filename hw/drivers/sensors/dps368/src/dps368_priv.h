/*Copyright 2019 Infineon Technologies AG
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     https://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * dps368_priv.h
 *
 *  Created on: Apr 15, 2019
 *      Author: Saumitra Chafekar
 */

#ifndef HW_DRIVERS_SENSORS_DPS368_SRC_DPS368_PRIV_H_
#define HW_DRIVERS_SENSORS_DPS368_SRC_DPS368_PRIV_H_

#include "os/mynewt.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#include "bus/drivers/spi_common.h"
#else
#include "hal/hal_i2c.h"
#include "hal/hal_spi.h"
#include "i2cn/i2cn.h"
#endif

#include "hal/hal_gpio.h"
#include "modlog/modlog.h"
#include "stats/stats.h"


#define DPS368_SPI_READ_CMD_BIT         0x80


#ifdef __cplusplus
extern "C" {
#endif

/**
 * sensor stats init
 *
 *
 * @param ptr Device structure
 *
 * @return  nothing
 */
void
dps368_stats_int(struct os_dev *dev);

/**
 * Write a single register to DPS368 value over underlying communication interface
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 */
int dps368_write_reg(struct sensor_itf *itf, uint8_t addr, uint8_t value);

/**
 * Read a single or multiple registers from DPS368 over underlying communication interface
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 */
int dps368_read_regs(struct sensor_itf *itf, uint8_t addr, uint8_t *buff, uint8_t len);

#ifdef __cplusplus
}
#endif


#endif /* HW_DRIVERS_SENSORS_DPS368_SRC_DPS368_PRIV_H_ */
