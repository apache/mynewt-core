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

#include "crypto/crypto.h"
#include "crypto_k64f/crypto_k64f.h"

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
k64f_crypto_cau_aes_nr(cau_aes_func_t aes_func, const uint8_t *key,
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

static bool
k64f_crypto_has_support(struct crypto_dev *crypto, uint8_t op, uint16_t algo,
        uint16_t mode, uint16_t keylen)
{
    (void)op;

    if (algo != CRYPTO_ALGO_AES) {
        return false;
    }

    if (mode != CRYPTO_MODE_ECB) {
        return false;
    }

    if (!CRYPTO_VALID_AES_KEYLEN(keylen)) {
        return false;
    }

    return true;
}

static uint32_t
k64f_crypto_encrypt(struct crypto_dev *crypto, uint16_t algo, uint16_t mode,
        const uint8_t *key, uint16_t keylen, uint8_t *iv, const uint8_t *inbuf,
        uint8_t *outbuf, uint32_t len)
{
    (void)iv;

    if (!k64f_crypto_has_support(crypto, CRYPTO_OP_ENCRYPT, algo, mode, keylen)) {
        return 0;
    }

    return k64f_crypto_cau_aes_nr(cau_aes_encrypt, key, keylen, inbuf,
            outbuf, len);
}

static uint32_t
k64f_crypto_decrypt(struct crypto_dev *crypto, uint16_t algo, uint16_t mode,
        const uint8_t *key, uint16_t keylen, uint8_t *iv, const uint8_t *inbuf,
        uint8_t *outbuf, uint32_t len)
{
    (void)iv;

    if (!k64f_crypto_has_support(crypto, CRYPTO_OP_ENCRYPT, algo, mode, keylen)) {
        return 0;
    }

    return k64f_crypto_cau_aes_nr(cau_aes_decrypt, key, keylen, inbuf,
            outbuf, len);
}

static int
k64f_crypto_dev_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    struct crypto_dev *crypto;

    crypto = (struct crypto_dev *)dev;
    assert(crypto);

    if (!(dev->od_flags & OS_DEV_F_STATUS_OPEN)) {
        /* XXX need to do something here? */
    }

    return OS_OK;
}

int
k64f_crypto_dev_init(struct os_dev *dev, void *arg)
{
    struct crypto_dev *crypto;

    crypto = (struct crypto_dev *)dev;
    assert(crypto);

    OS_DEV_SETHANDLERS(dev, k64f_crypto_dev_open, NULL);

    assert(os_mutex_init(&gmtx) == 0);

    crypto->interface.encrypt = k64f_crypto_encrypt;
    crypto->interface.decrypt = k64f_crypto_decrypt;
    crypto->interface.has_support = k64f_crypto_has_support;

    return 0;
}
