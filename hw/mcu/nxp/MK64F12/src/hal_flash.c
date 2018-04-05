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
 * Internal flash for MK64F12.
 * Size of the flash depends on the MCU model, flash is memory mapped
 * and is divided to 2k sectors throughout.
 * Programming is done 2 bytes at a time.
 */

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "os/mynewt.h"
#include <hal/hal_flash_int.h>

#include "MK64F12.h"
#include "fsl_flash.h"

/*
 * Alignment restriction on writes.
 */
#define MK64F12_FLASH_ALIGN     8

static int mk64f12_flash_read(const struct hal_flash *dev, uint32_t address,
        void *dst, uint32_t num_bytes);
static int mk64f12_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes);
static int mk64f12_flash_erase_sector(const struct hal_flash *dev,
        uint32_t sector_address);
static int mk64f12_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *addr, uint32_t *sz);
static int mk64f12_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs mk64f12_flash_funcs = {
    .hff_read = mk64f12_flash_read,
    .hff_write = mk64f12_flash_write,
    .hff_erase_sector = mk64f12_flash_erase_sector,
    .hff_sector_info = mk64f12_flash_sector_info,
    .hff_init = mk64f12_flash_init
};

static flash_config_t mk64f12_config;

struct hal_flash mk64f12_flash_dev = {
    /* Most items are set after FLASH_Init() */
    .hf_itf = &mk64f12_flash_funcs,
    .hf_align = MK64F12_FLASH_ALIGN
};

static int
mk64f12_flash_read(const struct hal_flash *dev, uint32_t address,
        void *dst, uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

static int
mk64f12_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t len)
{
    uint8_t padded[MK64F12_FLASH_ALIGN];
    uint8_t pad_len;

    if (address % MK64F12_FLASH_ALIGN) {
        /*
         * Unaligned write.
         */
        return -1;
    }

    pad_len = len & (MK64F12_FLASH_ALIGN - 1);
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
        if (FLASH_Program(&mk64f12_config, address, (uint32_t *)src, len) !=
            kStatus_Success) {
            return -1;
        }
    }
    if (pad_len) {
        if (FLASH_Program(&mk64f12_config, address + len, (uint32_t *)padded,
                          MK64F12_FLASH_ALIGN) != kStatus_Success) {
            return -1;
        }
    }
    return 0;
}

static int
mk64f12_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    int sr;
    int rc;

    OS_ENTER_CRITICAL(sr);
    rc = FLASH_Erase(&mk64f12_config, sector_address,
                     mk64f12_config.PFlashSectorSize,
                     kFLASH_apiEraseKey);
    OS_EXIT_CRITICAL(sr);
    if (rc == kStatus_Success) {
        return 0;
    }
    return -1;
}

static int
mk64f12_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *addr, uint32_t *sz)
{
    *addr = mk64f12_config.PFlashBlockBase +
            (idx * mk64f12_config.PFlashSectorSize);
    *sz = mk64f12_config.PFlashSectorSize;
    return 0;
}

static int
mk64f12_flash_init(const struct hal_flash *dev)
{
    if (FLASH_Init(&mk64f12_config) == kStatus_Success) {
        mk64f12_flash_dev.hf_base_addr = mk64f12_config.PFlashBlockBase;
        mk64f12_flash_dev.hf_size = mk64f12_config.PFlashTotalSize;
        mk64f12_flash_dev.hf_sector_cnt =
             (mk64f12_config.PFlashTotalSize / mk64f12_config.PFlashSectorSize);
    }
    return 0;
}
