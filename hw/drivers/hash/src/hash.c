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

#include "hash/hash.h"

int
hash_custom_process(struct hash_dev *hash, uint16_t algo, const void *inbuf,
        uint32_t inlen, void *outbuf)
{
    int rc;
    struct hash_generic_context ctx;

    if ((hash->interface.algomask & algo) == 0) {
        return -1;
    }

    rc = hash->interface.start(hash, &ctx, algo);
    if (rc) {
        return -1;
    }

    rc = hash->interface.update(hash, &ctx, algo, inbuf, inlen);
    if (rc) {
        return -1;
    }

    rc = hash->interface.finish(hash, &ctx, algo, outbuf);
    return rc;
}

int
hash_custom_start(struct hash_dev *hash, void *ctx, uint16_t algo)
{
    if ((hash->interface.algomask & algo) == 0) {
        return -1;
    }

    return hash->interface.start(hash, ctx, algo);
}

int
hash_custom_update(struct hash_dev *hash, void *ctx, uint16_t algo,
        const void *inbuf, uint32_t inlen)
{
    return hash->interface.update(hash, ctx, algo, inbuf, inlen);
}

int
hash_custom_finish(struct hash_dev *hash, void *ctx, uint16_t algo,
        void *outbuf)
{
    return hash->interface.finish(hash, ctx, algo, outbuf);
}

/*
 * Helpers
 */

int
hash_sha256_process(struct hash_dev *hash, const void *inbuf, uint32_t inlen,
        void *outbuf)
{
    return hash_custom_process(hash, HASH_ALGO_SHA256, inbuf, inlen, outbuf);
}

int
hash_sha256_start(struct hash_sha256_context *ctx, struct hash_dev *hash)
{
    ctx->dev = hash;
    return hash_custom_start(hash, ctx, HASH_ALGO_SHA256);
}

int
hash_sha256_update(struct hash_sha256_context *ctx, const void *inbuf,
        uint32_t inlen)
{
    assert(ctx->dev);
    return hash_custom_update(ctx->dev, ctx, HASH_ALGO_SHA256, inbuf, inlen);
}

int
hash_sha256_finish(struct hash_sha256_context *ctx, void *outbuf)
{
    int rc;

    assert(ctx->dev);

    rc = hash_custom_finish(ctx->dev, ctx, HASH_ALGO_SHA256, outbuf);
    ctx->dev = NULL;

    return rc;
}

/*
 * Interface
 */

bool
hash_has_support(struct hash_dev *hash, uint16_t algo)
{
    return ((hash->interface.algomask & algo) != 0);
}
