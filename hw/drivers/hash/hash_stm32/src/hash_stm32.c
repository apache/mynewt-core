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

#if defined(STM32F437xx) || defined(STM32F439xx) || defined(STM32F479xx) || \
    defined(STM32F756xx) || defined(STM32F777xx) || defined(STM32F779xx) || \
    defined(STM32L4A6xx) || defined(STM32L4S5xx) || defined(STM32L4S7xx) || \
    defined(STM32L4S9xx)
static uint32_t g_algos = HASH_ALGO_SHA224 | HASH_ALGO_SHA256;
#else
static uint32_t g_algos = 0;
#endif

static int
stm32_hash_start(struct hash_dev *hash, void *ctx, uint16_t algo)
{
    uint32_t algomask;

    (void)ctx;

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

    HASH->CR = algomask | HASH_CR_INIT | HASH_DATATYPE_8B;

    return 0;
}

static int
stm32_hash_update(struct hash_dev *hash, void *ctx, uint16_t algo,
        const void *inbuf, uint32_t inlen)
{
    uint32_t *u32p;
    int i;

    (void)ctx;

    u32p = (uint32_t *)inbuf;
    for (i = 0; i < (inlen + 3) / 4; i++) {
        HASH->DIN = u32p[i];
    }
    __HAL_HASH_SET_NBVALIDBITS(inlen);

    return 0;
}

static int
stm32_hash_finish(struct hash_dev *hash, void *ctx, uint16_t algo,
        void *outbuf)
{
    uint8_t digestsz;
    uint32_t *u32p;
    int i;

    (void)ctx;

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
