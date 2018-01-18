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

#include <string.h>
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_flash_ex.h"
#include "hal/hal_flash_int.h"

static int stm32f1_flash_read(const struct hal_flash *dev, uint32_t address,
        void *dst, uint32_t num_bytes);
static int stm32f1_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes);
static int stm32f1_flash_erase_sector(const struct hal_flash *dev,
        uint32_t sector_address);
static int stm32f1_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz);
static int stm32f1_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs stm32f1_flash_funcs = {
    .hff_read = stm32f1_flash_read,
    .hff_write = stm32f1_flash_write,
    .hff_erase_sector = stm32f1_flash_erase_sector,
    .hff_sector_info = stm32f1_flash_sector_info,
    .hff_init = stm32f1_flash_init,
};

#define _FLASH_SIZE            (128 * 1024)
#define _FLASH_SECTOR_SIZE     1024

const struct hal_flash stm32f1_flash_dev = {
    .hf_itf = &stm32f1_flash_funcs,
    .hf_base_addr = 0x08000000,
    .hf_size = _FLASH_SIZE,
    .hf_sector_cnt = _FLASH_SIZE / _FLASH_SECTOR_SIZE,
    .hf_align = 2,
};

static int
stm32f1_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
        uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

static int
stm32f1_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    const uint16_t *sptr;
    uint32_t i;
    int rc;

    /*
     * Clear status of previous operation.
     */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP |
                           FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGERR |
                           FLASH_FLAG_BSY);

    num_bytes /= 2;
    for (i = 0, sptr = src; i < num_bytes; i++) {
        rc = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, sptr[i]);
        if (rc != 0) {
            return rc;
        }
        address += 2;
    }

    return 0;
}

static int
stm32f1_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    FLASH_EraseInitTypeDef eraseinit;
    uint32_t PageError;

    (void)PageError;

    if ((sector_address & (_FLASH_SECTOR_SIZE - 1)) == sector_address) {
        eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
        eraseinit.Banks = FLASH_BANK_1;
        eraseinit.PageAddress = sector_address;
        eraseinit.NbPages = _FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE;
        if (HAL_FLASHEx_Erase(&eraseinit, &PageError) == HAL_OK) {
            return 0;
        }
    }

    return -1;
}

static int
stm32f1_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    *address = dev->hf_base_addr + _FLASH_SECTOR_SIZE * idx;
    *sz = _FLASH_SECTOR_SIZE;
    return 0;
}

static int
stm32f1_flash_init(const struct hal_flash *dev)
{
    HAL_FLASH_Unlock();
    return 0;
}
