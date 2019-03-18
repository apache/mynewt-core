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
#include <string.h>

#include "os/mynewt.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "hal/hal_flash_int.h"

static uint8_t protected_flash[1];

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
            rc = SYS_EIO;
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

uint8_t
hal_flash_erased_val(uint8_t flash_id)
{
    const struct hal_flash *hf;

    hf = hal_bsp_flash_dev(flash_id);
    if (!hf) {
        return 1;
    }
    return hf->hf_erased_val;
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
        return SYS_EINVAL;
    }
    return 0;
}

int
hal_flash_read(uint8_t id, uint32_t address, void *dst, uint32_t num_bytes)
{
    const struct hal_flash *hf;
    int rc;

    hf = hal_bsp_flash_dev(id);
    if (!hf) {
        return SYS_EINVAL;
    }
    if (hal_flash_check_addr(hf, address) ||
      hal_flash_check_addr(hf, address + num_bytes)) {
        return SYS_EINVAL;
    }

    rc = hf->hf_itf->hff_read(hf, address, dst, num_bytes);
    if (rc != 0) {
        return SYS_EIO;
    }

    return 0;
}

#if MYNEWT_VAL(HAL_FLASH_VERIFY_WRITES)
/**
 * Verifies that the specified range of flash contains the given contents.
 *
 * @return                      0 on success;
 *                              SYS_E[...] code on error;
 *                              1 on unexpected flash contents.
 */
static int
hal_flash_cmp(const struct hal_flash *hf, uint32_t address, const void *val,
  uint32_t num_bytes)
{
    uint8_t buf[MYNEWT_VAL(HAL_FLASH_VERIFY_BUF_SZ)];

    const uint8_t *u8p;
    uint32_t off;
    uint32_t rem;
    int chunk_sz;
    int rc;

    u8p = val;

    for (off = 0; off < num_bytes; off += sizeof buf) {
        rem = num_bytes - off;
        if (rem >= sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = rem;
        }

        rc = hf->hf_itf->hff_read(hf, address + off, buf, chunk_sz);
        if (rc != 0) {
            return SYS_EIO;
        }

        if (memcmp(buf, u8p + off, chunk_sz) != 0) {
            return 1;
        }
    }

    return 0;
}
#endif

int
hal_flash_write(uint8_t id, uint32_t address, const void *src,
  uint32_t num_bytes)
{
    const struct hal_flash *hf;
    int rc;

    hf = hal_bsp_flash_dev(id);
    if (!hf) {
        return SYS_EINVAL;
    }
    if (hal_flash_check_addr(hf, address) ||
      hal_flash_check_addr(hf, address + num_bytes)) {
        return SYS_EINVAL;
    }

    if (protected_flash[id / 8] & (1 << (id & 7))) {
        return SYS_EACCES;
    }

    rc = hf->hf_itf->hff_write(hf, address, src, num_bytes);
    if (rc != 0) {
        return SYS_EIO;
    }

#if MYNEWT_VAL(HAL_FLASH_VERIFY_WRITES)
    assert(hal_flash_cmp(hf, address, src, num_bytes) == 0);
#endif

    return 0;
}

int
hal_flash_erase_sector(uint8_t id, uint32_t sector_address)
{
    const struct hal_flash *hf;
    uint32_t start;
    uint32_t size;
    int rc;
    int i;

    (void) start;
    (void) size;
    (void) i;

    hf = hal_bsp_flash_dev(id);
    if (!hf) {
        return SYS_EINVAL;
    }
    if (hal_flash_check_addr(hf, sector_address)) {
        return SYS_EINVAL;
    }

    if (protected_flash[id / 8] & (1 << (id & 7))) {
        return SYS_EACCES;
    }

    rc = hf->hf_itf->hff_erase_sector(hf, sector_address);
    if (rc != 0) {
        return SYS_EIO;
    }

#if MYNEWT_VAL(HAL_FLASH_VERIFY_ERASES)
    /* Find the sector bounds so we can verify the erase. */
    for (i = 0; i < hf->hf_sector_cnt; i++) {
        rc = hf->hf_itf->hff_sector_info(hf, i, &start, &size);
        assert(rc == 0);

        if (sector_address == start) {
            assert(hal_flash_isempty_no_buf(id, start, size) == 1);
            break;
        }
    }
#endif

    return 0;
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
        return SYS_EINVAL;
    }
    if (hal_flash_check_addr(hf, address) ||
      hal_flash_check_addr(hf, address + num_bytes)) {
        return SYS_EINVAL;
    }

    if (protected_flash[id / 8] & (1 << (id & 7))) {
        return SYS_EACCES;
    }

    end = address + num_bytes;
    if (end <= address) {
        /*
         * Check for wrap-around.
         */
        return SYS_EINVAL;
    }

    if (hf->hf_itf->hff_erase) {
        hf->hf_itf->hff_erase(hf, address, num_bytes);
#if MYNEWT_VAL(HAL_FLASH_VERIFY_ERASES)
        assert(hal_flash_isempty_no_buf(id, address, num_bytes) == 1);
#endif
    } else {
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
                    return SYS_EIO;
                }

#if MYNEWT_VAL(HAL_FLASH_VERIFY_ERASES)
                assert(hal_flash_isempty_no_buf(id, start, size) == 1);
#endif
            }
        }
    }
    return 0;
}

int
hal_flash_is_erased(const struct hal_flash *hf, uint32_t address, void *dst,
        uint32_t num_bytes)
{
    uint32_t i;
    uint8_t *buf;
    int rc;

    buf = dst;

    rc = hf->hf_itf->hff_read(hf, address, dst, num_bytes);
    if (rc != 0) {
        return SYS_EIO;
    }

    for (i = 0; i < num_bytes; i++) {
        if (buf[i] != hf->hf_erased_val) {
            return 0;
        }
    }
    return 1;
}

int
hal_flash_isempty(uint8_t id, uint32_t address, void *dst, uint32_t num_bytes)
{
    const struct hal_flash *hf;
    int rc;

    hf = hal_bsp_flash_dev(id);
    if (!hf) {
        return SYS_EINVAL;
    }
    if (hal_flash_check_addr(hf, address) ||
      hal_flash_check_addr(hf, address + num_bytes)) {
        return SYS_EINVAL;
    }
    if (hf->hf_itf->hff_is_empty) {
        rc = hf->hf_itf->hff_is_empty(hf, address, dst, num_bytes);
        if (rc < 0) {
            return SYS_EIO;
        } else {
            return rc;
        }
    } else {
        return hal_flash_is_erased(hf, address, dst, num_bytes);
    }
}

int
hal_flash_isempty_no_buf(uint8_t id, uint32_t address, uint32_t num_bytes)
{
    uint8_t buf[MYNEWT_VAL(HAL_FLASH_VERIFY_BUF_SZ)];
    uint32_t blksz;
    uint32_t rem;
    uint32_t off;
    int empty;

    for (off = 0; off < num_bytes; off += sizeof buf) {
        rem = num_bytes - off;

        blksz = sizeof buf;
        if (blksz > rem) {
            blksz = rem;
        }

        empty = hal_flash_isempty(id, address + off, buf, blksz);
        if (empty != 1) {
            return empty;
        }
    }

    return 1;
}

int
hal_flash_ioctl(uint8_t id, uint32_t cmd, void *args)
{
    return 0;
}

int
hal_flash_write_protect(uint8_t id, uint8_t protect)
{
    if (NULL == hal_bsp_flash_dev(id)) {
        return SYS_EINVAL;
    }
    if (id / 8 >= sizeof(protected_flash) / sizeof(protected_flash[0])) {
        return SYS_EINVAL;
    }

    if (protect) {
        protected_flash[id / 8] |= (1 << (id & 7));
    } else {
        protected_flash[id / 8] &= ~(1 << (id & 7));
    }

    return SYS_EOK;
}
