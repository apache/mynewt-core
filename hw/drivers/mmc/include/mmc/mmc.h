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

#ifndef __MMC_H__
#define __MMC_H__

#include <os/os_dev.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MMC driver errors.
 */
#define MMC_OK              (0)
#define MMC_CARD_ERROR      (-1)  /* Is there a card installed? */
#define MMC_READ_ERROR      (-2)
#define MMC_WRITE_ERROR     (-3)
#define MMC_TIMEOUT         (-4)
#define MMC_PARAM_ERROR     (-5)
#define MMC_CRC_ERROR       (-6)
#define MMC_DEVICE_ERROR    (-7)
#define MMC_RESPONSE_ERROR  (-8)
#define MMC_VOLTAGE_ERROR   (-9)

/**
 * Initialize the MMC driver
 *
 * @param spi_num Number of the SPI channel to be used by MMC
 * @param spi_cfg Low-level device specific SPI configuration
 * @param ss_pin GPIO number of the SS pin
 *
 * @return 0 on success, non-zero on failure
 */
int
mmc_init(int spi_num, void *spi_cfg, int ss_pin);

/**
 * Read data from MMC
 *
 * @param mmc_id
 * @param addr
 * @param buf
 * @param len
 *
 * @return 0 on success, non-zero on failure
 */
int
mmc_read(uint8_t mmc_id, uint32_t addr, void *buf, size_t len);

/**
 * Write data to the MMC
 *
 * @param mmc_id
 * @param addr
 * @param buf
 * @param len
 *
 * @return 0 on success, non-zero on failure
 */
int
mmc_write(uint8_t mmc_id, uint32_t addr, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __MMC_H__ */
