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
#include "mbedtls/aesni.h"
#endif

#if !defined(MBEDTLS_GCM_ALT)

/* Parameter validation macros */
#define GCM_VALIDATE_RET(cond) \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_GCM_BAD_INPUT)
#define GCM_VALIDATE(cond) \
    MBEDTLS_INTERNAL_VALIDATE(cond)

/*
 * Precompute small multiples of H, that is set
 *      HH[i] || HL[i] = H times i,
 * where i is seen as a field element as in [MGV], ie high-order bits
 * correspond to low powers of P. The result is stored in the same way, that
 * is the high-order bit of HH corresponds to P^0 and the low-order bit of HL
 * corresponds to P^127.
 */
static int gcm_gen_table(mbedtls_gcm_context *ctx)
{
    int ret, i, j;
    uint64_t hi, lo;
    uint64_t vl, vh;
    unsigned char h[16];
    size_t olen = 0;

    memset(h, 0, 16);
    if ((ret = mbedtls_cipher_update(&ctx->cipher_ctx, h, 16, h, &olen)) != 0) {
        return ret;
    }

    /* pack h as two 64-bits ints, big-endian */
    hi = MBEDTLS_GET_UINT32_BE(h,  0);
    lo = MBEDTLS_GET_UINT32_BE(h,  4);
    vh = (uint64_t) hi << 32 | lo;

    hi = MBEDTLS_GET_UINT32_BE(h,  8);
    lo = MBEDTLS_GET_UINT32_BE(h,  12);
    vl = (uint64_t) hi << 32 | lo;

    /* 8 = 1000 corresponds to 1 in GF(2^128) */
    ctx->HL[8] = vl;
    ctx->HH[8] = vh;

#if defined(MBEDTLS_AESNI_HAVE_CODE)
    /* With CLMUL support, we need only h, not the rest of the table */
    if (mbedtls_aesni_has_support(MBEDTLS_AESNI_CLMUL)) {
        return 0;
    }
#endif

    /* 0 corresponds to 0 in GF(2^128) */
    ctx->HH[0] = 0;
    ctx->HL[0] = 0;

    for (i = 4; i > 0; i >>= 1) {
        uint32_t T = (vl & 1) * 0xe1000000U;
        vl  = (vh << 63) | (vl >> 1);
        vh  = (vh >> 1) ^ ((uint64_t) T << 32);

        ctx->HL[i] = vl;
        ctx->HH[i] = vh;
    }

    for (i = 2; i <= 8; i *= 2) {
        uint64_t *HiL = ctx->HL + i, *HiH = ctx->HH + i;
        vh = *HiH;
        vl = *HiL;
        for (j = 1; j < i; j++) {
            HiH[j] = vh ^ ctx->HH[j];
            HiL[j] = vl ^ ctx->HL[j];
        }
    }

    return 0;
}

/*
 * Shoup's method for multiplication use this table with
 *      last4[x] = x times P^128
 * where x and last4[x] are seen as elements of GF(2^128) as in [MGV]
 */
static const uint64_t last4[16] =
{
    0x0000, 0x1c20, 0x3840, 0x2460,
    0x7080, 0x6ca0, 0x48c0, 0x54e0,
    0xe100, 0xfd20, 0xd940, 0xc560,
    0x9180, 0x8da0, 0xa9c0, 0xb5e0
};

/*
 * Sets output to x times H using the precomputed tables.
 * x and output are seen as elements of GF(2^128) as in [MGV].
 */
static void gcm_mult(mbedtls_gcm_context *ctx, const unsigned char x[16],
                     unsigned char output[16])
{
    int i = 0;
    unsigned char lo, hi, rem;
    uint64_t zh, zl;

#if defined(MBEDTLS_AESNI_HAVE_CODE)
    if (mbedtls_aesni_has_support(MBEDTLS_AESNI_CLMUL)) {
        unsigned char h[16];

        MBEDTLS_PUT_UINT32_BE(ctx->HH[8] >> 32, h,  0);
        MBEDTLS_PUT_UINT32_BE(ctx->HH[8],       h,  4);
        MBEDTLS_PUT_UINT32_BE(ctx->HL[8] >> 32, h,  8);
        MBEDTLS_PUT_UINT32_BE(ctx->HL[8],       h, 12);

        mbedtls_aesni_gcm_mult(output, x, h);
        return;
    }
#endif /* MBEDTLS_AESNI_HAVE_CODE */

    lo = x[15] & 0xf;

    zh = ctx->HH[lo];
    zl = ctx->HL[lo];

    for (i = 15; i >= 0; i--) {
        lo = x[i] & 0xf;
        hi = (x[i] >> 4) & 0xf;

        if (i != 15) {
            rem = (unsigned char) zl & 0xf;
            zl = (zh << 60) | (zl >> 4);
            zh = (zh >> 4);
            zh ^= (uint64_t) last4[rem] << 48;
            zh ^= ctx->HH[lo];
            zl ^= ctx->HL[lo];

        }

        rem = (unsigned char) zl & 0xf;
        zl = (zh << 60) | (zl >> 4);
        zh = (zh >> 4);
        zh ^= (uint64_t) last4[rem] << 48;
        zh ^= ctx->HH[hi];
        zl ^= ctx->HL[hi];
    }

    MBEDTLS_PUT_UINT32_BE(zh >> 32, output, 0);
    MBEDTLS_PUT_UINT32_BE(zh, output, 4);
    MBEDTLS_PUT_UINT32_BE(zl >> 32, output, 8);
    MBEDTLS_PUT_UINT32_BE(zl, output, 12);
}

int mbedtls_gcm_update_add( mbedtls_gcm_context *ctx,
                            size_t add_len,
                            const unsigned char *add )
{
    const unsigned char *p;
    size_t i;
    size_t use_len;

    if ( ctx->add_len & 15 )
    {
        return( MBEDTLS_ERR_GCM_BAD_INPUT );
    }
    ctx->add_len += add_len;
    p = add;

    while (add_len > 0)
    {
        use_len = ( add_len < 16 ) ? add_len : 16;

        for( i = 0; i < use_len; i++ ) {
            ctx->buf[i] ^= p[i];
        }
        gcm_mult( ctx, ctx->buf, ctx->buf );

        add_len -= use_len;
        p += use_len;
    }

    return( 0 );
}

int mbedtls_gcm_setkey_noalloc( mbedtls_gcm_context *ctx,
                                const mbedtls_cipher_info_t *cipher_info,
                                const unsigned char *key,
                                void *cipher_ctx)
{
    int ret;

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

    if( ( ret = mbedtls_cipher_setkey( &ctx->cipher_ctx, key,
                               cipher_info->key_bitlen,
                               MBEDTLS_ENCRYPT ) ) != 0 )
    {
        return( ret );
    }

    if( ( ret = gcm_gen_table( ctx ) ) != 0 )
        return( ret );

    return( 0 );
}
#endif /* !MBEDTLS_GCM_ALT */

#endif /* MBEDTLS_GCM_C */
