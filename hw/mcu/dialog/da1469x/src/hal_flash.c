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

#include <assert.h>
#include <string.h>
#include "mcu/mcu.h"
#include "hal/hal_flash_int.h"

static int da1469x_hff_read(const struct hal_flash *dev, uint32_t address,
                            void *dst, uint32_t num_bytes);
static int da1469x_hff_write(const struct hal_flash *dev, uint32_t address,
                             const void *src, uint32_t num_bytes);
static int da1469x_hff_erase_sector(const struct hal_flash *dev,
                                    uint32_t sector_address);
static int da1469x_hff_sector_info(const struct hal_flash *dev, int idx,
                                   uint32_t *address, uint32_t *sz);
static int da1469x_hff_init(const struct hal_flash *dev);

static const struct hal_flash_funcs da1469x_flash_funcs = {
    .hff_read = da1469x_hff_read,
    .hff_write = da1469x_hff_write,
    .hff_erase_sector = da1469x_hff_erase_sector,
    .hff_sector_info = da1469x_hff_sector_info,
    .hff_init = da1469x_hff_init
};

const struct hal_flash da1469x_flash_dev = {
    .hf_itf = &da1469x_flash_funcs,
    .hf_base_addr = 0,
    .hf_size = 1024 * 1024, /* W25Q80EWSNIG: 8Mb = 1MB */
    .hf_sector_cnt = 256,   /* W25Q80EWSNIG: 4KB sector */
    .hf_align = 1,
    .hf_erased_val = 0xff,
};

static int
da1469x_hff_read(const struct hal_flash *dev, uint32_t address, void *dst,
                 uint32_t num_bytes)
{
    if (address + num_bytes >= dev->hf_size) {
        return -1;
    }

    address += MCU_MEM_QSPIF_M_START_ADDRESS;

    memcpy(dst, (void *)address, num_bytes);

    return 0;
}

static int
da1469x_hff_write(const struct hal_flash *dev, uint32_t address,
                  const void *src, uint32_t num_bytes)
{
    assert(0);
    return -1;
}

static int
da1469x_hff_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    assert(0);
    return -1;
}

static int
da1469x_hff_sector_info(const struct hal_flash *dev, int idx,
                        uint32_t *address, uint32_t *sz)
{
    assert(idx < dev->hf_sector_cnt);

    /* XXX assume uniform sector size */
    *sz = dev->hf_size / dev->hf_sector_cnt;
    *address = idx * (*sz);

    return 0;
}

static int
da1469x_hff_init(const struct hal_flash *dev)
{
    return 0;
}
