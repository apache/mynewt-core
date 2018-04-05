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

#include <assert.h>
#include <string.h>
#include "os/mynewt.h"
#include <am_hal_flash.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_flash.h>
#include <mcu/system_apollo2.h>


static int
apollo2_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
    uint32_t num_bytes);
static int
apollo2_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes);
static int
apollo2_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_addr);
static int
apollo2_flash_sector_info(const struct hal_flash *dev, int idx, uint32_t *addr,
    uint32_t *sz);
static int
apollo2_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs apollo2_flash_funcs = {
    .hff_read = apollo2_flash_read,
    .hff_write = apollo2_flash_write,
    .hff_erase_sector = apollo2_flash_erase_sector,
    .hff_sector_info = apollo2_flash_sector_info,
    .hff_init = apollo2_flash_init
};

const struct hal_flash apollo2_flash_dev = {
        .hf_itf = &apollo2_flash_funcs,
        .hf_base_addr = 0x00000000,
        .hf_size = 1024 * 1024,
        .hf_sector_cnt = 128,
        .hf_align = 1
};

static int
apollo2_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
    uint32_t num_bytes)
{
    memcpy(dst, (void *) address, num_bytes);

    return (0);
}

static int
apollo2_flash_write_odd(const struct hal_flash *dev, uint32_t address,
                        const void *src, uint32_t num_bytes)
{
    uint32_t *base;
    uint32_t word;
    uint8_t *u8p;
    int offset;
    int rc;

    offset = address % 4;
    assert(offset + num_bytes <= 4);

    base = (uint32_t *)(address - offset);
    word = *base;

    u8p = (uint8_t *)&word;
    u8p += offset;
    memcpy(u8p, src, num_bytes);

    rc = am_hal_flash_program_main(AM_HAL_FLASH_PROGRAM_KEY, &word,
                                   base, 1);
    return rc;
}

static int
apollo2_flash_write(const struct hal_flash *dev, uint32_t address,
    const void *src, uint32_t num_bytes)
{
    const uint8_t *u8p;
    int lead_size;
    int lead_off;
    int words;
    int sr;
    int remainder;
    int rc;
    int i;

    __HAL_DISABLE_INTERRUPTS(sr);

    u8p = src;

    /* Write leading partial word, if any. */
    lead_off = address % 4;
    if (lead_off != 0) {
        lead_size = 4 - lead_off;
        if (lead_size > num_bytes) {
            lead_size = num_bytes;
        }

        rc = apollo2_flash_write_odd(dev, address, u8p, lead_size);
        if (rc != 0) {
            goto done;
        }

        u8p += lead_size;
        num_bytes -= lead_size;
        address += lead_size;
    }

    if (num_bytes == 0) {
        rc = 0;
        goto done;
    }

    /* Write aligned words in the middle. */
    words = num_bytes / 4;
    if ((uint32_t)u8p % 4 == 0) {
        rc = am_hal_flash_program_main(AM_HAL_FLASH_PROGRAM_KEY,
                                       (uint32_t *)u8p, (uint32_t *)address,
                                       words);
        if (rc != 0) {
            goto done;
        }
    } else {
        for (i = 0; i < words; i++) {
            rc = apollo2_flash_write_odd(dev, address + i * 4, u8p + i * 4, 4);
            if (rc != 0) {
                goto done;
            }
        }
    }

    /* Write trailing partial word, if any. */
    remainder = num_bytes - (words * 4);
    if (remainder > 0) {
        rc = apollo2_flash_write_odd(dev,
                                     address + num_bytes - remainder,
                                     u8p + num_bytes - remainder,
                                     remainder);
        if (rc != 0) {
            goto done;
        }
    }

    rc = 0;

done:
    __HAL_ENABLE_INTERRUPTS(sr);
    return rc;
}

static int
apollo2_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_addr)
{
    uint32_t inst;
    uint32_t page;
    int rc;

    inst = AM_HAL_FLASH_ADDR2INST(sector_addr);
    page = AM_HAL_FLASH_ADDR2PAGE(sector_addr);

    rc = am_hal_flash_page_erase(AM_HAL_FLASH_PROGRAM_KEY, inst, page);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int
apollo2_flash_sector_info(const struct hal_flash *dev, int idx, uint32_t *addr,
    uint32_t *sz)
{
    *addr = idx * AM_HAL_FLASH_PAGE_SIZE;
    *sz = AM_HAL_FLASH_PAGE_SIZE;

    return (0);
}

static int
apollo2_flash_init(const struct hal_flash *dev)
{
    return (0);
}
