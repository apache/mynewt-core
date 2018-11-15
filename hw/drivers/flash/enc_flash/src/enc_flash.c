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

#include <os/mynewt.h>

#include "enc_flash/enc_flash.h"
#include "enc_flash/enc_flash_int.h"

#define HAL_TO_ENC(dev) (struct enc_flash_dev *)(dev)

static int enc_flash_read(const struct hal_flash *h_dev, uint32_t addr,
                          void *buf, uint32_t len);
static int enc_flash_write(const struct hal_flash *h_dev, uint32_t addr,
                           const void *buf, uint32_t len);
static int enc_flash_erase_sector(const struct hal_flash *h_dev,
                                  uint32_t addr);
static int enc_flash_sector_info(const struct hal_flash *h_dev, int idx,
                                 uint32_t *addr, uint32_t *sz);
static int enc_flash_is_empty(const struct hal_flash *h_dev, uint32_t addr,
                              void *buf, uint32_t len);
static int enc_flash_init(const struct hal_flash *h_dev);

const struct hal_flash_funcs enc_flash_funcs = {
    .hff_read         = enc_flash_read,
    .hff_write        = enc_flash_write,
    .hff_erase_sector = enc_flash_erase_sector,
    .hff_sector_info  = enc_flash_sector_info,
    .hff_is_empty     = enc_flash_is_empty,
    .hff_init         = enc_flash_init,
};

/*
 * Read first all the data in to provided memory area, then apply the
 * cipher -> text conversion.
 */
static int
enc_flash_read(const struct hal_flash *h_dev, uint32_t addr, void *buf,
               uint32_t len)
{
    struct enc_flash_dev *dev = HAL_TO_ENC(h_dev);
    uint8_t *bufb = buf;
    int i;
    int blk_end;
    int rc = 0;

    h_dev = dev->efd_hwdev;

    rc = h_dev->hf_itf->hff_read(h_dev, addr, buf, len);
    if (rc) {
        return rc;
    }
    i = addr & (ENC_FLASH_BLK - 1);
    addr &= ~(ENC_FLASH_BLK - 1);
    blk_end = ENC_FLASH_BLK;
    if (blk_end > i + len) {
        blk_end = i + len;
    }
    while (1) {
        enc_flash_crypt_arch(dev, addr, bufb, bufb, i, blk_end - i);
        len -= (blk_end - i);
        bufb += (blk_end - i);
        if (len == 0) {
            break;
        }
        i = 0;
        addr += ENC_FLASH_BLK;
        blk_end = ENC_FLASH_BLK;
        if (blk_end > len) {
            blk_end = len;
        }
    }
    return rc;
}

static int
enc_flash_write(const struct hal_flash *h_dev, uint32_t addr,
                const void *buf, uint32_t len)
{
    struct enc_flash_dev *dev = HAL_TO_ENC(h_dev);
    const uint8_t *bufb = buf;
    int i;
    int blksz;
    int rc = 0;
    uint8_t ctext[ENC_FLASH_BLK];

    h_dev = dev->efd_hwdev;

    i = addr & (ENC_FLASH_BLK - 1);
    blksz = ENC_FLASH_BLK - i;
    if (blksz > len) {
        blksz = len;
    }
    while (1) {
        enc_flash_crypt_arch(dev, addr & ~(ENC_FLASH_BLK - 1), bufb, ctext,
                             i, blksz);
        len -= blksz;
        bufb += blksz;
        rc = h_dev->hf_itf->hff_write(h_dev, addr, ctext, blksz);
        if (rc) {
            return rc;
        }
        if (len == 0) {
            break;
        }
        i = 0;
        addr += blksz;
        blksz = ENC_FLASH_BLK;
        if (blksz > len) {
            blksz = len;
        }
    }
    return rc;
}

static int
enc_flash_erase_sector(const struct hal_flash *h_dev, uint32_t addr)
{
    struct enc_flash_dev *dev = HAL_TO_ENC(h_dev);

    h_dev = dev->efd_hwdev;

    return h_dev->hf_itf->hff_erase_sector(h_dev, addr);
}

static int
enc_flash_sector_info(const struct hal_flash *h_dev, int idx,
                      uint32_t *addr, uint32_t *sz)
{
    struct enc_flash_dev *dev = HAL_TO_ENC(h_dev);

    h_dev = dev->efd_hwdev;

    return h_dev->hf_itf->hff_sector_info(h_dev, idx, addr, sz);
}

static int
enc_flash_is_empty(const struct hal_flash *h_dev, uint32_t addr, void *buf,
                   uint32_t len)
{
    struct enc_flash_dev *dev = HAL_TO_ENC(h_dev);
    const struct hal_flash *hwdev;
    int rc;

    hwdev = dev->efd_hwdev;

    if (hwdev->hf_itf->hff_is_empty) {
        return hwdev->hf_itf->hff_is_empty(hwdev, addr, buf, len);
    } else {
        rc = hal_flash_is_erased(hwdev, addr, buf, len);

        /*
         * If error or low-level flash is erased, avoid reading it.
         */
        if (rc < 0 || rc == 1) {
            return rc;
        }

        /*
         * Also read the underlying data.
         */
        return enc_flash_read(h_dev, addr, buf, len);
    }
}

void
enc_flash_setkey(struct hal_flash *h_dev, uint8_t *key)
{
    enc_flash_setkey_arch(HAL_TO_ENC(h_dev), key);
}

static int
enc_flash_init(const struct hal_flash *h_dev)
{
    struct enc_flash_dev *dev = HAL_TO_ENC(h_dev);

    h_dev = dev->efd_hwdev;

    dev->efd_hal.hf_base_addr = h_dev->hf_base_addr;
    dev->efd_hal.hf_size =  h_dev->hf_size;
    dev->efd_hal.hf_sector_cnt = h_dev->hf_sector_cnt;
    dev->efd_hal.hf_align = h_dev->hf_align;
    dev->efd_hal.hf_erased_val = h_dev->hf_erased_val;

    enc_flash_init_arch(dev);

    return 0;
}
