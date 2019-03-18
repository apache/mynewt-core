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

#include <enc_flash/enc_flash.h>
#include <crypto/crypto.h>
#include <ef_crypto/ef_crypto.h>

#define EDEV_TO_CRYPTO(dev) (struct eflash_crypto_dev *)(dev)
#define ENC_FLASH_NONCE "mynewtencfla"

static void
ef_crypto_get_block(struct eflash_crypto_dev *dev, uint32_t addr, uint8_t *blk)
{
    memcpy(blk, ENC_FLASH_NONCE, 12);
    memcpy(blk + 12, &addr, sizeof(addr));

    crypto_encrypt_aes_ecb(dev->ecd_crypto, dev->ecd_key, 128, blk, blk, 16);
}


void
enc_flash_crypt_arch(struct enc_flash_dev *edev, uint32_t blk_addr,
                     const uint8_t *src, uint8_t *tgt, int off, int cnt)
{
    struct eflash_crypto_dev *dev = EDEV_TO_CRYPTO(edev);
    uint8_t blk[ENC_FLASH_BLK];
    uint8_t *b;
    int i;

    assert(dev->ecd_crypto);

    ef_crypto_get_block(dev, blk_addr, blk);
    b = &blk[off];
    for (i = 0 ; i < cnt; i++) {
        *tgt++ = *b++ ^ *src++;
    }
}

void
enc_flash_setkey_arch(struct enc_flash_dev *edev, uint8_t *key)
{
    struct eflash_crypto_dev *dev = EDEV_TO_CRYPTO(edev);

    memcpy(dev->ecd_key, key, ENC_FLASH_BLK);
}

int
enc_flash_init_arch(const struct enc_flash_dev *edev)
{
    struct eflash_crypto_dev *dev = EDEV_TO_CRYPTO(edev);

    dev->ecd_crypto = (struct crypto_dev *) os_dev_open("crypto",
            OS_TIMEOUT_NEVER, NULL);
    assert(dev->ecd_crypto);

    return 0;
}
