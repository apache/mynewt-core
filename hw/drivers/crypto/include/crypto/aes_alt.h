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

#ifndef __AES_ALT_H__
#define __AES_ALT_H__

#include <os/mynewt.h>

#if MYNEWT_VAL(MBEDTLS_AES_ALT)

#ifdef __cplusplus
extern "C" {
#endif

#include <crypto/crypto.h>

typedef struct mbedtls_aes_context
{
    struct crypto_dev *crypto;
    uint8_t key[AES_MAX_KEY_LEN];
    uint8_t keylen;
} mbedtls_aes_context;

void mbedtls_aes_init(mbedtls_aes_context *ctx);
void mbedtls_aes_free(mbedtls_aes_context *ctx);
int mbedtls_aes_setkey_enc(mbedtls_aes_context *ctx, const unsigned char *key,
        unsigned int keybits);
int mbedtls_aes_crypt_ecb(mbedtls_aes_context *ctx, int mode,
        const unsigned char input[16], unsigned char output[16]);

#ifdef __cplusplus
}
#endif

#endif /* MYNEWT_VAL(MBEDTLS_AES_ALT) */

#endif /* __AES_ALT_H__ */
