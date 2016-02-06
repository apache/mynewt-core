/**
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
 * Internal flash for STM32F3.
 * Size of the flash depends on the MCU model, flash is memory mapped
 * and is divided to 2k sectors throughout.
 * Programming is done 2 bytes at a time.
 */
#include <string.h>
#include <assert.h>
#include <hal/hal_flash_int.h>

#include "mcu/stm32f30x.h"
#include "mcu/stm32f30x_flash.h"

#define STM32F3_BASE_ADDR               0x08000000
#define STM32F3_ADDR_2_SEC_IDX(a)       (((a) - STM32F3_BASE_ADDR) / 2048)
#define STM32F3_SEC_IDX_2_ADDR(idx)     (STM32F3_BASE_ADDR + ((idx) * 2048))

static int stm32f3_flash_read(uint32_t address, void *dst, uint32_t num_bytes);
static int stm32f3_flash_write(uint32_t address, const void *src,
  uint32_t num_bytes);
static int stm32f3_flash_erase_sector(uint32_t sector_address);
static int stm32f3_flash_sector_info(int idx, uint32_t *addr, uint32_t *sz);
static int stm32f3_flash_init(void);

static const struct hal_flash_funcs stm32f3_flash_funcs = {
    .hff_read = stm32f3_flash_read,
    .hff_write = stm32f3_flash_write,
    .hff_erase_sector = stm32f3_flash_erase_sector,
    .hff_sector_info = stm32f3_flash_sector_info,
    .hff_init = stm32f3_flash_init
};

const struct hal_flash stm32f3_flash_dev = {
    .hf_itf = &stm32f3_flash_funcs,
    .hf_base_addr = STM32F3_BASE_ADDR,
#ifdef STM32F303xC
    .hf_size = 256 * 1024,
    .hf_sector_cnt = 128,
#endif
    .hf_align = 2
};

static int
stm32f3_flash_read(uint32_t address, void *dst, uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

static int
stm32f3_flash_write(uint32_t address, const void *src, uint32_t len)
{
    FLASH_Status rc;
    int num_half_words;
    int i;
    uint16_t *src16 = (uint16_t *)src;

    if (address % sizeof(uint16_t)) {
        /*
         * Unaligned write.
         */
        return -1;
    }
    num_half_words = len / 2;

    for (i = 0; i < num_half_words; i++) {
        rc = FLASH_ProgramHalfWord(address, src16[i]);
        if (rc != FLASH_COMPLETE) {
            goto err;
        }
        address += 2;
    }
    if (num_half_words * 2 != len) {
        rc = FLASH_ProgramHalfWord(address, ((uint8_t *)src)[len] << 8);
        if (rc != FLASH_COMPLETE) {
            goto err;
        }
    }
    return 0;
err:
    return -1;
}

static int
stm32f3_flash_erase_sector(uint32_t sector_address)
{
    if (FLASH_ErasePage(sector_address) == FLASH_COMPLETE) {
        return 0;
    }
    return -1;
}

static int
stm32f3_flash_sector_info(int idx, uint32_t *addr, uint32_t *sz)
{
    assert(idx < stm32f3_flash_dev.hf_sector_cnt);
    *addr = STM32F3_SEC_IDX_2_ADDR(idx);
    *sz = 2048;
    return 0;
}

static int
stm32f3_flash_init(void)
{
    FLASH_Unlock();
    return 0;
}
