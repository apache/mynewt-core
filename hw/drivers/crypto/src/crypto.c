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

#include "crypto/crypto.h"

/*
 * Implement modes using ECB for non-available HW support
 */

#if MYNEWT_VAL(CRYPTO_NEED_CTR) && !MYNEWT_VAL(CRYPTO_HW_AES_CTR)
static int
crypto_do_ctr(struct crypto_dev *crypto, const void *key, uint16_t keylen,
        void *nonce, const void *inbuf, void *outbuf, uint32_t len)
{
    size_t remain;
    uint32_t sz;
    uint32_t i;
    uint8_t *outbuf8 = (uint8_t *)outbuf;
    uint8_t *inbuf8 = (uint8_t *)inbuf;
    uint8_t _nonce[AES_BLOCK_LEN];
    uint8_t _out[AES_BLOCK_LEN];
    int rc;

    if (crypto->interface.encrypt == NULL) {
        return 0;
    }

    sz = 0;
    remain = len;
    memcpy(_nonce, nonce, AES_BLOCK_LEN);
    while (len) {
        if (len > AES_BLOCK_LEN) {
            len = AES_BLOCK_LEN;
        }

        rc = crypto->interface.encrypt(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_ECB,
                (const uint8_t *)key, keylen, NULL, (const uint8_t *)_nonce,
                _out, AES_BLOCK_LEN);
        if (rc != AES_BLOCK_LEN) {
            return sz + rc;
        }

        for (i = 0; i < len; i++) {
            outbuf8[i] = inbuf8[i] ^ _out[i];
        }

        for (i = AES_BLOCK_LEN; i > 0; --i) {
            if (++_nonce[i - 1] != 0) {
                break;
            }
        }

        inbuf8 += len;
        outbuf8 += len;
        sz += len;
        remain -= len;
        len = remain;
    }

    memcpy(nonce, _nonce, AES_BLOCK_LEN);
    return sz;
}
#endif /* MYNEWT_VAL(CRYPTO_NEED_CTR) && !MYNEWT_VAL(CRYPTO_HW_AES_CTR) */

#if MYNEWT_VAL(CRYPTO_NEED_CBC) && !MYNEWT_VAL(CRYPTO_HW_AES_CBC)
static int
crypto_do_cbc(struct crypto_dev *crypto, uint8_t op, const void *key,
        uint16_t keylen, void *iv, const void *inbuf, void *outbuf, uint32_t len)
{
    size_t remain;
    uint32_t i;
    uint32_t j;
    uint8_t tmp[AES_BLOCK_LEN];
    const uint8_t *ivp;
    uint8_t iv_save[AES_BLOCK_LEN * 2];
    uint8_t ivpos;
    uint8_t *outbuf8 = (uint8_t *)outbuf;
    const uint8_t *inbuf8 = (const uint8_t *)inbuf;
    bool inplace;
    int rc;

    if (!CRYPTO_VALID_OP(op) || (len & (AES_BLOCK_LEN - 1))) {
        return 0;
    }

    if (op == CRYPTO_OP_ENCRYPT && crypto->interface.encrypt == NULL) {
        return 0;
    }

    if (op == CRYPTO_OP_DECRYPT && crypto->interface.decrypt == NULL) {
        return 0;
    }

    i = 0;
    remain = len;
    ivp = iv;
    inplace = (inbuf == outbuf);
    ivpos = 0;
    while (len) {
        if (len > AES_BLOCK_LEN) {
            len = AES_BLOCK_LEN;
        }

        if (op == CRYPTO_OP_ENCRYPT) {
            for (j = 0; j < AES_BLOCK_LEN; j++) {
                tmp[j] = ivp[j] ^ inbuf8[j+i];
            }

            rc = crypto->interface.encrypt(crypto, CRYPTO_ALGO_AES,
                    CRYPTO_MODE_ECB, (const uint8_t *)key, keylen, NULL, tmp,
                    &outbuf8[i], AES_BLOCK_LEN);
            if (rc != AES_BLOCK_LEN) {
                return rc;
            }

            ivp = &outbuf8[i];
        } else {
            rc = crypto->interface.decrypt(crypto, CRYPTO_ALGO_AES,
                    CRYPTO_MODE_ECB, (const uint8_t *)key, keylen, NULL,
                    &inbuf8[i], tmp, AES_BLOCK_LEN);
            if (rc != AES_BLOCK_LEN) {
                return rc;
            }

            if (inplace) {
                memcpy(&iv_save[ivpos], &inbuf8[i], AES_BLOCK_LEN);
            }

            for (j = 0; j < AES_BLOCK_LEN; j++) {
                outbuf8[i+j] = ivp[j] ^ tmp[j];
            }

            if (inplace) {
                ivp = &iv_save[ivpos];
                ivpos = (ivpos + AES_BLOCK_LEN) % (AES_BLOCK_LEN * 2);
            } else {
                ivp = &inbuf8[i];
            }
        }

        remain -= len;
        i += len;
        len = remain;
    }

    memcpy(iv, ivp, AES_BLOCK_LEN);
    return i;
}
#endif /* MYNEWT_VAL(CRYPTO_NEED_CBC) && !MYNEWT_VAL(CRYPTO_HW_AES_CBC) */

/*
 * Custom (low-level) functions
 */

uint32_t
crypto_encrypt_custom(struct crypto_dev *crypto, uint16_t algo, uint16_t mode,
        const void *key, uint16_t keylen, void *iv, const void *inbuf,
        void *outbuf, uint32_t len)
{
    if (!crypto_has_support(crypto, CRYPTO_OP_ENCRYPT, algo, mode, keylen)) {
        switch (mode) {
        case CRYPTO_MODE_CTR:
#if MYNEWT_VAL(CRYPTO_NEED_CTR) && !MYNEWT_VAL(CRYPTO_HW_AES_CTR)
            if (crypto_has_support(crypto, CRYPTO_OP_ENCRYPT, algo, CRYPTO_MODE_ECB, keylen)) {
                return crypto_do_ctr(crypto, key, keylen, iv, inbuf, outbuf, len);
            }
#endif
            break;
        case CRYPTO_MODE_CBC:
#if MYNEWT_VAL(CRYPTO_NEED_CBC) && !MYNEWT_VAL(CRYPTO_HW_AES_CBC)
            if (crypto_has_support(crypto, CRYPTO_OP_ENCRYPT, algo, CRYPTO_MODE_ECB, keylen)) {
                return crypto_do_cbc(crypto, CRYPTO_OP_ENCRYPT, key, keylen,
                        iv, inbuf, outbuf, len);
            }
#endif
            break;
        default:
            return 0;
        }
    }

    if (crypto->interface.encrypt == NULL) {
        return 0;
    }

    return crypto->interface.encrypt(crypto, algo, mode, (const uint8_t *)key,
            keylen, (uint8_t *)iv, (const uint8_t *)inbuf, (uint8_t *)outbuf, len);
}

uint32_t
crypto_encryptv_custom(struct crypto_dev *crypto, uint16_t algo, uint16_t mode,
        const void *key, uint16_t keylen, void *iv, struct crypto_iovec *iov,
        uint32_t iovlen)
{
    uint32_t len;
    uint32_t total;
    int i;

    if (crypto->interface.encrypt == NULL) {
        return 0;
    }

    total = 0;
    for (i = 0; i < iovlen; i++) {
        len = crypto_encrypt_custom(crypto, algo, mode, key, keylen, iv,
                iov[i].iov_base, iov[i].iov_base, iov[i].iov_len);
        total += len;
        if (len != iov[i].iov_len) {
            break;
        }
    }

    return total;
}

uint32_t
crypto_decrypt_custom(struct crypto_dev *crypto, uint16_t algo, uint16_t mode,
        const void *key, uint16_t keylen, void *iv, const void *inbuf,
        void *outbuf, uint32_t len)
{
    if (!crypto_has_support(crypto, CRYPTO_OP_DECRYPT, algo, mode, keylen)) {
        switch (mode) {
        case CRYPTO_MODE_CTR:
#if MYNEWT_VAL(CRYPTO_NEED_CTR) && !MYNEWT_VAL(CRYPTO_HW_AES_CTR)
            /* NOTE: not a typo, CTR always encrypts */
            if (crypto_has_support(crypto, CRYPTO_OP_ENCRYPT, algo, CRYPTO_MODE_ECB, keylen)) {
                return crypto_do_ctr(crypto, key, keylen, iv, inbuf, outbuf, len);
            }
#endif
            break;
        case CRYPTO_MODE_CBC:
#if MYNEWT_VAL(CRYPTO_NEED_CBC) && !MYNEWT_VAL(CRYPTO_HW_AES_CBC)
            if (crypto_has_support(crypto, CRYPTO_OP_DECRYPT, algo, CRYPTO_MODE_ECB, keylen)) {
                return crypto_do_cbc(crypto, CRYPTO_OP_DECRYPT, key, keylen,
                        iv, inbuf, outbuf, len);
            }
#endif
            break;
        default:
            return 0;
        }
    }

    if (crypto->interface.decrypt == NULL) {
        return 0;
    }

    return crypto->interface.decrypt(crypto, algo, mode, (const uint8_t *)key,
            keylen, (uint8_t *)iv, (const uint8_t *)inbuf, (uint8_t *)outbuf, len);
}

uint32_t
crypto_decryptv_custom(struct crypto_dev *crypto, uint16_t algo, uint16_t mode,
        const void *key, uint16_t keylen, void *iv, struct crypto_iovec *iov,
        uint32_t iovlen)
{
    uint32_t len;
    uint32_t total;
    int i;

    if (crypto->interface.decrypt == NULL) {
        return 0;
    }

    total = 0;
    for (i = 0; i < iovlen; i++) {
        len = crypto_decrypt_custom(crypto, algo, mode, key, keylen, iv,
                iov[i].iov_base, iov[i].iov_base, iov[i].iov_len);
        total += len;
        if (len != iov[i].iov_len) {
            break;
        }
    }

    return total;
}

/*
 * AES-ECB helpers
 */

uint32_t
crypto_encrypt_aes_ecb(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, const void *inbuf, void *outbuf, uint32_t len)
{
    return crypto_encrypt_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_ECB,
            key, keylen, NULL, inbuf, outbuf, len);
}

uint32_t
crypto_encryptv_aes_ecb(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, struct crypto_iovec *iov, uint32_t iovlen)
{
    return crypto_encryptv_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_ECB,
            key, keylen, NULL, iov, iovlen);
}

uint32_t
crypto_decrypt_aes_ecb(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, const void *inbuf, void *outbuf, uint32_t len)
{
    return crypto_decrypt_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_ECB,
            key, keylen, NULL, inbuf, outbuf, len);
}

uint32_t
crypto_decryptv_aes_ecb(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, struct crypto_iovec *iov, uint32_t iovlen)
{
    return crypto_decryptv_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_ECB,
            key, keylen, NULL, iov, iovlen);
}

/*
 * AES-CBC helpers
 */

uint32_t
crypto_encrypt_aes_cbc(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *iv, const void *inbuf, void *outbuf,
        uint32_t len)
{
    return crypto_encrypt_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_CBC,
        key, keylen, iv, inbuf, outbuf, len);
}

uint32_t
crypto_encryptv_aes_cbc(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *iv, struct crypto_iovec *iov, uint32_t iovlen)
{
    return crypto_encryptv_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_CBC,
            key, keylen, iv, iov, iovlen);
}

uint32_t
crypto_decrypt_aes_cbc(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *iv, const void *inbuf, void *outbuf,
        uint32_t len)
{
    return crypto_decrypt_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_CBC,
        key, keylen, iv, inbuf, outbuf, len);
}

uint32_t
crypto_decryptv_aes_cbc(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *iv, struct crypto_iovec *iov, uint32_t iovlen)
{
    return crypto_decryptv_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_CBC,
            key, keylen, iv, iov, iovlen);
}

/*
 * AES-CTR helpers
 */

uint32_t
crypto_encrypt_aes_ctr(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *nonce, const void *inbuf, void *outbuf,
        uint32_t len)
{
    return crypto_encrypt_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_CTR,
        key, keylen, nonce, inbuf, outbuf, len);
}

uint32_t
crypto_encryptv_aes_ctr(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *nonce, struct crypto_iovec *iov, uint32_t iovlen)
{
    return crypto_encryptv_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_CTR,
            key, keylen, nonce, iov, iovlen);
}

uint32_t
crypto_decrypt_aes_ctr(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *nonce, const void *inbuf, void *outbuf,
        uint32_t len)
{
    return crypto_decrypt_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_CTR,
        key, keylen, nonce, inbuf, outbuf, len);
}

uint32_t
crypto_decryptv_aes_ctr(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *nonce, struct crypto_iovec *iov, uint32_t iovlen)
{
    return crypto_decryptv_custom(crypto, CRYPTO_ALGO_AES, CRYPTO_MODE_CTR,
            key, keylen, nonce, iov, iovlen);
}

/*
 * More driver interface
 */

bool
crypto_has_support(struct crypto_dev *crypto, uint8_t op, uint16_t algo,
        uint16_t mode, uint16_t keylen)
{
    assert(crypto->interface.has_support);
    return crypto->interface.has_support(crypto, op, algo, mode, keylen);
}
