/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <inttypes.h>
#include <bsp/bsp.h>

#include "hal/hal_flash.h"
#include "hal/hal_flash_int.h"

int
hal_flash_init(void)
{
    struct hal_flash *hf;
    uint8_t i;
    int rc = 0;

    for (i = 0; ; i++) {
        hf = bsp_flash_dev(i);
        if (!hf) {
            break;
        }
        if (hf->hf_itf->hff_init()) {
            rc = -1;
        }
    }
    return rc;
}

static int
hal_flash_check_addr(struct hal_flash *hf, uint32_t addr)
{
    if (addr < hf->hf_sectors[0] || addr > hf->hf_sectors[0] + hf->hf_size) {
        return -1;
    }
    return 0;
}

int
hal_flash_read(uint8_t id, uint32_t address, void *dst, uint32_t num_bytes)
{
    struct hal_flash *hf;

    hf = bsp_flash_dev(id);
    if (!hf) {
        return -1;
    }
    if (hal_flash_check_addr(hf, address) ||
      hal_flash_check_addr(hf, address + num_bytes)) {
        return -1;
    }
    return hf->hf_itf->hff_read(address, dst, num_bytes);
}

int
hal_flash_write(uint8_t id, uint32_t address, const void *src,
  uint32_t num_bytes)
{
    struct hal_flash *hf;

    hf = bsp_flash_dev(id);
    if (!hf) {
        return -1;
    }
    if (hal_flash_check_addr(hf, address) ||
      hal_flash_check_addr(hf, address + num_bytes)) {
        return -1;
    }
    return hf->hf_itf->hff_write(address, src, num_bytes);
}

int
hal_flash_erase_sector(uint8_t id, uint32_t sector_address)
{
    struct hal_flash *hf;

    hf = bsp_flash_dev(id);
    if (!hf) {
        return -1;
    }
    if (hal_flash_check_addr(hf, sector_address)) {
        return -1;
    }
    return hf->hf_itf->hff_erase_sector(sector_address);
}

int
hal_flash_erase(uint8_t id, uint32_t address, uint32_t num_bytes)
{
    struct hal_flash *hf;
    uint32_t end;
    uint32_t end_area;
    const uint32_t *area;
    int i;

    hf = bsp_flash_dev(id);
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
    area = hf->hf_sectors;

    for (i = 0; i < hf->hf_sector_cnt; i++) {
        if (i < hf->hf_sector_cnt) {
            end_area = area[i + 1];
        } else {
            end_area = hf->hf_sectors[0] + hf->hf_size;
        }
        if (address < end_area && end > area[i]) {
            /*
             * If some region of eraseable area falls inside sector,
             * erase the sector.
             */
            if (hf->hf_itf->hff_erase_sector(area[i])) {
                return -1;
            }
        }
    }
    return 0;
}
