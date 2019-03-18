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

#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include <inttypes.h>
#include <stddef.h>
#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AES definitions
 */
#define AES_BLOCK_LEN     16

#define AES_128_KEY_LEN   16
#define AES_192_KEY_LEN   24
#define AES_256_KEY_LEN   32
#define AES_MAX_KEY_LEN   AES_256_KEY_LEN

#define CRYPTO_VALID_AES_KEYLEN(x) \
    (((x) == 128) || ((x) == 192) || ((x) == 256))

/*
 * Driver capabilities
 */
#define CRYPTO_OP_ENCRYPT              0x01
#define CRYPTO_OP_DECRYPT              0x02
#define CRYPTO_VALID_OP(x) \
    (((x) == CRYPTO_OP_ENCRYPT) || ((x) == CRYPTO_OP_DECRYPT))

#define CRYPTO_ALGO_AES                0x0001
#define CRYPTO_ALGO_DES                0x0002
#define CRYPTO_ALGO_3DES               0x0004
#define CRYPTO_ALGO_RSA                0x0010

#define CRYPTO_MODE_ECB                0x0001
#define CRYPTO_MODE_CBC                0x0002
#define CRYPTO_MODE_CTR                0x0004
#define CRYPTO_MODE_CCM                0x0008
#define CRYPTO_MODE_GCM                0x0010

struct crypto_dev;

typedef uint32_t (* crypto_op_func_t)(struct crypto_dev *crypto, uint16_t algo,
        uint16_t mode, const uint8_t *key, uint16_t keylen, uint8_t *iv,
        const uint8_t *inbuf, uint8_t *outbuf, uint32_t len);
typedef bool (* crypto_support_func_t)(struct crypto_dev *crypto, uint8_t op,
        uint16_t algo, uint16_t mode, uint16_t keylen);

/**
 * @struct crypto_interface
 * @brief Provides the interface into a HW crypto driver
 *
 * @var crypto_interface::encrypt
 * encrypt is a crypto_op_func_t pointer to the encryption routine
 *
 * @var crypto_interface::decrypt
 * decrypt is a crypto_op_func_t pointer to the decryption routine
 *
 * @var crypto_interface::has_support
 * has_support is used to inquire about which algos/modes are natively
 * supported
 */
struct crypto_interface {
    crypto_op_func_t encrypt;
    crypto_op_func_t decrypt;
    crypto_support_func_t has_support;
};

struct crypto_dev {
    struct os_dev dev;
    struct crypto_interface interface;
};

struct crypto_iovec {
    void *iov_base;
    size_t iov_len;
};

/**
 * Encrypt a buffer using custom parameters
 *
 * @note iv receives the initial vector and returns the final vector
 *       after running on the block, so subsequent calls can use this value
 *
 * @param crypto   OS device
 * @param algo     Algorithm to use (see CRYPTO_ALGO_*)
 * @param mode     Mode to use (see CRYPTO_MODE_*)
 * @param key      The key
 * @param keylen   Length of the key in bits
 * @param iv       NULL or initial value or nonce
 * @param inbuf    The input data to be encrypted
 * @param outbuf   Results of the encryption of data
 * @param len      Length of the buffer
 *
 * @return  number of bytes encrypted
 */
uint32_t crypto_encrypt_custom(struct crypto_dev *crypto, uint16_t algo,
        uint16_t mode, const void *key, uint16_t keylen, void *iv,
        const void *inbuf, void *outbuf, uint32_t len);

/**
 * Encrypt an iovec using custom parameters
 *
 * @note iv receives the initial vector and returns the final vector
 *       after running on the block, so subsequent calls can use this value
 *
 * @param crypto   OS device
 * @param algo     Algorithm to use (see CRYPTO_ALGO_*)
 * @param mode     Mode to use (see CRYPTO_MODE_*)
 * @param key      The key
 * @param keylen   Length of the key in bits
 * @param iv       NULL or initial value or nonce
 * @param iov      An iovec of the buffers to be in-place encrypted
 * @param iovlen   Number of buffers in the iovec
 *
 * @return Number of bytes encrypted
 */
uint32_t crypto_encryptv_custom(struct crypto_dev *crypto, uint16_t algo,
        uint16_t mode, const void *key, uint16_t keylen, void *iv,
        struct crypto_iovec *iov, uint32_t iovlen);

/**
 * Decrypt a buffer using custom parameters
 *
 * @note iv receives the initial vector and returns the final vector
 *       after running on the block, so subsequent calls can use this value
 *
 * @param crypto   OS device
 * @param algo     Algorithm to use (see CRYPTO_ALGO_*)
 * @param mode     Mode to use (see CRYPTO_MODE_*)
 * @param key      The key
 * @param keylen   Length of the key in bits
 * @param iv       NULL or initial value or nonce
 * @param inbuf    The input data to be encrypted
 * @param outbuf   Results of the encryption of data
 * @param len      Length of the buffer
 *
 * @return  number of bytes encrypted
 */
uint32_t crypto_decrypt_custom(struct crypto_dev *crypto, uint16_t algo,
        uint16_t mode, const void *key, uint16_t keylen, void *iv,
        const void *inbuf, void *outbuf, uint32_t len);

/**
 * Decrypt an iovec using custom parameters
 *
 * @note iv receives the initial vector and returns the final vector
 *       after running on the block, so subsequent calls can use this value
 *
 * @param crypto   OS device
 * @param algo     Algorithm to use (see CRYPTO_ALGO_*)
 * @param mode     Mode to use (see CRYPTO_MODE_*)
 * @param key      The key
 * @param keylen   Length of the key in bits
 * @param iv       NULL or initial value or nonce
 * @param iov      An iovec of the buffers to be in-place decrypted
 * @param iovlen   Number of buffers in the iovec
 *
 * @return Number of bytes decrypted
 */
uint32_t crypto_decryptv_custom(struct crypto_dev *crypto, uint16_t algo,
        uint16_t mode, const void *key, uint16_t keylen, void *iv,
        struct crypto_iovec *iov, uint32_t iovlen);

/*
 * Query Crypto HW capabilities
 *
 * @param crypto   OS device
 * @param op       CRYPTO_OP_ENCRYPTION or CRYPTO_OP_DECRYPTION
 * @param algo     Algorithm to check for (CRYPTO_ALGO_*)
 * @param mode     Mode to check for (CRYPTO_ALGO_*)
 * @param keylen   Length of the key
 *
 * @return true if the device has support, false otherwise.
 */
bool crypto_has_support(struct crypto_dev *crypto, uint8_t op, uint16_t algo,
        uint16_t mode, uint16_t keylen);

/*
 * AES helpers
 */

/**
 * Encrypt buffer using AES-ECB
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param inbuf    Input buffer
 * @param outbuf   Output buffer
 * @param len      Length of the buffers
 *
 * @return Number of bytes encrypted
 */
uint32_t crypto_encrypt_aes_ecb(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, const void *inbuf, void *outbuf, uint32_t len);

/**
 * Encrypt iovec using AES-ECB
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param iov      An iovec of the buffers to be in-place encrypted
 * @param iovlen   Number of buffers in the iovec
 *
 * @return Number of bytes encrypted
 */
uint32_t crypto_encryptv_aes_ecb(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, struct crypto_iovec *iov, uint32_t iovlen);

/**
 * Decrypt buffer using AES-ECB
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param inbuf    Input buffer
 * @param outbuf   Output buffer
 * @param len      Length of the buffers
 *
 * @return Number of bytes decrypted
 */
uint32_t crypto_decrypt_aes_ecb(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, const void *inbuf, void *outbuf, uint32_t len);

/**
 * Decrypt iovec using AES-ECB
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param iov      An iovec of the buffers to be in-place decrypted
 * @param iovlen   Number of buffers in the iovec
 *
 * @return Number of bytes decrypted
 */
uint32_t crypto_decryptv_aes_ecb(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, struct crypto_iovec *iov, uint32_t iovlen);

/**
 * Encrypt buffer using AES-CBC
 *
 * @note Updated iv is written after function finishes
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param iv       Initial vector
 * @param inbuf    Input buffer
 * @param outbuf   Output buffer
 * @param len      Length of the buffers
 *
 * @return Number of bytes encrypted
 */
uint32_t crypto_encrypt_aes_cbc(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *iv, const void *inbuf, void *outbuf, uint32_t len);

/**
 * Encrypt iovec using AES-CBC
 *
 * @note Updated iv is written after function finishes
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param iv       Initial vector
 * @param iov      An iovec of the buffers to be in-place encrypted
 * @param iovlen   Number of buffers in the iovec
 *
 * @return Number of bytes encrypted
 */
uint32_t crypto_encryptv_aes_cbc(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *iv, struct crypto_iovec *iov, uint32_t iovlen);

/**
 * Decrypt buffer using AES-CBC
 *
 * @note Updated iv is written after function finishes
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param iv       Initial vector
 * @param inbuf    Input buffer
 * @param outbuf   Output buffer
 * @param len      Length of the buffers
 *
 * @return Number of bytes decrypted
 */
uint32_t crypto_decrypt_aes_cbc(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *iv, const void *inbuf, void *outbuf,
        uint32_t len);

/**
 * Decrypt iovec using AES-CBC
 *
 * @note Updated iv is written after function finishes
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param iv       Initial vector
 * @param iov      An iovec of the buffers to be in-place decrypted
 * @param iovlen   Number of buffers in the iovec
 *
 * @return Number of bytes decrypted
 */
uint32_t crypto_decryptv_aes_cbc(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *iv, struct crypto_iovec *iov,  uint32_t iovlen);

/**
 * Encrypt buffer using AES-CTR
 *
 * @note Updated nonce is written after function finishes
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param nonce    Nonce
 * @param inbuf    Input buffer
 * @param outbuf   Output buffer
 * @param len      Length of the buffers
 *
 * @return Number of bytes encrypted
 */
uint32_t crypto_encrypt_aes_ctr(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *nonce, const void *inbuf, void *outbuf,
        uint32_t len);

/**
 * Encrypt iovec using AES-CTR
 *
 * @note Updated nonce is written after function finishes
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param nonce    Nonce
 * @param iov      An iovec of the buffers to be in-place encrypted
 * @param iovlen   Number of buffers in the iovec
 *
 * @return Number of bytes encrypted
 */
uint32_t crypto_encryptv_aes_ctr(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *nonce, struct crypto_iovec *iov, uint32_t iovlen);

/**
 * Decrypt buffer using AES-CTR
 *
 * @note Updated nonce is written after function finishes
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param nonce    Nonce
 * @param inbuf    Input buffer
 * @param outbuf   Output buffer
 * @param len      Length of the buffers
 *
 * @return Number of bytes decrypted
 */
uint32_t crypto_decrypt_aes_ctr(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *nonce, const void *inbuf, void *outbuf,
        uint32_t len);

/**
 * Decrypt iovec using AES-CTR
 *
 * @note Updated nonce is written after function finishes
 *
 * @param crypto   OS device
 * @param key      Key
 * @param keylen   Key length should 128, 192 or 256 (AES size)
 * @param nonce    Nonce
 * @param iov      An iovec of the buffers to be in-place decrypted
 * @param iovlen   Number of buffers in the iovec
 *
 * @return Number of bytes decrypted
 */
uint32_t crypto_decryptv_aes_ctr(struct crypto_dev *crypto, const void *key,
        uint16_t keylen, void *nonce, struct crypto_iovec *iov, uint32_t iovlen);

#ifdef __cplusplus
}
#endif

#endif /* __CRYPTO_H__ */
