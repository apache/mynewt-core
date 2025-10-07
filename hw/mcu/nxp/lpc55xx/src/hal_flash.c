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
#include <assert.h>
#include <os/mynewt.h>
#include <hal/hal_flash_int.h>

#include <fsl_iap.h>

#define ROUND_DOWN(a, n) (((a) / (n)) * (n))

/*
 * Alignment restriction on writes.
 */
#define MCUX_FLASH_ALIGN MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE)

static int mcux_flash_read(const struct hal_flash *dev, uint32_t address,
                           void *dst, uint32_t num_bytes);
static int mcux_flash_write(const struct hal_flash *dev, uint32_t address,
                            const void *src, uint32_t num_bytes);
static int mcux_flash_erase_sector(const struct hal_flash *dev,
                                   uint32_t sector_address);
static int mcux_flash_sector_info(const struct hal_flash *dev, int idx,
                                  uint32_t *addr, uint32_t *sz);
static int mcux_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs mcux_flash_funcs = {
    .hff_read = mcux_flash_read,
    .hff_write = mcux_flash_write,
    .hff_erase_sector = mcux_flash_erase_sector,
    .hff_sector_info = mcux_flash_sector_info,
    .hff_init = mcux_flash_init
};

static flash_config_t mcux_config = {0};

struct hal_flash mcux_flash_dev = {
    /* Most items are set after FLASH_Init() */
    .hf_itf = &mcux_flash_funcs,
    .hf_align = MCUX_FLASH_ALIGN,
    .hf_erased_val = 0xff,
};

static int
mcux_flash_read(const struct hal_flash *dev,
                uint32_t address,
                void *dst,
                uint32_t num_bytes)
{
    status_t status;

    status = FLASH_Read(&mcux_config, address, dst, num_bytes);
    if (status == kStatus_FLASH_Success) {
        return 0;
    }

    if (status == kStatus_FLASH_EccError) {
        status = FLASH_VerifyErase(&mcux_config, ROUND_DOWN(address, 4),
                                   ROUND_DOWN(address + num_bytes + 3, 4) -
                                       ROUND_DOWN(address, 4));
        /* If flash is erased memcpy will result in hard fault, just fill 0xFF */
        if (status == kStatus_FLASH_Success) {
            memset(dst, 0xFF, num_bytes);
            return 0;
        } else {
            return -2;
        }
    }
    return -1;
}

static int
mcux_flash_write(const struct hal_flash *dev,
                 uint32_t address,
                 const void *src,
                 uint32_t len)
{
    uint8_t padded[MCUX_FLASH_ALIGN];
    uint8_t pad_len;

    if (address % MCUX_FLASH_ALIGN) {
        /*
         * Unaligned write.
         */
        return -1;
    }

    pad_len = len & (MCUX_FLASH_ALIGN - 1);
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
        if (FLASH_Program(&mcux_config, address, (void *)src, len) !=
            kStatus_Success) {
            return -1;
        }
    }
    if (pad_len) {
        if (FLASH_Program(&mcux_config, address + len, (void *)padded,
                          MCUX_FLASH_ALIGN) != kStatus_Success) {
            return -1;
        }
    }
    return 0;
}

static int
mcux_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    int sr;
    int rc;
    uint32_t sector_size;
    FLASH_GetProperty(&mcux_config,
                      kFLASH_PropertyPflashSectorSize,
                      &sector_size);

    OS_ENTER_CRITICAL(sr);
    rc = FLASH_Erase(&mcux_config, sector_address,
                     sector_size,
                     kFLASH_ApiEraseKey);
    OS_EXIT_CRITICAL(sr);
    if (rc == kStatus_Success) {
        return 0;
    }
    return -1;
}

static int
mcux_flash_sector_info(const struct hal_flash *dev,
                       int idx,
                       uint32_t *addr,
                       uint32_t *sz)
{
    uint32_t sector_size;
    FLASH_GetProperty(&mcux_config,
                      kFLASH_PropertyPflashSectorSize,
                      &sector_size);
    *addr = mcux_flash_dev.hf_base_addr + (idx * sector_size);
    FLASH_GetProperty(&mcux_config,
                      kFLASH_PropertyPflashSectorSize,
                      sz);
    return 0;
}

static int
mcux_flash_init(const struct hal_flash *dev)
{
    uint32_t sector_size;
    if (FLASH_Init(&mcux_config) != kStatus_FLASH_Success) {
        return -1;
    }
    FLASH_GetProperty(&mcux_config,
                      kFLASH_PropertyPflashBlockBaseAddr,
                      &mcux_flash_dev.hf_base_addr);
    FLASH_GetProperty(&mcux_config,
                      kFLASH_PropertyPflashTotalSize,
                      &mcux_flash_dev.hf_size);
    FLASH_GetProperty(&mcux_config,
                      kFLASH_PropertyPflashSectorSize,
                      &sector_size);
    mcux_flash_dev.hf_sector_cnt = (mcux_flash_dev.hf_size / sector_size);
    return 0;
}
