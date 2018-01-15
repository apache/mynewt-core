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
#include "stm32l1xx_hal_def.h"
#include "stm32l1xx_hal_flash.h"
#include "stm32l1xx_hal_flash_ex.h"
#include "hal/hal_flash_int.h"
#include "hal/hal_watchdog.h"

static int stm32l1_flash_read(const struct hal_flash *dev, uint32_t address,
        void *dst, uint32_t num_bytes);
static int stm32l1_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes);
static int stm32l1_flash_erase_sector(const struct hal_flash *dev,
        uint32_t sector_address);
static int stm32l1_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz);
static int stm32l1_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs stm32l1_flash_funcs = {
    .hff_read = stm32l1_flash_read,
    .hff_write = stm32l1_flash_write,
    .hff_erase_sector = stm32l1_flash_erase_sector,
    .hff_sector_info = stm32l1_flash_sector_info,
    .hff_init = stm32l1_flash_init,
};

#define _FLASH_SIZE            (256 * 1024)
#define _FLASH_SECTOR_SIZE     4096

const struct hal_flash stm32l1_flash_dev = {
    .hf_itf = &stm32l1_flash_funcs,
    .hf_base_addr = 0x08000000,
    .hf_size = _FLASH_SIZE,
    .hf_sector_cnt = _FLASH_SIZE / _FLASH_SECTOR_SIZE,
    .hf_align = 8,
};

static int
stm32l1_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
        uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

static int
stm32l1_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    const uint32_t *sptr;
    uint32_t i;
    int rc;
    uint32_t num_words;

    if (!num_bytes) {
        return -1;
    }

    /* Clear status of previous operation. */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_MASK);

    sptr = (const uint32_t *)src;
    num_words = ((num_bytes - 1) / 4) + 1;
    for (i = 0; i < num_words; i++) {
        rc = HAL_FLASH_Program(FLASH_TYPEPROGRAMDATA_WORD, address, sptr[i]);
        if (rc != HAL_OK) {
            return rc;
        }
        address += 4;

        /*
         * Long writes take excessive time, and stall the idle
         * thread, so tickling the watchdog here to avoid resetting
         * during writes...
         */
        if (!(i % 32)) {
            hal_watchdog_tickle();
        }
    }

    return 0;
}

static int
stm32l1_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    FLASH_EraseInitTypeDef eraseinit;
    uint32_t PageError;
    HAL_StatusTypeDef rc;

    (void)PageError;

    if (!(sector_address & (_FLASH_SECTOR_SIZE - 1))) {
        /* FIXME: why is an err flag set? */

        /* Clear status of previous operation. */
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_MASK);

        eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
        eraseinit.PageAddress = sector_address;
        eraseinit.NbPages = _FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE;
        rc = HAL_FLASHEx_Erase(&eraseinit, &PageError);
        if (rc == HAL_OK) {
            return 0;
        }
    }

    return -1;
}

static int
stm32l1_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    *address = dev->hf_base_addr + _FLASH_SECTOR_SIZE * idx;
    *sz = _FLASH_SECTOR_SIZE;
    return 0;
}

static int
stm32l1_flash_init(const struct hal_flash *dev)
{
    HAL_FLASH_Unlock();
    /* TODO: enable ACC64 + prefetch */
    return 0;
}
