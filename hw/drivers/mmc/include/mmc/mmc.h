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

#include "os/mynewt.h"
#include <disk/disk.h>
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include <bus/drivers/spi_common.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MMC driver errors.
 */
#define MMC_OK                (0)
#define MMC_CARD_ERROR        (-1)  /* Is there a card installed? */
#define MMC_READ_ERROR        (-2)
#define MMC_WRITE_ERROR       (-3)
#define MMC_TIMEOUT           (-4)
#define MMC_PARAM_ERROR       (-5)
#define MMC_CRC_ERROR         (-6)
#define MMC_DEVICE_ERROR      (-7)
#define MMC_RESPONSE_ERROR    (-8)
#define MMC_VOLTAGE_ERROR     (-9)
#define MMC_INVALID_COMMAND   (-10)
#define MMC_ERASE_ERROR       (-11)
#define MMC_ADDR_ERROR        (-12)

extern struct disk_ops mmc_ops;

struct mmc_spi_cfg {
    uint32_t initial_freq_khz;
    uint32_t freq_khz;
    uint8_t clock_mode;
};

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
mmc_init(int spi_num, struct mmc_spi_cfg *spi_cfg, int ss_pin);

/**
 * Read data from MMC
 *
 * @param mmc_id Id of the MMC device (currently must be 0)
 * @param addr Disk address (in bytes) to be read from
 * @param buf Buffer where data should be copied to
 * @param len Amount of data to read/copy
 *
 * @return 0 on success, non-zero on failure
 */
int
mmc_read(uint8_t mmc_id, uint32_t addr, void *buf, uint32_t len);

/**
 * Write data to the MMC
 *
 * @param mmc_id Id of the MMC device (currently must be 0)
 * @param addr Disk address (in bytes) to be written to
 * @param buf Buffer where data should be copied from
 * @param len Amount of data to copy/write
 *
 * @return 0 on success, non-zero on failure
 */
int
mmc_write(uint8_t mmc_id, uint32_t addr, const void *buf, uint32_t len);

/**
 * TODO
 */
int
mmc_ioctl(uint8_t mmc_id, uint32_t cmd, void *arg);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
struct mmc_config {
    struct bus_spi_node_cfg spi_cfg;
};
struct mmc {
    struct bus_spi_node node;
};

int mmc_create_dev(struct mmc *mmc, const char *name, struct mmc_config *mmc_cfg);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MMC_H__ */
