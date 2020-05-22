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

#include <os/mynewt.h>

#if MYNEWT_VAL(MBEDTLS_AES_ALT)

#include "crypto/crypto.h"
#include "crypto/aes_alt.h"
#include "mbedtls/aes.h"

void
mbedtls_aes_init(mbedtls_aes_context *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->crypto = (struct crypto_dev *) os_dev_open("crypto", OS_TIMEOUT_NEVER,
            NULL);
    assert(ctx->crypto);
}

void
mbedtls_aes_free(mbedtls_aes_context *ctx)
{
    os_dev_close((struct os_dev *)ctx->crypto);
    memset(ctx, 0, sizeof(*ctx));
}

static int
mbedtls_aes_setkey(mbedtls_aes_context *ctx, const unsigned char *key,
                   unsigned int keybits)
{
    switch (keybits) {
    case AES_128_KEY_LEN * 8:
    case AES_192_KEY_LEN * 8:
    case AES_256_KEY_LEN * 8:
        memcpy(ctx->key, key, keybits / 8);
        ctx->keylen = keybits;
        return 0;
    }

    return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
}

static int
mbedtls_aes_validate_input(mbedtls_aes_context *ctx, const unsigned char *key,
                           unsigned int keybits)
{
    assert(ctx);

    if (key == NULL) {
        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }

    return 0;
}

int
mbedtls_aes_setkey_enc(mbedtls_aes_context *ctx, const unsigned char *key,
                       unsigned int keybits)
{
    int ret = mbedtls_aes_validate_input(ctx, key, keybits);

    if (ret != 0) {
        return ret;
    }

    return mbedtls_aes_setkey(ctx, key, keybits);
}

int
mbedtls_aes_setkey_dec(mbedtls_aes_context *ctx, const unsigned char *key,
                       unsigned int keybits)
{
    return mbedtls_aes_setkey_enc(ctx, key, keybits);
}

int
mbedtls_aes_crypt_ecb(mbedtls_aes_context *ctx, int mode,
                      const unsigned char input[16], unsigned char output[16])
{
    int ret = crypto_encrypt_aes_ecb(ctx->crypto, ctx->key, ctx->keylen,
                                     (const uint8_t *)input, (uint8_t *)output, AES_BLOCK_LEN);
    return (ret == AES_BLOCK_LEN) ? 0 : -1;
}

#endif /* MYNEWT_VAL(MBEDTLS_AES_ALT) */
