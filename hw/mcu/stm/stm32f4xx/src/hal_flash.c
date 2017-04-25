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
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "hal/hal_flash_int.h"

static int stm32f4_flash_read(const struct hal_flash *dev, uint32_t address,
        void *dst, uint32_t num_bytes);
static int stm32f4_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes);
static int stm32f4_flash_erase_sector(const struct hal_flash *dev,
        uint32_t sector_address);
static int stm32f4_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz);
static int stm32f4_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs stm32f4_flash_funcs = {
    .hff_read = stm32f4_flash_read,
    .hff_write = stm32f4_flash_write,
    .hff_erase_sector = stm32f4_flash_erase_sector,
    .hff_sector_info = stm32f4_flash_sector_info,
    .hff_init = stm32f4_flash_init
};

static const uint32_t stm32f4_flash_sectors[] = {
    0x08000000,     /* 16kB */
    0x08004000,     /* 16kB */
    0x08008000,     /* 16kB */
    0x0800c000,     /* 16kB */
    0x08010000,     /* 64kB */
    0x08020000,     /* 128kB */
    0x08040000,     /* 128kB */
    0x08060000,     /* 128kB */
    0x08080000,     /* 128kB */
    0x080a0000,     /* 128kB */
    0x080c0000,     /* 128kB */
    0x080e0000,     /* 128kB */
    0x08100000      /* End of flash */
};

#define STM32F4_FLASH_NUM_AREAS                                         \
    (int)(sizeof(stm32f4_flash_sectors) /                               \
      sizeof(stm32f4_flash_sectors[0]))

const struct hal_flash stm32f4_flash_dev = {
    .hf_itf = &stm32f4_flash_funcs,
    .hf_base_addr = 0x08000000,
    .hf_size = 1024 * 1024,
    .hf_sector_cnt = STM32F4_FLASH_NUM_AREAS - 1,
    .hf_align = 1
};

static int
stm32f4_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
        uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

static int
stm32f4_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    const uint8_t *sptr;
    uint32_t i;
    int rc;

    sptr = src;
    /*
     * Clear status of previous operation.
     */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | \
      FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR| FLASH_FLAG_PGSERR);
    for (i = 0; i < num_bytes; i++) {
        rc = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address, sptr[i]);
        if (rc != 0) {
            return rc;
        }

        address++;
    }

    return 0;
}

static void
stm32f4_flash_erase_sector_id(int sector_id)
{
    FLASH_EraseInitTypeDef eraseinit;
    uint32_t SectorError;
    
    eraseinit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseinit.Banks = 0;
    eraseinit.Sector = sector_id;
    eraseinit.NbSectors = 1;
    eraseinit.VoltageRange = FLASH_VOLTAGE_RANGE_1; 

    HAL_FLASHEx_Erase(&eraseinit, &SectorError);
}

static int
stm32f4_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    int i;

    for (i = 0; i < STM32F4_FLASH_NUM_AREAS - 1; i++) {
        if (stm32f4_flash_sectors[i] == sector_address) {
            stm32f4_flash_erase_sector_id(i);
            return 0;
        }
    }

    return -1;
}

static int
stm32f4_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    *address = stm32f4_flash_sectors[idx];
    *sz = stm32f4_flash_sectors[idx + 1] - stm32f4_flash_sectors[idx];
    return 0;
}

static int
stm32f4_flash_init(const struct hal_flash *dev)
{
    HAL_FLASH_Unlock();
    return 0;
}
