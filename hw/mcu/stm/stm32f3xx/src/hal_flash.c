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
#include "stm32f3xx_hal_def.h"
#include "stm32f3xx_hal_flash.h"
#include "stm32f3xx_hal_flash_ex.h"
#include "hal/hal_flash_int.h"

/*
 * Flash is organized in FLASH_PAGE_SIZE blocks, which for the
 * STM32F3 family is 2KB.
 * */
#define HAL_FLASH_SECTOR_SIZE   FLASH_PAGE_SIZE
#define HAL_FLASH_SIZE          ((*((uint16_t*)FLASH_SIZE_DATA_REGISTER)) * 1024U)


static int stm32f3_flash_read(const struct hal_flash *dev, uint32_t address,
        void *dst, uint32_t num_bytes);
static int stm32f3_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes);
static int stm32f3_flash_erase_sector(const struct hal_flash *dev,
        uint32_t sector_address);
static int stm32f3_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz);
static int stm32f3_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs stm32f3_flash_funcs = {
    .hff_read = stm32f3_flash_read,
    .hff_write = stm32f3_flash_write,
    .hff_erase_sector = stm32f3_flash_erase_sector,
    .hff_sector_info = stm32f3_flash_sector_info,
    .hff_init = stm32f3_flash_init
};

struct hal_flash stm32f3_flash_dev_;

static int
stm32f3_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
        uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

static int
stm32f3_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    const uint16_t *sptr;
    uint32_t i, half_words;
    int rc;

    sptr = src;
    half_words = (num_bytes + 1) / 2;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR);

    for (i = 0, rc = HAL_OK; i < half_words && rc == HAL_OK; ++i) {
        rc = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, sptr[i]);
        address += 2;
    }
    HAL_FLASH_Lock();

    return rc == HAL_OK ? 0 : rc;
}

static int
stm32f3_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    FLASH_EraseInitTypeDef erase;
    int rc = -1;
    uint32_t errorPage = -1;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = sector_address;
    erase.NbPages = 1;

    HAL_FLASH_Unlock();
    if (HAL_OK == HAL_FLASHEx_Erase(&erase, &errorPage)) {
      rc = 0;
    }
    HAL_FLASH_Lock();

    return rc;
}

static int
stm32f3_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    *address = FLASH_BASE + HAL_FLASH_SECTOR_SIZE * idx;
    *sz = HAL_FLASH_SECTOR_SIZE;
    return 0;
}

static int
stm32f3_flash_init(const struct hal_flash *dev)
{
    HAL_FLASH_Lock();
    return 0;
}

struct hal_flash*
stm32f3_flash_dev()
{
    if (0 == stm32f3_flash_dev_.hf_itf) {
        stm32f3_flash_dev_.hf_itf = &stm32f3_flash_funcs;
        stm32f3_flash_dev_.hf_base_addr = FLASH_BASE;
        stm32f3_flash_dev_.hf_size = HAL_FLASH_SIZE;
        stm32f3_flash_dev_.hf_sector_cnt = HAL_FLASH_SIZE / HAL_FLASH_SECTOR_SIZE;
        stm32f3_flash_dev_.hf_align = 2;
    }
    return &stm32f3_flash_dev_;
}
