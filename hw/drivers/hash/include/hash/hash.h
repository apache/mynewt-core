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

#ifndef __HASH_H__
#define __HASH_H__

#include <inttypes.h>
#include <stddef.h>
#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HASH definitions
 */
#define SHA224_DIGEST_LEN              28
#define SHA256_DIGEST_LEN              32
#define HASH_MAX_DIGEST_LEN            SHA256_DIGEST_LEN

#define SHA256_BLOCK_LEN               64  /* 512 bits */
#define HASH_MAX_BLOCK_LEN             SHA256_BLOCK_LEN

/*
 * MCU specific context structs
 */
#include "hash_context.h"

struct hash_generic_context {
    union {
        struct hash_sha224_context sha224ctx;
        struct hash_sha256_context sha256ctx;
    };
};

/*
 * Driver capabilities
 */
#define HASH_ALGO_SHA224               0x0001
#define HASH_ALGO_SHA256               0x0002
#define HASH_ALGO_SHA512               0x0004

struct hash_dev;

typedef int (* hash_start_op_func_t)(struct hash_dev *hash, void *ctx,
        uint16_t algo);
typedef int (* hash_update_op_func_t)(struct hash_dev *hash, void *ctx,
        uint16_t algo, const void *inbuf, uint32_t inlen);
typedef int (* hash_finish_op_func_t)(struct hash_dev *hash, void *ctx,
        uint16_t algo, void *outbuf);

/**
 * @struct hash_interface
 * @brief Provides the interface into a HW hash driver
 *
 * @var hash_interface::start
 * start is a hash_start_op_func_t pointer to the hashing routine used
 * to start a new stream operation
 *
 * @var hash_interface::update
 * update is a hash_update_op_func_t pointer to the hashing routine used
 * to update the current stream operation with new data
 *
 * @var hash_interface::finish
 * finish is a hash_finish_op_func_t pointer to the hashing routine used
 * to finish the current stream operation and return a digest
 *
 * @var hash_interface::algomask
 * algomask stores a bitmask of algorithms supported by this hash driver
 */
struct hash_interface {
    hash_start_op_func_t start;
    hash_update_op_func_t update;
    hash_finish_op_func_t finish;
    uint32_t algomask;
};

struct hash_dev {
    struct os_dev dev;
    struct hash_interface interface;
};

/**
 * Hash a buffer using custom parameters; this should be used when
 * all data to be hashed is already available, since it does all
 * hashing in a single call.
 *
 * NOTE: if the hash needs to be constantly update with new data use
 * _start(), _update(), _finish().
 *
 * @param hash     OS device
 * @param algo     Algorithm to use (see HASH_ALGO_*)
 * @param inbuf    The input data to be encrypted
 * @param inlen    Length of the input buffer
 * @param outbuf   Digest result
 *
 * @return 0 if succesfull; -1 otherwise
 */
int hash_custom_process(struct hash_dev *hash, uint16_t algo,
        const void *inbuf, uint32_t inlen, void *outbuf);

/**
 * Start a stream hash operation with custom parameters
 *
 * NOTE: call _update() with contents and _finish() to capture the
 * final digest.
 *
 * @param hash     OS device
 * @param ctx      A context struct for the chosen algo
 * @param algo     Algorithm to use (see HASH_ALGO_*)
 *
 * @return 0 if succesfull; -1 otherwise
 */
int hash_custom_start(struct hash_dev *hash, void *ctx, uint16_t algo);

/**
 * Update the current hash operation with new data.
 *
 * NOTE: _start() must have been called previously.
 *
 * @param hash     OS device
 * @param ctx      A context struct for the chosen algo
 * @param algo     Algorithm to use (see HASH_ALGO_*)
 * @param inbuf    The input data to be encrypted
 * @param inlen    Length of the input buffer
 *
 * @return 0 if succesfull; -1 otherwise
 */
int hash_custom_update(struct hash_dev *hash, void *ctx, uint16_t algo,
        const void *inbuf, uint32_t inlen);

/**
 * Finish a stream hash operation and return the final digest.
 *
 * @param hash     OS device
 * @param ctx      A context struct for the chosen algo
 * @param algo     Algorithm to use (see HASH_ALGO_*)
 * @param outbuf   Digest result
 *
 * @return 0 if succesfull; -1 otherwise
 */
int hash_custom_finish(struct hash_dev *hash, void *ctx, uint16_t algo,
        void *outbuf);

/*
 * Query Hash HW capabilities
 *
 * @param hash     OS device
 * @param algo     Hash algorithm to check for (HASH_ALGO_*)
 *
 * @return true if the device has support, false otherwise.
 */
bool hash_has_support(struct hash_dev *hash, uint16_t algo);

/*
 * Helpers
 */

/**
 * Generate SHA256 digest of input data buffer; this should be used
 * when all data to be hashed is already available, since it does all
 * hashing in a single call.
 *
 * @param hash     OS device
 * @param inbuf    Input buffer
 * @param inlen    Length of the input buffer
 * @param outbuf   Output digest
 *
 * @return 0 if successfull, -1 on error
 */
int hash_sha256_process(struct hash_dev *hash, const void *inbuf,
        uint32_t inlen, void *outbuf);

/*
 * Start a stream sha256 operation.
 *
 * @param hash     OS device
 * @param ctx      A hash_sha256_context struct
 * @param algo     Algorithm to use (see HASH_ALGO_*)
 *
 * @return 0 if successfull, -1 on error
 */
int hash_sha256_start(struct hash_sha256_context *ctx, struct hash_dev *hash);

/*
 * Update the sha256 operation with new data.
 *
 * @param ctx      A hash_sha256_context struct
 * @param inbuf    The input data to be encrypted
 * @param inlen    Length of the input buffer
 *
 * @return 0 if successfull, -1 on error
 */
int hash_sha256_update(struct hash_sha256_context *ctx, const void *inbuf,
        uint32_t inlen);

/*
 * Finish the sha256 operation and return the final digest.
 *
 * @param ctx      A hash_sha256_context struct
 * @param outbuf   Resulting digest of the sha256
 *
 * @return 0 if successfull, -1 on error
 */
int hash_sha256_finish(struct hash_sha256_context *ctx, void *outbuf);

#ifdef __cplusplus
}
#endif

#endif /* __HASH_H__ */
