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
#include <os/mynewt.h>
#include <mcu/stm32_hal.h>
#include "hal/hal_flash_int.h"
#include "hal/hal_watchdog.h"

#define FLASH_IS_LINEAR    MYNEWT_VAL(STM32_FLASH_IS_LINEAR)
#define FLASH_WRITE_SIZE   MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE)
#define FLASH_ERASED_VAL   MYNEWT_VAL(MCU_FLASH_ERASED_VAL)

#define _FLASH_SIZE         (MYNEWT_VAL(STM32_FLASH_SIZE_KB) * 1024)

#if FLASH_IS_LINEAR
#define _FLASH_SECTOR_SIZE  MYNEWT_VAL(STM32_FLASH_SECTOR_SIZE)
#endif

static int stm32_flash_read(const struct hal_flash *dev, uint32_t address,
        void *dst, uint32_t num_bytes);
static int stm32_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes);
static int stm32_flash_erase_sector(const struct hal_flash *dev,
        uint32_t sector_address);
static int stm32_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz);
static int stm32_flash_init(const struct hal_flash *dev);

const struct hal_flash_funcs stm32_flash_funcs = {
    .hff_read = stm32_flash_read,
    .hff_write = stm32_flash_write,
    .hff_erase_sector = stm32_flash_erase_sector,
    .hff_sector_info = stm32_flash_sector_info,
    .hff_init = stm32_flash_init
};

#if !FLASH_IS_LINEAR
extern const uint32_t stm32_flash_sectors[];
#define FLASH_NUM_AREAS MYNEWT_VAL(STM32_FLASH_NUM_AREAS)
#else
#define FLASH_NUM_AREAS (_FLASH_SIZE / _FLASH_SECTOR_SIZE)
#endif

const struct hal_flash stm32_flash_dev = {
    .hf_itf = &stm32_flash_funcs,
    .hf_base_addr = FLASH_BASE,
    .hf_size = _FLASH_SIZE,
    .hf_sector_cnt = FLASH_NUM_AREAS,
    .hf_align = FLASH_WRITE_SIZE,
    .hf_erased_val = FLASH_ERASED_VAL,
};

static int
stm32_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
        uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

#if FLASH_IS_LINEAR
static int
stm32_flash_write_linear(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    uint64_t val;
    uint32_t i;
    int rc;
    uint8_t align;
    uint32_t num_words;

    align = dev->hf_align;

#if MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE) == 1
    num_words = num_bytes;
#elif MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE) == 2
    num_words = ((num_bytes - 1) >> 1) + 1;
#elif MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE) == 4
    num_words = ((num_bytes - 1) >> 2) + 1;
#elif MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE) == 8
    num_words = ((num_bytes - 1) >> 3) + 1;
#else
    #error "Unsupported MCU_FLASH_MIN_WRITE_SIZE"
#endif

    /* FIXME: L4 didn't have error clearing here, should check if other families
     *        need this (or can be removed)! */
    STM32_HAL_FLASH_CLEAR_ERRORS();

    for (i = 0; i < num_words; i++) {
        if (num_bytes < align) {
            memcpy(&val, &((uint8_t *)src)[i * align], num_bytes);
            memset((uint32_t *)&val + num_bytes, dev->hf_erased_val, align - num_bytes);
        } else {
            memcpy(&val, &((uint8_t *)src)[i * align], align);
        }

        /* FIXME: L1 was previously unlocking flash before erasing/programming,
         * and locking again afterwards. Maybe all MCUs should do the same?
         */
        rc = HAL_FLASH_Program(FLASH_PROGRAM_TYPE, address, val);
        if (rc != HAL_OK) {
            return rc;
        }

        address += align;

        /* underflowing is ok here... */
        num_bytes -= align;

        /*
         * Long writes take excessive time, and stall the idle thread,
         * so tickling the watchdog here to avoid reset...
         */
        if (!(i % 32)) {
            hal_watchdog_tickle();
        }
    }

    return 0;
}
#endif

#if !FLASH_IS_LINEAR
static int
stm32_flash_write_non_linear(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    const uint8_t *sptr;
    uint32_t i;
    int rc;

    sptr = src;
    /*
     * Clear status of previous operation.
     */
    STM32_HAL_FLASH_CLEAR_ERRORS();

    for (i = 0; i < num_bytes; i++) {
        rc = HAL_FLASH_Program(FLASH_PROGRAM_TYPE, address, sptr[i]);
        if (rc != 0) {
            return rc;
        }

        address++;
    }

    return 0;
}
#endif

static int
stm32_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    if (!num_bytes) {
        return -1;
    }

#if FLASH_IS_LINEAR
    return stm32_flash_write_linear(dev, address, src, num_bytes);
#else
    return stm32_flash_write_non_linear(dev, address, src, num_bytes);
#endif
}

#if !FLASH_IS_LINEAR
static int
stm32_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    FLASH_EraseInitTypeDef eraseinit;
    HAL_StatusTypeDef err;
    uint32_t SectorError;
    int i;

    for (i = 0; i < dev->hf_sector_cnt; i++) {
        if (stm32_flash_sectors[i] == sector_address) {
            eraseinit.TypeErase = FLASH_TYPEERASE_SECTORS;
#ifdef FLASH_OPTCR_nDBANK
            eraseinit.Banks = FLASH_BANK_1; /* Only used for mass erase */
#endif
            eraseinit.Sector = i;
            eraseinit.NbSectors = 1;
            eraseinit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

            err = HAL_FLASHEx_Erase(&eraseinit, &SectorError);
            if (err) {
                return -1;
            }
            return 0;
        }
    }

    return -1;
}

#else /* FLASH_IS_LINEAR */

int stm32_mcu_flash_erase_sector(const struct hal_flash *, uint32_t);

static int
stm32_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    return stm32_mcu_flash_erase_sector(dev, sector_address);
}

#endif

static int
stm32_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    (void)dev;

#if FLASH_IS_LINEAR
    *address = dev->hf_base_addr + _FLASH_SECTOR_SIZE * idx;
    *sz = _FLASH_SECTOR_SIZE;
#else
    *address = stm32_flash_sectors[idx];
    *sz = stm32_flash_sectors[idx + 1] - stm32_flash_sectors[idx];
#endif

    return 0;
}

static int
stm32_flash_init(const struct hal_flash *dev)
{
    (void)dev;
    STM32_HAL_FLASH_INIT();
    return 0;
}
