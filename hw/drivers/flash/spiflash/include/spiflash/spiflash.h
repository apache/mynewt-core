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

#ifndef __SPIFLASH_H__
#define __SPIFLASH_H__

#include <hal/hal_flash_int.h>
#include <hal/hal_spi.h>

#ifdef __cplusplus
extern "C" {
#endif

struct spiflash_dev {
    struct hal_flash hal;
    struct hal_spi_settings spi_settings;
    int spi_num;
    void *spi_cfg;                  /** Low-level MCU SPI config */
    int ss_pin;
    uint16_t sector_size;
    uint16_t page_size;
    /* Array of supported flash chips */
    const struct spiflash_chip *supported_chips;
    /* Pointer to one of the supported chips */
    const struct spiflash_chip *flash_chip;
};

extern struct spiflash_dev spiflash_dev;

#define SPIFLASH_PAGE_PROGRAM               0x02
#define SPIFLASH_READ                       0x03
#define SPIFLASH_READ_STATUS_REGISTER       0x05
#define SPIFLASH_READ_STATUS_REGISTER2      0x35
#define SPIFLASH_WRITE_ENABLE               0x06
#define SPIFLASH_FAST_READ                  0x0B
#define SPIFLASH_SECTOR_ERASE               0x20
#define SPIFLASH_DEEP_POWER_DOWN            0xB9
#define SPIFLASH_RELEASE_POWER_DOWN         0xAB
#define SPIFLASH_READ_MANUFACTURER_ID       0x90
#define SPIFLASH_READ_JEDEC_ID              0x9F

#define SPIFLASH_STATUS_BUSY                0x01
#define SPIFLASH_STATUS_WRITE_ENABLE        0x02

/*
 * Flash identification bytes from 0x9F command
 */
struct jedec_id {
    uint8_t ji_manufacturer;
    uint8_t ji_type;
    uint8_t ji_capacity;
};
struct spiflash_chip {
    struct jedec_id fc_jedec_id;
    void (*fc_release_power_down)(struct spiflash_dev *dev);
};

#define JEDEC_MFC_ISSI              0x9D
#define JEDEC_MFC_WINBOND           0xEF
#define JEDEC_MFC_GIGADEVICE        0xC8
#define JEDEC_MFC_MACRONIX          0xC2
#define JEDEC_MFC_MICRON            0x20
#define JEDEC_MFC_MICROCHIP         0xBF
#define JEDEC_MFC_ADESTO            0x1F

#define FLASH_CAPACITY_256KBIT      0x09
#define FLASH_CAPACITY_512KBIT      0x10
#define FLASH_CAPACITY_1MBIT        0x11
#define FLASH_CAPACITY_2MBIT        0x12
#define FLASH_CAPACITY_4MBIT        0x13
#define FLASH_CAPACITY_8MBIT        0x14
#define FLASH_CAPACITY_16MBIT       0x15
#define FLASH_CAPACITY_32MBIT       0x16

int spiflash_init(const struct hal_flash *dev);

#ifdef __cplusplus
}
#endif

#endif /* __SPIFLASH_H__ */
