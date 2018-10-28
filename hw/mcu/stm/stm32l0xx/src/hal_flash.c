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
#include "stm32l0xx_hal_def.h"
#include "stm32l0xx_hal_rcc.h"
#include "stm32l0xx_hal_flash.h"
#include "stm32l0xx_hal_flash_ex.h"
#include "hal/hal_flash_int.h"
#include "hal/hal_watchdog.h"

static int stm32l0_flash_read(const struct hal_flash *dev, uint32_t address,
        void *dst, uint32_t num_bytes);
static int stm32l0_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes);
static int stm32l0_flash_erase_sector(const struct hal_flash *dev,
        uint32_t sector_address);
static int stm32l0_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz);
static int stm32l0_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs stm32l0_flash_funcs = {
    .hff_read = stm32l0_flash_read,
    .hff_write = stm32l0_flash_write,
    .hff_erase_sector = stm32l0_flash_erase_sector,
    .hff_sector_info = stm32l0_flash_sector_info,
    .hff_init = stm32l0_flash_init,
};

#define _FLASH_SIZE              (192 * 1024)
/*
 * NOTE: the actual page size if 128 bytes, but that would mean an
 * enormous amount of pages, so here make it look like 1K.
 */
#define _FLASH_SECTOR_SIZE       1024
#define _FLASH_PAGES_PER_SECTOR  8

const struct hal_flash stm32l0_flash_dev = {
    .hf_itf = &stm32l0_flash_funcs,
    .hf_base_addr = 0x08000000,
    .hf_size = _FLASH_SIZE,
    .hf_sector_cnt = _FLASH_SIZE / _FLASH_SECTOR_SIZE,
    /* FIXME: 2 also ok? */
    .hf_align = 4,
    .hf_erased_val = 0,
};

static int
stm32l0_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
        uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

static int
stm32l0_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    uint64_t val;
    uint8_t align;
    uint32_t i;
    int rc;
    uint32_t num_words;

    if (!num_bytes) {
        return -1;
    }

    align = dev->hf_align;
    num_words = ((num_bytes - 1) / align) + 1;

    /* clear previous errors */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP |
                    FLASH_FLAG_ENDHV      |
                    FLASH_FLAG_READY      |
                    FLASH_FLAG_WRPERR     |
                    FLASH_FLAG_PGAERR     |
                    FLASH_FLAG_SIZERR     |
                    FLASH_FLAG_OPTVERR    |
                    FLASH_FLAG_RDERR      |
                    FLASH_FLAG_FWWERR     |
                    FLASH_FLAG_NOTZEROERR);

    for (i = 0; i < num_words; i++) {
        //__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGSERR);

        if (num_bytes < align) {
            memcpy(&val, &((uint8_t *)src)[i * align], num_bytes);
            memset((uint32_t *)&val + num_bytes, 0, align - num_bytes);
        } else {
            memcpy(&val, &((uint8_t *)src)[i * align], align);
        }

        rc = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, val);
        if (rc != HAL_OK) {
            return rc;
        }

        address += align;

        /* underflowing is ok here... */
        num_bytes -= align;

        /*
         * Long writes take excessive time, and stall the idle thread, so
         * tickling the watchdog here to avoid resetting during writes...
         */
        if (!(i % 32)) {
            hal_watchdog_tickle();
        }
    }

    return 0;
}

static int
stm32l0_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    FLASH_EraseInitTypeDef eraseinit;
    uint32_t PageError;
    HAL_StatusTypeDef rc;

    (void)PageError;

    if (!(sector_address & (_FLASH_SECTOR_SIZE - 1))) {
        /* FIXME: why is an err flag set? */

        /* Clear status of previous operation. */
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP |
                        FLASH_FLAG_ENDHV      |
                        FLASH_FLAG_READY      |
                        FLASH_FLAG_WRPERR     |
                        FLASH_FLAG_PGAERR     |
                        FLASH_FLAG_SIZERR     |
                        FLASH_FLAG_OPTVERR    |
                        FLASH_FLAG_RDERR      |
                        FLASH_FLAG_FWWERR     |
                        FLASH_FLAG_NOTZEROERR);

        eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
        eraseinit.PageAddress = sector_address;
        eraseinit.NbPages = _FLASH_PAGES_PER_SECTOR;
        rc = HAL_FLASHEx_Erase(&eraseinit, &PageError);
        if (rc == HAL_OK) {
            return 0;
        }
    }

    return -1;
}

static int
stm32l0_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    *address = dev->hf_base_addr + _FLASH_SECTOR_SIZE * idx;
    *sz = _FLASH_SECTOR_SIZE;
    return 0;
}

static int
stm32l0_flash_init(const struct hal_flash *dev)
{
    //__HAL_FLASH_BUFFER_CACHE_DISABLE();
    //__HAL_FLASH_PREREAD_BUFFER_ENABLE();
    //__HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    __HAL_RCC_MIF_CLK_ENABLE();
    HAL_FLASH_Unlock();
    return 0;
}
