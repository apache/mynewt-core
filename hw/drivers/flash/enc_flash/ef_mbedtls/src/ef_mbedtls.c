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

#include <mbedtls/aes.h>

#include <enc_flash/enc_flash.h>
#include "ef_mbedtls/ef_mbedtls.h"

#define EDEV_TO_MBED(dev) (struct eflash_mbedtls_dev *)(edev)
#define ENC_FLASH_NONCE "mynewtencfla"

static void
ef_mbedtls_get_block(struct eflash_mbedtls_dev *dev, uint32_t addr, uint8_t *blk)
{
    mbedtls_aes_context ctx;

    mbedtls_aes_init(&ctx);

    memcpy(blk, ENC_FLASH_NONCE, 12);
    memcpy(blk + 12, &addr, sizeof(addr));

    mbedtls_aes_setkey_enc(&ctx, dev->key, 128);
    mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, blk, blk);
    mbedtls_aes_free(&ctx);
}

void
enc_flash_crypt_arch(struct enc_flash_dev *edev, uint32_t blk_addr,
                     const uint8_t *src, uint8_t *tgt, int off, int cnt)
{
    struct eflash_mbedtls_dev *dev = EDEV_TO_MBED(edev);
    uint8_t blk[ENC_FLASH_BLK];
    uint8_t *b;
    int i;

    ef_mbedtls_get_block(dev, blk_addr, blk);
    b = &blk[off];
    for (i = 0 ; i < cnt; i++) {
        *tgt++ = *b++ ^ *src++;
    }
}

void
enc_flash_setkey_arch(struct enc_flash_dev *edev, uint8_t *key)
{
    struct eflash_mbedtls_dev *dev = EDEV_TO_MBED(edev);

    memcpy(dev->key, key, ENC_FLASH_BLK);
}

int
enc_flash_init_arch(const struct eflash_mbedtls_dev *edev)
{
    return 0;
}
