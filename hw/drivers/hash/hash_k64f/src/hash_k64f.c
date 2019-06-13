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

#include "hash/hash.h"
#include "hash_k64f/hash_k64f.h"

static struct os_mutex gmtx;
static uint32_t g_algos = HASH_ALGO_SHA256;

/*
 * These routines are exported by NXP's provided CAU and mmCAU software
 * library.
 */
extern int mmcau_sha256_initialize_output(const unsigned int *output);
extern void mmcau_sha256_hash_n(const unsigned char *msg_data,
        const int num_blks, unsigned int *sha256_state);
extern void mmcau_sha256_update(const unsigned char *msg_data,
        const int num_blks, unsigned int *sha256_state);
extern void mmcau_sha256_hash (const unsigned char *msg_data,
        unsigned int *sha256_state);

/*
 * Fallback CAU routines in C.
 */
extern int cau_sha256_initialize_output(const unsigned int *output);
extern void cau_sha256_hash_n(const unsigned char *msg_data,
        const int num_blks, unsigned int *sha256_state);
extern void cau_sha256_update(const unsigned char *msg_data,
        const int num_blks, unsigned int *sha256_state);
extern void cau_sha256_hash(const unsigned char *msg_data,
        unsigned int *sha256_state);

static int
k64f_hash_start(struct hash_dev *hash, void *ctx, uint16_t algo)
{
    struct hash_sha256_context *sha256ctx;

    if (!(algo & g_algos)) {
        return -1;
    }

    os_mutex_pend(&gmtx, OS_TIMEOUT_NEVER);
    sha256ctx = (struct hash_sha256_context *)ctx;
    (void)cau_sha256_initialize_output(sha256ctx->output);
    sha256ctx->len = 0;
    sha256ctx->remain = 0;

    return 0;
}

static int
k64f_hash_update(struct hash_dev *hash, void *ctx, uint16_t algo,
        const void *inbuf, uint32_t inlen)
{
    uint32_t i;
    uint32_t remain;
    struct hash_sha256_context *sha256ctx;
    uint8_t *u8p;

    sha256ctx = (struct hash_sha256_context *)ctx;
    i = 0;
    remain = inlen;
    u8p = (uint8_t *)inbuf;

    while (remain >= SHA256_BLOCK_LEN) {
        cau_sha256_hash_n((const unsigned char *)&u8p[i], 1,
                sha256ctx->output);
        remain -= SHA256_BLOCK_LEN;
        i += SHA256_BLOCK_LEN;
    }

    sha256ctx->len += i;

    if (remain > 0) {
        memcpy(sha256ctx->pad, &u8p[i], remain);
        sha256ctx->remain = remain;
    }

    return 0;
}

static int
k64f_hash_finish(struct hash_dev *hash, void *ctx, uint16_t algo,
        void *outbuf)
{
    uint32_t i;
    uint32_t *b32p;
    uint8_t ARRSZ = sizeof(uint64_t);
    struct hash_sha256_context *sha256ctx;

    sha256ctx = (struct hash_sha256_context *)ctx;
    sha256ctx->pad[sha256ctx->remain] = 0x80;
    sha256ctx->len += sha256ctx->remain;
    sha256ctx->remain++;

    i = sha256ctx->remain;

    if (sha256ctx->remain >= (SHA256_BLOCK_LEN - ARRSZ)) {
        memset(&sha256ctx->pad[i], 0, SHA256_BLOCK_LEN - i);
        cau_sha256_hash_n((const unsigned char *)sha256ctx->pad, 1,
                sha256ctx->output);
        i = 0;
    }

    /*
     * Pad remaining data of the last block and add the original message's
     * length as big endian 64-bit
     */
    memset(&sha256ctx->pad[i], 0, SHA256_BLOCK_LEN - i);
    *((uint64_t *)&sha256ctx->pad[SHA256_BLOCK_LEN - sizeof(uint64_t)]) =
        os_bswap_64(sha256ctx->len * 8);
    cau_sha256_hash_n((const unsigned char *)sha256ctx->pad, 1,
            sha256ctx->output);

    b32p = (uint32_t *)outbuf;
    for (i = 0; i < 8; i++) {
        b32p[i] = os_bswap_32(sha256ctx->output[i]);
    }

    os_mutex_release(&gmtx);
    return 0;
}

static int
k64f_hash_dev_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    struct hash_dev *hash;

    hash = (struct hash_dev *)dev;
    assert(hash);

    /* Not reentrant */
    if (dev->od_flags & OS_DEV_F_STATUS_OPEN) {
        return OS_EBUSY;
    }

    return OS_OK;
}

int
k64f_hash_dev_init(struct os_dev *dev, void *arg)
{
    struct hash_dev *hash;

    hash = (struct hash_dev *)dev;
    assert(hash);

    OS_DEV_SETHANDLERS(dev, k64f_hash_dev_open, NULL);

    assert(os_mutex_init(&gmtx) == 0);

    hash->interface.start = k64f_hash_start;
    hash->interface.update = k64f_hash_update;
    hash->interface.finish = k64f_hash_finish;
    hash->interface.algomask = g_algos;

    return 0;
}
