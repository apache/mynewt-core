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

#include <stdint.h>
#include "hash_k64f/hash_k64f.h"

/*
 * CAU interface
 */

#define CAU_CMD1_SHIFT 22
#define CAU_CMD2_SHIFT 11
#define CAU_CMD3_SHIFT 0

#define CAU_CMD1(x) (0x80000000 | (x) << CAU_CMD1_SHIFT)
#define CAU_CMD2(x) (0x00100000 | (x) << CAU_CMD2_SHIFT)
#define CAU_CMD3(x) (0x00000200 | (x) << CAU_CMD3_SHIFT)

#define CA7    9
#define CA8    10

#define ADRA   0x50      /* CAA = CAA + CAx */
#define MVAR   0x90      /* CAx = CAA */
#define HASH   0x120
#define SHS2   0x150

#define HF2C   6         /* Ch */
#define HF2M   7         /* Maj */
#define HF2S   8         /* S0 */
#define HF2T   9         /* S1 */
#define HF2U   10        /* s0 */
#define HF2V   11        /* s1 */

/*
 * First 32 bits of the fractional parts of the square roots of the
 * first 8 primes (2..19)
 */
static uint32_t sha256_initial_h[SHA256_DIGEST_LEN] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

/*
 * First 32 bits of the fractional parts of the cube roots of the
 * first 64 primes (2..311)
 */
static uint32_t sha256_k[] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

int
cau_sha256_initialize_output(const unsigned int *output)
{
    memcpy((unsigned int *)output, sha256_initial_h, SHA256_DIGEST_LEN);
    return 0;
}

void
cau_sha256_hash_n(const unsigned char *msg_data, const int num_blks,
        unsigned int *sha256_state)
{
    int i;
    uint32_t *b32p;
    /* NOTE: avoid stack overrun when stack size is small */
    static uint32_t w[64];
    int blk;

    b32p = (uint32_t *)msg_data;

    for (i = 0; i < 8; i++) {
        CAU->LDR_CA[i] = sha256_state[i];
    }

    blk = num_blks;
    while (blk-- > 0) {
        for (i = 0; i < 16; i++) {
            CAU->LDR_CAA = w[i] = os_bswap_32(b32p[i]);
            CAU->DIRECT[0] = CAU_CMD1(ADRA + CA7) + CAU_CMD2(HASH + HF2T) +
                CAU_CMD3(HASH + HF2C);
            CAU->ADR_CAA = sha256_k[i];
            CAU->DIRECT[0] = CAU_CMD1(MVAR + CA8) + CAU_CMD2(HASH + HF2S) +
                CAU_CMD3(HASH + HF2M);
            CAU->DIRECT[0] = CAU_CMD1(SHS2);
        }

        for (i = 16; i < 64; i++) {
            CAU->LDR_CAA = w[i - 16];
            CAU->LDR_CA[8] = w[i - 15];
            CAU->DIRECT[0] = CAU_CMD1(HASH + HF2U);
            CAU->ADR_CAA = w[i - 7];
            CAU->LDR_CA[8] = w[i - 2];
            CAU->DIRECT[0] = CAU_CMD1(HASH + HF2V);
            w[i] = CAU->STR_CAA;
            CAU->DIRECT[0] = CAU_CMD1(ADRA + CA7) + CAU_CMD2(HASH + HF2T) +
                CAU_CMD3(HASH + HF2C);
            CAU->ADR_CAA = sha256_k[i];
            CAU->DIRECT[0] = CAU_CMD1(MVAR + CA8) + CAU_CMD2(HASH + HF2S) +
                CAU_CMD3(HASH + HF2M);
            CAU->DIRECT[0] = CAU_CMD1(SHS2);
        }

        for (i = 0; i < 8; i++) {
            CAU->ADR_CA[i] = sha256_state[i];
            sha256_state[i] = CAU->STR_CA[i];
        }
    }
}

void
cau_sha256_update(const unsigned char *msg_data, const int num_blks,
        unsigned int *sha256_state)
{
    memcpy(sha256_state, sha256_initial_h, SHA256_DIGEST_LEN);
    cau_sha256_hash_n(msg_data, num_blks, sha256_state);
}

void
cau_sha256_hash(const unsigned char *msg_data, unsigned int *sha256_state)
{
    cau_sha256_hash_n(msg_data, 1, sha256_state);
}
