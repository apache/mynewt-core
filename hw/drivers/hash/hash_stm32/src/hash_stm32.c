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
#include "mcu/stm32_hal.h"
#include "hash/hash.h"
#include "hash_stm32/hash_stm32.h"

static struct os_mutex gmtx;

/*
 * STM32F415xx and STM32F417xx have a HASH unit that only supports MD5/SHA1
 */

#if defined(STM32F415xx) || defined(STM32F417xx)
static uint32_t g_algos = 0;
#else
static uint32_t g_algos = HASH_ALGO_SHA224 | HASH_ALGO_SHA256;
#endif

static int
stm32_hash_start(struct hash_dev *hash, void *ctx, uint16_t algo)
{
    uint32_t algomask;

    if (!(algo & g_algos)) {
        return -1;
    }

    os_mutex_pend(&gmtx, OS_TIMEOUT_NEVER);

    switch (algo) {
    case HASH_ALGO_SHA224:
        algomask = HASH_ALGOSELECTION_SHA224;
        break;
    case HASH_ALGO_SHA256:
        algomask = HASH_ALGOSELECTION_SHA256;
        break;
    default:
        assert(0);
        return -1;
    }

    ((struct hash_sha2_context *)ctx)->remain = 0;
    HASH->CR = algomask | HASH_CR_INIT | HASH_DATATYPE_8B;

    return 0;
}

static int
stm32_hash_update(struct hash_dev *hash, void *ctx, uint16_t algo,
                  const void *inbuf, uint32_t inlen)
{
    uint32_t *u32p;
    uint8_t *u8p;
    uint32_t remain;
    struct hash_sha2_context *sha2ctx;
    uint8_t statesz;

    sha2ctx = (struct hash_sha2_context *)ctx;
    statesz = sizeof sha2ctx->state;

    remain = inlen;
    u8p = (uint8_t *)inbuf;
    if (sha2ctx->remain) {
        while (sha2ctx->remain < statesz && remain) {
            sha2ctx->state[sha2ctx->remain++] = *(u8p++);
            remain--;
        }
        if (sha2ctx->remain == statesz) {
            HASH->DIN = *((uint32_t *)sha2ctx->state);
            sha2ctx->remain = 0;
        }
    }

    u32p = (uint32_t *)u8p;
    while (remain >= statesz) {
        HASH->DIN = *(u32p++);
        remain -= statesz;
    }

    if (remain) {
        u8p = (uint8_t *)u32p;
        while (sha2ctx->remain < statesz && remain) {
            sha2ctx->state[sha2ctx->remain++] = *u8p++;
            remain--;
        }
    }

    return 0;
}

static int
stm32_hash_finish(struct hash_dev *hash, void *ctx, uint16_t algo,
        void *outbuf)
{
    uint8_t digestsz;
    uint32_t *u32p;
    int i;
    struct hash_sha2_context *sha2ctx;

    sha2ctx = (struct hash_sha2_context *)ctx;

    if (sha2ctx->remain) {
        HASH->DIN = *((uint32_t *)sha2ctx->state);
    }
    __HAL_HASH_SET_NBVALIDBITS(sha2ctx->remain);

    switch (algo) {
    case HASH_ALGO_SHA224:
        digestsz = SHA224_DIGEST_LEN / 4;
        break;
    case HASH_ALGO_SHA256:
        digestsz = SHA256_DIGEST_LEN / 4;
        break;
    default:
        assert(0);
        return -1;
    }

    __HAL_HASH_START_DIGEST();

    while (HASH->SR & HASH_FLAG_BUSY) ;

    u32p = (uint32_t *)outbuf;
    for (i = 0; i < digestsz; i++) {
        /*
         * XXX HASH_DIGEST is only available on devices that support
         * SHA-2, and its first 5 words are mapped at the same address
         * as HASH below.
         */
        u32p[i] = os_bswap_32(HASH_DIGEST->HR[i]);
    }

    os_mutex_release(&gmtx);
    return 0;
}

static int
stm32_hash_dev_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    struct hash_dev *hash;

    hash = (struct hash_dev *)dev;
    assert(hash);

    /* XXX Not reentrant? */
    if (dev->od_flags & OS_DEV_F_STATUS_OPEN) {
        return OS_EBUSY;
    }

    __HAL_RCC_HASH_CLK_ENABLE();

    return 0;
}

int
stm32_hash_dev_init(struct os_dev *dev, void *arg)
{
    struct hash_dev *hash;

    hash = (struct hash_dev *)dev;
    assert(hash);

    OS_DEV_SETHANDLERS(dev, stm32_hash_dev_open, NULL);

    assert(os_mutex_init(&gmtx) == 0);

    hash->interface.start = stm32_hash_start;
    hash->interface.update = stm32_hash_update;
    hash->interface.finish = stm32_hash_finish;
    hash->interface.algomask = g_algos;

    return 0;
}
