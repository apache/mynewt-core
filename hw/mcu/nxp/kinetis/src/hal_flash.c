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

/*
 * Internal flash for the Kinetis family.
 * Size of the flash depends on the MCU model, flash is memory mapped
 * and is divided to 4k sectors throughout.
 */

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "os/mynewt.h"
#include <hal/hal_flash_int.h>

#include "syscfg/syscfg.h"
#include "fsl_flash.h"

/*
 * Alignment restriction on writes.
 */
#define KINETIS_FLASH_ALIGN MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE)

static int kinetis_flash_read(const struct hal_flash *dev, uint32_t address,
                              void *dst, uint32_t num_bytes);
static int kinetis_flash_write(const struct hal_flash *dev, uint32_t address,
                               const void *src, uint32_t num_bytes);
static int kinetis_flash_erase_sector(const struct hal_flash *dev,
                                      uint32_t sector_address);
static int kinetis_flash_sector_info(const struct hal_flash *dev, int idx,
                                     uint32_t *addr, uint32_t *sz);
static int kinetis_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs kinetis_flash_funcs = {
    .hff_read = kinetis_flash_read,
    .hff_write = kinetis_flash_write,
    .hff_erase_sector = kinetis_flash_erase_sector,
    .hff_sector_info = kinetis_flash_sector_info,
    .hff_init = kinetis_flash_init
};

static flash_config_t kinetis_config = {0};

struct hal_flash kinetis_flash_dev = {
    /* Most items are set after FLASH_Init() */
    .hf_itf = &kinetis_flash_funcs,
    .hf_align = KINETIS_FLASH_ALIGN,
    .hf_erased_val = 0xff,
};

static int
kinetis_flash_read(const struct hal_flash *dev,
                   uint32_t address,
                   void *dst,
                   uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

static int
kinetis_flash_write(const struct hal_flash *dev,
                    uint32_t address,
                    const void *src,
                    uint32_t len)
{
    uint8_t padded[KINETIS_FLASH_ALIGN];
    uint8_t pad_len;

    if (address % KINETIS_FLASH_ALIGN) {
        /*
         * Unaligned write.
         */
        return -1;
    }

    pad_len = len & (KINETIS_FLASH_ALIGN - 1);
    if (pad_len) {
        /*
         * FLASH_Program also needs length to be aligned to 8 bytes.
         * Pad these writes.
         */
        len -= pad_len;
        memcpy(padded, (uint8_t *)src + len, pad_len);
        memset(padded + pad_len, 0xff, sizeof(padded) - pad_len);
    }
    if (len) {
        if (FLASH_Program(&kinetis_config, address, (uint8_t *)src, len) !=
            kStatus_Success) {
            return -1;
        }
    }
    if (pad_len) {
        if (FLASH_Program(&kinetis_config, address + len, (uint8_t *)padded,
                          KINETIS_FLASH_ALIGN) != kStatus_Success) {
            return -1;
        }
    }
    return 0;
}

static int
kinetis_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    int sr;
    int rc;
    uint32_t sector_size;
    FLASH_GetProperty(&kinetis_config,
                      kFLASH_PropertyPflash0SectorSize,
                      &sector_size);

    OS_ENTER_CRITICAL(sr);
    rc = FLASH_Erase(&kinetis_config, sector_address,
                     sector_size,
                     kFLASH_ApiEraseKey);
    OS_EXIT_CRITICAL(sr);
    if (rc == kStatus_Success) {
        return 0;
    }
    return -1;
}

static int
kinetis_flash_sector_info(const struct hal_flash *dev,
                          int idx,
                          uint32_t *addr,
                          uint32_t *sz)
{
    uint32_t sector_size;
    FLASH_GetProperty(&kinetis_config,
                      kFLASH_PropertyPflash0SectorSize,
                      &sector_size);
    *addr = kinetis_flash_dev.hf_base_addr + (idx * sector_size);
    FLASH_GetProperty(&kinetis_config,
                      kFLASH_PropertyPflash0SectorSize,
                      sz);
    return 0;
}

static int
kinetis_flash_init(const struct hal_flash *dev)
{
    uint32_t sector_size;
    if (FLASH_Init(&kinetis_config) != kStatus_FTFx_Success) {
        return -1;
    }
    FLASH_GetProperty(&kinetis_config,
                      kFLASH_PropertyPflash0BlockBaseAddr, &
                      kinetis_flash_dev.hf_base_addr);
    FLASH_GetProperty(&kinetis_config,
                      kFLASH_PropertyPflash0TotalSize, &
                      kinetis_flash_dev.hf_size);
    FLASH_GetProperty(&kinetis_config,
                      kFLASH_PropertyPflash0SectorSize,
                      &sector_size);
    kinetis_flash_dev.hf_sector_cnt = (kinetis_flash_dev.hf_size / sector_size);
    return 0;
}
