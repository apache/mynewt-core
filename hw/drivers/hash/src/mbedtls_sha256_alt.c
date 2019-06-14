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

#if MYNEWT_VAL(MBEDTLS_SHA256_ALT)

#include "hash/hash.h"
#include "hash/sha256_alt.h"

void
mbedtls_sha256_init(mbedtls_sha256_context *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->hash = (struct hash_dev *) os_dev_open("hash", OS_TIMEOUT_NEVER,
            NULL);
    assert(ctx->hash);
}

void
mbedtls_sha256_free(mbedtls_sha256_context *ctx)
{
    os_dev_close((struct os_dev *)ctx->hash);
    memset(ctx, 0, sizeof(*ctx));
}

void
mbedtls_sha256_clone(mbedtls_sha256_context *dst,
        const mbedtls_sha256_context *src)
{
    memcpy(dst, src, sizeof(*dst));
}

int
mbedtls_sha256_starts_ret(mbedtls_sha256_context *ctx, int is224)
{
    /* SHA-224 not supported */
    if (is224) {
        return -1;
    }
    return hash_sha256_start(&ctx->sha256ctx, ctx->hash);
}

int mbedtls_sha256_update_ret(mbedtls_sha256_context *ctx,
        const unsigned char *input, size_t ilen)
{
    return hash_sha256_update(&ctx->sha256ctx, input, ilen);
}

int mbedtls_sha256_finish_ret(mbedtls_sha256_context *ctx,
        unsigned char output[32])
{
    return hash_sha256_finish(&ctx->sha256ctx, output);
}

/*
 * XXX deprecated mbedTLS functions
 */

void
mbedtls_sha256_starts(mbedtls_sha256_context *ctx, int is224)
{
    /* SHA-224 not supported */
    if (is224) {
        return;
    }
    (void)hash_sha256_start(&ctx->sha256ctx, ctx->hash);
}

void mbedtls_sha256_update(mbedtls_sha256_context *ctx,
        const unsigned char *input, size_t ilen)
{
    (void)hash_sha256_update(&ctx->sha256ctx, input, ilen);
}

void mbedtls_sha256_finish(mbedtls_sha256_context *ctx,
        unsigned char output[32])
{
    (void)hash_sha256_finish(&ctx->sha256ctx, output);
}

#endif /* MYNEWT_VAL(MBEDTLS_SHA256_ALT) */
