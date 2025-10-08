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

/* Mynewt Mbed TLS extension (based on gcm.c)
 * TODO should be upstream eventually..
 */

/*
 *  NIST SP800-38D compliant GCM implementation
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * http://csrc.nist.gov/publications/nistpubs/800-38D/SP-800-38D.pdf
 *
 * See also:
 * [MGV] http://csrc.nist.gov/groups/ST/toolkit/BCM/documents/proposedmodes/gcm/gcm-revised-spec.pdf
 *
 * We use the algorithm described as Shoup's method with 4-bit tables in
 * [MGV] 4.1, pp. 12-13, to enhance speed without using too much memory.
 */

#include <mbedtls/gcm_mynewt.h>
#include <common.h>

#if defined(MBEDTLS_GCM_C)

#include "mbedtls/gcm.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#include <string.h>

#if defined(MBEDTLS_AESNI_C)
#include "aesni.h"
#endif

#if !defined(MBEDTLS_GCM_ALT)

/* Used to select the acceleration mechanism */
#define MBEDTLS_GCM_ACC_SMALLTABLE 0
#define MBEDTLS_GCM_ACC_LARGETABLE 1
#define MBEDTLS_GCM_ACC_AESNI      2
#define MBEDTLS_GCM_ACC_AESCE      3

static inline void
gcm_set_acceleration(mbedtls_gcm_context *ctx)
{
#if defined(MBEDTLS_GCM_LARGE_TABLE)
    ctx->acceleration = MBEDTLS_GCM_ACC_LARGETABLE;
#else
    ctx->acceleration = MBEDTLS_GCM_ACC_SMALLTABLE;
#endif

#if defined(MBEDTLS_AESNI_HAVE_CODE)
    /* With CLMUL support, we need only h, not the rest of the table */
    if (mbedtls_aesni_has_support(MBEDTLS_AESNI_CLMUL)) {
        ctx->acceleration = MBEDTLS_GCM_ACC_AESNI;
    }
#endif

#if defined(MBEDTLS_AESCE_HAVE_CODE)
    if (MBEDTLS_AESCE_HAS_SUPPORT()) {
        ctx->acceleration = MBEDTLS_GCM_ACC_AESCE;
    }
#endif
}

static inline void
gcm_gen_table_rightshift(uint64_t dst[2], const uint64_t src[2])
{
    uint8_t *u8Dst = (uint8_t *)dst;
    uint8_t *u8Src = (uint8_t *)src;

    MBEDTLS_PUT_UINT64_BE(MBEDTLS_GET_UINT64_BE(&src[1], 0) >> 1, &dst[1], 0);
    u8Dst[8] |= (u8Src[7] & 0x01) << 7;
    MBEDTLS_PUT_UINT64_BE(MBEDTLS_GET_UINT64_BE(&src[0], 0) >> 1, &dst[0], 0);
    u8Dst[0] ^= (u8Src[15] & 0x01) ? 0xE1 : 0;
}

/*
 * Precompute small multiples of H, that is set
 *      HH[i] || HL[i] = H times i,
 * where i is seen as a field element as in [MGV], ie high-order bits
 * correspond to low powers of P. The result is stored in the same way, that
 * is the high-order bit of HH corresponds to P^0 and the low-order bit of HL
 * corresponds to P^127.
 */
static int
gcm_gen_table(mbedtls_gcm_context *ctx)
{
    int ret, i, j;
    uint64_t u64h[2] = { 0 };
    uint8_t *h = (uint8_t *)u64h;

#if defined(MBEDTLS_BLOCK_CIPHER_C)
    ret = mbedtls_block_cipher_encrypt(&ctx->block_cipher_ctx, h, h);
#else
    size_t olen = 0;
    ret = mbedtls_cipher_update(&ctx->cipher_ctx, h, 16, h, &olen);
#endif
    if (ret != 0) {
        return ret;
    }

    gcm_set_acceleration(ctx);

    /* MBEDTLS_GCM_HTABLE_SIZE/2 = 1000 corresponds to 1 in GF(2^128) */
    ctx->H[MBEDTLS_GCM_HTABLE_SIZE / 2][0] = u64h[0];
    ctx->H[MBEDTLS_GCM_HTABLE_SIZE / 2][1] = u64h[1];

    switch (ctx->acceleration) {
#if defined(MBEDTLS_AESNI_HAVE_CODE)
    case MBEDTLS_GCM_ACC_AESNI:
        return 0;
#endif

#if defined(MBEDTLS_AESCE_HAVE_CODE)
    case MBEDTLS_GCM_ACC_AESCE:
        return 0;
#endif

    default:
        /* 0 corresponds to 0 in GF(2^128) */
        ctx->H[0][0] = 0;
        ctx->H[0][1] = 0;

        for (i = MBEDTLS_GCM_HTABLE_SIZE / 4; i > 0; i >>= 1) {
            gcm_gen_table_rightshift(ctx->H[i], ctx->H[i * 2]);
        }

#if !defined(MBEDTLS_GCM_LARGE_TABLE)
        /* pack elements of H as 64-bits ints, big-endian */
        for (i = MBEDTLS_GCM_HTABLE_SIZE / 2; i > 0; i >>= 1) {
            MBEDTLS_PUT_UINT64_BE(ctx->H[i][0], &ctx->H[i][0], 0);
            MBEDTLS_PUT_UINT64_BE(ctx->H[i][1], &ctx->H[i][1], 0);
        }
#endif

        for (i = 2; i < MBEDTLS_GCM_HTABLE_SIZE; i <<= 1) {
            for (j = 1; j < i; j++) {
                mbedtls_xor_no_simd((unsigned char *)ctx->H[i + j],
                                    (unsigned char *)ctx->H[i],
                                    (unsigned char *)ctx->H[j], 16);
            }
        }
    }

    return 0;
}

int
mbedtls_gcm_setkey_noalloc(mbedtls_gcm_context *ctx,
                           const mbedtls_cipher_info_t *cipher_info,
                           const unsigned char *key, unsigned int keybits,
                           void *cipher_ctx)
{
    int ret;

    if (keybits != 128 && keybits != 192 && keybits != 256) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    ctx->cipher_ctx.cipher_info = cipher_info;
    ctx->cipher_ctx.cipher_ctx = cipher_ctx;
#if defined(MBEDTLS_CIPHER_MODE_WITH_PADDING)
    /*
     * Ignore possible errors caused by a cipher mode that doesn't use padding
     */
#if defined(MBEDTLS_CIPHER_PADDING_PKCS7)
    (void) mbedtls_cipher_set_padding_mode( &ctx->cipher_ctx,
                               MBEDTLS_PADDING_PKCS7 );
#else
    (void) mbedtls_cipher_set_padding_mode( &ctx->cipher_ctx,
                               MBEDTLS_PADDING_NONE );
#endif
#endif /* MBEDTLS_CIPHER_MODE_WITH_PADDING */

    if ((ret = mbedtls_cipher_setkey(&ctx->cipher_ctx, key, keybits,
                                     MBEDTLS_ENCRYPT)) != 0) {
        return ret;
    }

    if ((ret = gcm_gen_table(ctx)) != 0) {
        return ret;
    }

    return 0;
}
#endif /* !MBEDTLS_GCM_ALT */

#endif /* MBEDTLS_GCM_C */
