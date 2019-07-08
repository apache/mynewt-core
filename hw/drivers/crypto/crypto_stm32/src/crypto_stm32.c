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
#include "mcu/cmsis_nvic.h"
#include <os/mynewt.h>
#include "mcu/stm32_hal.h"
#include "crypto/crypto.h"
#include "crypto_stm32/crypto_stm32.h"

static struct os_mutex gmtx;
static CRYP_HandleTypeDef g_hcryp;

static inline uint32_t
CRYP_KEYSIZE_FROM_KEYLEN(uint16_t keylen)
{
    switch (keylen) {
    case 128:
        return CRYP_KEYSIZE_128B;
    case 192:
        return CRYP_KEYSIZE_192B;
    case 256:
        return CRYP_KEYSIZE_256B;
    default:
        assert(0);
        return 0;
    }
}

static bool
stm32_has_support(struct crypto_dev *crypto, uint8_t op, uint16_t algo,
        uint16_t mode, uint16_t keylen)
{
    (void)op;

    if (algo != CRYPTO_ALGO_AES || !CRYPTO_VALID_AES_KEYLEN(keylen)) {
        return false;
    }

    switch (mode) {
    case CRYPTO_MODE_ECB: /* fallthrough */
    case CRYPTO_MODE_CBC: /* fallthrough */
    case CRYPTO_MODE_CTR:
        return true;
    }

    return false;
}

static uint32_t
stm32_crypto_op(struct crypto_dev *crypto, uint8_t op, uint16_t algo,
        uint16_t mode, const uint8_t *key, uint16_t keylen, uint8_t *iv,
        const uint8_t *inbuf, uint8_t *outbuf, uint32_t len)
{
    HAL_StatusTypeDef status;
    uint32_t sz = 0;
    uint8_t i;
    uint8_t iv_save[AES_BLOCK_LEN];
    unsigned int inc;
    unsigned int carry;
    unsigned int v;

    if (!stm32_has_support(crypto, op, algo, mode, keylen)) {
        return 0;
    }

    g_hcryp.Init.KeySize = CRYP_KEYSIZE_FROM_KEYLEN(keylen);
    g_hcryp.Init.pKey = (uint8_t *)key;
    switch (mode) {
    case CRYPTO_MODE_CBC: /* fallthrough */
    case CRYPTO_MODE_CTR:
        g_hcryp.Init.pInitVect = (uint8_t *)iv;
        break;
    }

    os_mutex_pend(&gmtx, OS_TIMEOUT_NEVER);

    status = HAL_CRYP_Init(&g_hcryp);
    if (status != HAL_OK) {
        sz = 0;
        goto out;
    }

    switch (mode) {
    case CRYPTO_MODE_ECB:
        if (op == CRYPTO_OP_ENCRYPT) {
            status = HAL_CRYP_AESECB_Encrypt(&g_hcryp, (uint8_t *)inbuf, len,
                    outbuf, HAL_MAX_DELAY);
        } else {
            status = HAL_CRYP_AESECB_Decrypt(&g_hcryp, (uint8_t *)inbuf, len,
                    outbuf, HAL_MAX_DELAY);
        }
        break;
    case CRYPTO_MODE_CBC:
        if (op == CRYPTO_OP_ENCRYPT) {
            status = HAL_CRYP_AESCBC_Encrypt(&g_hcryp, (uint8_t *)inbuf, len,
                    outbuf, HAL_MAX_DELAY);
            if (status == HAL_OK) {
                memcpy(iv, &outbuf[len-AES_BLOCK_LEN], AES_BLOCK_LEN);
            }
        } else {
            memcpy(iv_save, &inbuf[len-AES_BLOCK_LEN], AES_BLOCK_LEN);
            status = HAL_CRYP_AESCBC_Decrypt(&g_hcryp, (uint8_t *)inbuf, len,
                    outbuf, HAL_MAX_DELAY);
            if (status == HAL_OK) {
                memcpy(iv, iv_save, AES_BLOCK_LEN);
            }
        }
        break;
    case CRYPTO_MODE_CTR:
        if (op == CRYPTO_OP_ENCRYPT) {
            status = HAL_CRYP_AESCTR_Encrypt(&g_hcryp, (uint8_t *)inbuf, len,
                    outbuf, HAL_MAX_DELAY);
        } else {
            status = HAL_CRYP_AESCTR_Decrypt(&g_hcryp, (uint8_t *)inbuf, len,
                    outbuf, HAL_MAX_DELAY);
        }
        if (status == HAL_OK) {
            inc = (len + AES_BLOCK_LEN - 1) / AES_BLOCK_LEN;
            carry = 0;
            for (i = AES_BLOCK_LEN; (inc != 0 || carry != 0) && i > 0; --i) {
                v = carry + (uint8_t)inc + iv[i - 1];
                iv[i - 1] = (uint8_t)v;
                inc >>= 8;
                carry = v >> 8;
            }
        }
        break;
    default:
        sz = 0;
        goto out;
    }

    if (status == HAL_OK) {
        sz = len;
    }

out:
    os_mutex_release(&gmtx);
    return sz;
}

static uint32_t
stm32_crypto_encrypt(struct crypto_dev *crypto, uint16_t algo, uint16_t mode,
        const uint8_t *key, uint16_t keylen, uint8_t *iv, const uint8_t *inbuf,
        uint8_t *outbuf, uint32_t len)
{
    return stm32_crypto_op(crypto, CRYPTO_OP_ENCRYPT, algo, mode, key, keylen,
            iv, inbuf, outbuf, len);
}

static uint32_t
stm32_crypto_decrypt(struct crypto_dev *crypto, uint16_t algo, uint16_t mode,
        const uint8_t *key, uint16_t keylen, uint8_t *iv, const uint8_t *inbuf,
        uint8_t *outbuf, uint32_t len)
{
    return stm32_crypto_op(crypto, CRYPTO_OP_DECRYPT, algo, mode, key, keylen,
            iv, inbuf, outbuf, len);
}

static int
stm32_crypto_dev_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    struct crypto_dev *crypto;

    crypto = (struct crypto_dev *)dev;
    assert(crypto);

    if (!(dev->od_flags & OS_DEV_F_STATUS_OPEN)) {
        __HAL_RCC_CRYP_CLK_ENABLE();
        g_hcryp.Instance = CRYP;
        g_hcryp.Init.DataType = CRYP_DATATYPE_8B;
    }

    return 0;
}

int
stm32_crypto_dev_init(struct os_dev *dev, void *arg)
{
    struct crypto_dev *crypto;

    crypto = (struct crypto_dev *)dev;
    assert(crypto);

    OS_DEV_SETHANDLERS(dev, stm32_crypto_dev_open, NULL);

    assert(os_mutex_init(&gmtx) == 0);

    crypto->interface.encrypt = stm32_crypto_encrypt;
    crypto->interface.decrypt = stm32_crypto_decrypt;
    crypto->interface.has_support = stm32_has_support;

    return 0;
}
