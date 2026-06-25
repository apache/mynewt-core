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

struct mmc_spi_cfg {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    const char *bus_name;
#endif
    uint32_t initial_freq_khz;
    uint32_t freq_khz;
    int pin_cs;
    uint8_t clock_mode;
};

struct mmc_spi {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_spi_node node;
#else
    int spi_num;
#endif
    struct mmc_spi_cfg mmc_spi_cfg;
};

typedef struct mmc_disk {
    struct mmc_spi spi;
    disk_t disk;
    disk_info_t disk_info;
    uint8_t ocr[4];
    uint8_t csd[16];
    uint8_t scr[8];
    uint8_t cid[16];
    uint8_t app_cmd : 1;
    struct os_sem sem;
} mmc_disk_t;

#if MYNEWT_VAL_BUS_DRIVER_PRESENT
int mmc_create_dev(mmc_disk_t *mmc, const char *name, struct mmc_spi_cfg *mmc_cfg);
#else
int mmc_create(mmc_disk_t *mmc, int spi_num, struct mmc_spi_cfg *mmc_cfg);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MMC_H__ */
