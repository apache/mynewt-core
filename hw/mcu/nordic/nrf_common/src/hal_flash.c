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

/* XXX: nrfx nvmc driver does not support NRF54 devices yet */
#ifndef NRF54L15_XXAA
#include <string.h>
#include <assert.h>
#include <hal/hal_flash_int.h>
#include <nrf.h>
#include <nrf_hal.h>
#include <nrfx_nvmc.h>

#ifdef NRF51
#define NRF_FLASH_SECTOR_SZ 1024
#else
#define NRF_FLASH_SECTOR_SZ 4096
#endif

#define NRF_FLASH_SECTOR_CNT (NRF_MEMORY_FLASH_SIZE / NRF_FLASH_SECTOR_SZ)

static int nrf_flash_read(const struct hal_flash *dev, uint32_t address,
                          void *dst, uint32_t num_bytes);
static int nrf_flash_write(const struct hal_flash *dev, uint32_t address,
                           const void *src, uint32_t num_bytes);
static int nrf_flash_erase_sector(const struct hal_flash *dev,
                                  uint32_t sector_address);
static int nrf_flash_sector_info(const struct hal_flash *dev, int idx,
                                 uint32_t *address, uint32_t *sz);
static int nrf_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs nrf_flash_funcs = {
    .hff_read = nrf_flash_read,
    .hff_write = nrf_flash_write,
    .hff_erase_sector = nrf_flash_erase_sector,
    .hff_sector_info = nrf_flash_sector_info,
    .hff_init = nrf_flash_init
};

const struct hal_flash nrf_flash_dev = {
    .hf_itf = &nrf_flash_funcs,
    .hf_base_addr = NRF_MEMORY_FLASH_BASE,
    .hf_size = NRF_MEMORY_FLASH_SIZE,
    .hf_sector_cnt = NRF_FLASH_SECTOR_CNT,
    .hf_align = MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE),
    .hf_erased_val = 0xff,
};

static int
nrf_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
               uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

static int
nrf_flash_write(const struct hal_flash *dev, uint32_t address,
                const void *src, uint32_t num_bytes)
{
    int sr;

    __HAL_DISABLE_INTERRUPTS(sr);
    nrfx_nvmc_bytes_write(address, src, num_bytes);
    __HAL_ENABLE_INTERRUPTS(sr);

    return 0;
}

static int
nrf_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    int sr;
    int rc;

    __HAL_DISABLE_INTERRUPTS(sr);
    rc = nrfx_nvmc_page_erase(sector_address);
    __HAL_ENABLE_INTERRUPTS(sr);

    if (rc != NRFX_SUCCESS) {
        return -1;
    } else {
        return 0;
    }
}

static int
nrf_flash_sector_info(const struct hal_flash *dev, int idx,
                      uint32_t *address, uint32_t *sz)
{
    assert(idx < nrf_flash_dev.hf_sector_cnt);
    *address = dev->hf_base_addr + idx * NRF_FLASH_SECTOR_SZ;
    *sz = NRF_FLASH_SECTOR_SZ;
    return 0;
}

static int
nrf_flash_init(const struct hal_flash *dev)
{
    return 0;
}
#endif
