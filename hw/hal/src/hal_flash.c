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
#include <inttypes.h>
#include <assert.h>

#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "hal/hal_flash_int.h"

int
hal_flash_init(void)
{
    const struct hal_flash *hf;
    uint8_t i;
    int rc = 0;

    for (i = 0; ; i++) {
        hf = hal_bsp_flash_dev(i);
        if (!hf) {
            break;
        }
        if (hf->hf_itf->hff_init(hf)) {
            rc = -1;
        }
    }
    return rc;
}

uint8_t
hal_flash_align(uint8_t flash_id)
{
    const struct hal_flash *hf;

    hf = hal_bsp_flash_dev(flash_id);
    if (!hf) {
        return 1;
    }
    return hf->hf_align;
}

uint32_t
hal_flash_sector_size(const struct hal_flash *hf, int sec_idx)
{
    uint32_t size;
    uint32_t start;

    if (hf->hf_itf->hff_sector_info(hf, sec_idx, &start, &size)) {
        return 0;
    }
    return size;
}

static int
hal_flash_check_addr(const struct hal_flash *hf, uint32_t addr)
{
    if (addr < hf->hf_base_addr || addr > hf->hf_base_addr + hf->hf_size) {
        return -1;
    }
    return 0;
}

int
hal_flash_read(uint8_t id, uint32_t address, void *dst, uint32_t num_bytes)
{
    const struct hal_flash *hf;

    hf = hal_bsp_flash_dev(id);
    if (!hf) {
        return -1;
    }
    if (hal_flash_check_addr(hf, address) ||
      hal_flash_check_addr(hf, address + num_bytes)) {
        return -1;
    }
    return hf->hf_itf->hff_read(hf, address, dst, num_bytes);
}

int
hal_flash_write(uint8_t id, uint32_t address, const void *src,
  uint32_t num_bytes)
{
    const struct hal_flash *hf;

    hf = hal_bsp_flash_dev(id);
    if (!hf) {
        return -1;
    }
    if (hal_flash_check_addr(hf, address) ||
      hal_flash_check_addr(hf, address + num_bytes)) {
        return -1;
    }
    return hf->hf_itf->hff_write(hf, address, src, num_bytes);
}

int
hal_flash_erase_sector(uint8_t id, uint32_t sector_address)
{
    const struct hal_flash *hf;

    hf = hal_bsp_flash_dev(id);
    if (!hf) {
        return -1;
    }
    if (hal_flash_check_addr(hf, sector_address)) {
        return -1;
    }
    return hf->hf_itf->hff_erase_sector(hf, sector_address);
}

int
hal_flash_erase(uint8_t id, uint32_t address, uint32_t num_bytes)
{
    const struct hal_flash *hf;
    uint32_t start, size;
    uint32_t end;
    uint32_t end_area;
    int i;
    int rc;

    hf = hal_bsp_flash_dev(id);
    if (!hf) {
        return -1;
    }
    if (hal_flash_check_addr(hf, address) ||
      hal_flash_check_addr(hf, address + num_bytes)) {
        return -1;
    }

    end = address + num_bytes;
    if (end <= address) {
        /*
         * Check for wrap-around.
         */
        return -1;
    }

    for (i = 0; i < hf->hf_sector_cnt; i++) {
        rc = hf->hf_itf->hff_sector_info(hf, i, &start, &size);
        assert(rc == 0);
        end_area = start + size;
        if (address < end_area && end > start) {
            /*
             * If some region of eraseable area falls inside sector,
             * erase the sector.
             */
            if (hf->hf_itf->hff_erase_sector(hf, start)) {
                return -1;
            }
        }
    }
    return 0;
}

int
hal_flash_ioctl(uint8_t id, uint32_t cmd, void *args)
{
    return 0;
}
