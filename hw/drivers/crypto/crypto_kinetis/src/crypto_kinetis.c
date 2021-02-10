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

#include <stdint.h>
#include <string.h>
#include "mcu/cmsis_nvic.h"
#include <os/mynewt.h>

#include "crypto/crypto.h"
#include "crypto_kinetis/crypto_kinetis.h"

#if MYNEWT_VAL(KINETIS_CRYPTO_USE_CAU)
#define USE_CAU 1
#elif MYNEWT_VAL(KINETIS_CRYPTO_USE_LTC)
#define USE_LTC 1
#include "fsl_ltc.h"
#else
#error "Unsupported CRYPTO HW"
#endif

static struct os_mutex gmtx;

static inline uint8_t
ROUNDS_PER_KEYLEN(uint16_t keylen)
{
    switch (keylen) {
    case 128: return 10;
    case 192: return 12;
    case 256: return 14;
    default:
        assert(0);
        return 0;
    }
}

#if USE_CAU
/*
 * These routines are exported by NXP's provided CAU and mmCAU software
 * library.
 */
extern void mmcau_aes_set_key(const unsigned char *key,
        const int key_size,
        unsigned char *key_sch);
extern void mmcau_aes_encrypt(const unsigned char *in,
        const unsigned char *key_sch,
        const int nr,
        unsigned char *out);
extern void mmcau_aes_decrypt(const unsigned char *in,
        const unsigned char *key_sch,
        const int nr,
        unsigned char *out);

/*
 * Fallback CAU routines in C.
 */
extern void cau_aes_set_key(const unsigned char *key, const int key_size,
        unsigned char *key_sch);
extern void cau_aes_encrypt(const unsigned char *in,
        const unsigned char *key_sch, int nr, unsigned char *out);
extern void cau_aes_decrypt(const unsigned char *in,
        const unsigned char *key_sch, const int nr, unsigned char *out);

typedef void (* cau_aes_func_t)(const unsigned char *in,
        const unsigned char *key_sch,
        const int nr,
        unsigned char *out);

static uint32_t
kinetis_crypto_cau_aes_nr(cau_aes_func_t aes_func, const uint8_t *key,
        int keylen, const uint8_t *inbuf, uint8_t *outbuf, size_t len)
{
    uint32_t i;
    size_t remain;

    /* NOTE: allows for any key size (AES-128 upto AES-256) */
    unsigned char keysch[240];

    /* expect an AES block sized buffer */
    assert(len & ~0xf);

    os_mutex_pend(&gmtx, OS_TIMEOUT_NEVER);

    cau_aes_set_key((const unsigned char *)key, keylen, keysch);

    i = 0;
    remain = len;
    while (remain) {
        if (len > AES_BLOCK_LEN) {
            len = AES_BLOCK_LEN;
        }

        aes_func((const unsigned char *)&inbuf[i], keysch,
                ROUNDS_PER_KEYLEN(keylen), (unsigned char *)&outbuf[i]);

        i += len;
        remain -= len;
        len = remain;
    }

    os_mutex_release(&gmtx);

    return i;
}

#else /* USE_LTC */

static uint32_t
kinetis_crypto_ltc_aes_encrypt(uint16_t mode, const uint8_t *key,
        int keylen, uint8_t *iv, const uint8_t *inbuf, uint8_t *outbuf,
        size_t len)
{
    status_t ret = 0;
    uint32_t keysize = keylen / 8;

    switch (mode) {
    case CRYPTO_MODE_ECB:
        ret = LTC_AES_EncryptEcb(LTC0, inbuf, outbuf, len, key, keysize);
        if (ret == 0) {
            return len;
        }
        break;
    case CRYPTO_MODE_CTR:
        ret = LTC_AES_CryptCtr(LTC0, inbuf, outbuf, len, iv, key, keysize, NULL, NULL);
        if (ret == 0) {
            return len;
        }
        break;
    case CRYPTO_MODE_CBC:
        ret = LTC_AES_EncryptCbc(LTC0, inbuf, outbuf, len, iv, key, keysize);
        if (ret == 0) {
            memcpy(iv, &outbuf[len-AES_BLOCK_LEN], AES_BLOCK_LEN);
            return len;
        }
        break;
    }

    return 0;
}

static uint32_t
kinetis_crypto_ltc_aes_decrypt(uint16_t mode, const uint8_t *key,
        int keylen, uint8_t *iv, const uint8_t *inbuf, uint8_t *outbuf,
        size_t len)
{
    status_t ret = 0;
    uint16_t keysize = keylen / 8;
    uint8_t iv_save[AES_BLOCK_LEN];

    switch (mode) {
    case CRYPTO_MODE_ECB:
        ret = LTC_AES_DecryptEcb(LTC0, inbuf, outbuf, len, key, keysize, kLTC_EncryptKey);
        if (ret == 0) {
            return len;
        }
        break;
    case CRYPTO_MODE_CTR:
        ret = LTC_AES_CryptCtr(LTC0, inbuf, outbuf, len, iv, key, keysize, NULL, NULL);
        if (ret == 0) {
            return len;
        }
        break;
    case CRYPTO_MODE_CBC:
        memcpy(iv_save, &inbuf[len-AES_BLOCK_LEN], AES_BLOCK_LEN);
        ret = LTC_AES_DecryptCbc(LTC0, inbuf, outbuf, len, iv, key, keysize, kLTC_EncryptKey);
        if (ret == 0) {
            memcpy(iv, iv_save, AES_BLOCK_LEN);
            return len;
        }
        break;
    }

    return 0;
}

#endif

static bool
kinetis_crypto_has_support(struct crypto_dev *crypto, uint8_t op,
        uint16_t algo, uint16_t mode, uint16_t keylen)
{
    (void)op;

    if (algo != CRYPTO_ALGO_AES) {
        return false;
    }

#if USE_CAU
    if ((mode & CRYPTO_MODE_ECB) == 0) {
#else
    if ((mode & (CRYPTO_MODE_ECB | CRYPTO_MODE_CBC | CRYPTO_MODE_CTR)) == 0) {
#endif
        return false;
    }

#if USE_CAU
    if (!CRYPTO_VALID_AES_KEYLEN(keylen)) {
#else
    if (!ltc_check_key_size(keylen / 8)) {
#endif
        return false;
    }

    return true;
}

static uint32_t
kinetis_crypto_encrypt(struct crypto_dev *crypto, uint16_t algo, uint16_t mode,
        const uint8_t *key, uint16_t keylen, uint8_t *iv, const uint8_t *inbuf,
        uint8_t *outbuf, uint32_t len)
{
    (void)iv;

    if (!kinetis_crypto_has_support(crypto, CRYPTO_OP_ENCRYPT, algo, mode, keylen)) {
        return 0;
    }

#if USE_CAU
    return kinetis_crypto_cau_aes_nr(cau_aes_encrypt, key, keylen, inbuf,
            outbuf, len);
#else
    return kinetis_crypto_ltc_aes_encrypt(mode, key, keylen, iv, inbuf,
            outbuf, len);
#endif
}

static uint32_t
kinetis_crypto_decrypt(struct crypto_dev *crypto, uint16_t algo, uint16_t mode,
        const uint8_t *key, uint16_t keylen, uint8_t *iv, const uint8_t *inbuf,
        uint8_t *outbuf, uint32_t len)
{
    (void)iv;

    if (!kinetis_crypto_has_support(crypto, CRYPTO_OP_ENCRYPT, algo, mode, keylen)) {
        return 0;
    }

#if USE_CAU
    return kinetis_crypto_cau_aes_nr(cau_aes_decrypt, key, keylen, inbuf,
            outbuf, len);
#else
    return kinetis_crypto_ltc_aes_decrypt(mode, key, keylen, iv, inbuf,
            outbuf, len);
#endif
}

static int
kinetis_crypto_dev_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    struct crypto_dev *crypto;

    crypto = (struct crypto_dev *)dev;
    assert(crypto);

    if (!(dev->od_flags & OS_DEV_F_STATUS_OPEN)) {
#if USE_LTC
        LTC_Init(LTC0);
        /* Enable differential power analysis resistance */
        LTC_SetDpaMaskSeed(LTC0, SIM->UIDL);
#endif
    }

    return OS_OK;
}

int
kinetis_crypto_dev_init(struct os_dev *dev, void *arg)
{
    struct crypto_dev *crypto;

    crypto = (struct crypto_dev *)dev;
    assert(crypto);

    OS_DEV_SETHANDLERS(dev, kinetis_crypto_dev_open, NULL);

    assert(os_mutex_init(&gmtx) == 0);

    crypto->interface.encrypt = kinetis_crypto_encrypt;
    crypto->interface.decrypt = kinetis_crypto_decrypt;
    crypto->interface.has_support = kinetis_crypto_has_support;

    return 0;
}
