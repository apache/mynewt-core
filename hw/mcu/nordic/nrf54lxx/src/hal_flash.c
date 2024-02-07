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
#include <assert.h>
#include <mcu/nrf54l_hal.h>
#include <hal/hal_flash_int.h>
#include <nrf_mem.h>

#define NRF54L_FLASH_SECTOR_SZ 4096

static int
nrf54l_flash_wait_ready(void)
{
    int i;

    for (i = 0; i < 100000; i++) {
        if (NRF_RRAMC->READY == RRAMC_READY_READY_Ready) {
            return 0;
        }
    }
    return -1;
}

static int
nrf54l_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
                  uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

/*
 * Flash write is done by writing 4 bytes at a time at a word boundary.
 */
static int
nrf54l_flash_write(const struct hal_flash *dev, uint32_t address,
                   const void *src, uint32_t num_bytes)
{
    int sr;
    int rc = -1;
    uint32_t val;
    int cnt;
    uint32_t tmp;

    if (nrf54l_flash_wait_ready()) {
        return -1;
    }
    __HAL_DISABLE_INTERRUPTS(sr);

    NRF_RRAMC->CONFIG = RRAMC_CONFIG_WEN_Enabled; /* Enable erase OP */

    tmp = address & 0x3;
    if (tmp) {
        if (nrf54l_flash_wait_ready()) {
            goto out;
        }
        /*
         * Starts at a non-word boundary. Read 4 bytes which were there
         * before, update with new data, and write back.
         */
        val = *(uint32_t *)(address & ~0x3);
        cnt = 4 - tmp;
        if (cnt > num_bytes) {
            cnt = num_bytes;
        }
        memcpy((uint8_t *)&val + tmp, src, cnt);
        *(uint32_t *)(address & ~0x3) = val;
        address += cnt;
        num_bytes -= cnt;
        src += cnt;
    }

    while (num_bytes >= sizeof(uint32_t)) {
        /*
         * Write data 4 bytes at a time.
         */
        if (nrf54l_flash_wait_ready()) {
            goto out;
        }
        *(uint32_t *)address = *(uint32_t *)src;
        address += sizeof(uint32_t);
        src += sizeof(uint32_t);
        num_bytes -= sizeof(uint32_t);
    }
    if (num_bytes) {
        /*
         * Deal with the trailing bytes.
         */
        val = *(uint32_t *)address;
        memcpy(&val, src, num_bytes);
        if (nrf54l_flash_wait_ready()) {
            goto out;
        }
        *(uint32_t *)address = val;
    }

    rc = nrf54l_flash_wait_ready();
out:
    NRF_RRAMC->CONFIG = RRAMC_CONFIG_WEN_Disabled;
    __HAL_ENABLE_INTERRUPTS(sr);
    return rc;
}

static int
nrf54l_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    uint32_t val = 0xffffffff;

    return nrf54l_flash_write(dev, sector_address, &val, sizeof(val));
}

static int
nrf54l_flash_sector_info(const struct hal_flash *dev, int idx,
                         uint32_t *address, uint32_t *sz)
{
    assert(idx < nrf54l_flash_dev.hf_sector_cnt);
    *address = idx * NRF54L_FLASH_SECTOR_SZ;
    *sz = NRF54L_FLASH_SECTOR_SZ;
    return 0;
}

static int
nrf54l_flash_init(const struct hal_flash *dev)
{
    return 0;
}

static const struct hal_flash_funcs nrf54l_flash_funcs = {
    .hff_read = nrf54l_flash_read,
    .hff_write = nrf54l_flash_write,
    .hff_erase_sector = nrf54l_flash_erase_sector,
    .hff_sector_info = nrf54l_flash_sector_info,
    .hff_init = nrf54l_flash_init
};

const struct hal_flash nrf54l_flash_dev = {
    .hf_itf = &nrf54l_flash_funcs,
    .hf_base_addr = NRF_MEMORY_FLASH_BASE,
    .hf_size = NRF_MEMORY_FLASH_SIZE,
    .hf_sector_cnt = NRF54L_FLASH_SECTOR_SZ,
    .hf_align = MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE),
    .hf_erased_val = 0xff,
};
