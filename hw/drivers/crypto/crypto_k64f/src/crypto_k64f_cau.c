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
#include "crypto_k64f/crypto_k64f.h"

/*
 * CAU interface
 */

#define CAU_CMD1_SHIFT 22
#define CAU_CMD2_SHIFT 11
#define CAU_CMD3_SHIFT 0

#define CAU_CMD1(x) (0x80000000 | (x) << CAU_CMD1_SHIFT)
#define CAU_CMD2(x) (0x00100000 | (x) << CAU_CMD2_SHIFT)
#define CAU_CMD3(x) (0x00000200 | (x) << CAU_CMD3_SHIFT)

#define CAA    1
#define CA0    2
#define CA1    3
#define CA2    4
#define CA3    5
#define AESS   0xa0
#define AESIS  0xb0
#define AESR   0xe0
#define AESIR  0xf0

void
cau_aes_encrypt(const unsigned char *in, const unsigned char *key_sch, int nr,
        unsigned char *out)
{
    uint32_t *u32p;
    uint32_t *ks32p;
    uint8_t i, n;

    ks32p = (uint32_t *)key_sch;

    u32p = (uint32_t *)in;
    for (i = 0; i < 4; i++) {
        CAU->LDR_CA[i] = os_bswap_32(u32p[i]) ^ *ks32p++;
    }

    for (n = 0; n < nr - 1; n++) {
        CAU->DIRECT[0] = CAU_CMD1(AESS + CA0) + CAU_CMD2(AESS + CA1) +
            CAU_CMD3(AESS + CA2);
        CAU->DIRECT[1] = CAU_CMD1(AESS + CA3) + CAU_CMD2(AESR);
        for (i = 0; i < 4; i++) {
            CAU->AESC_CA[i] = *ks32p++;
        }
    }

    CAU->DIRECT[0] = CAU_CMD1(AESS + CA0) + CAU_CMD2(AESS + CA1) +
        CAU_CMD3(AESS + CA2);
    CAU->DIRECT[1] = CAU_CMD1(AESS + CA3) + CAU_CMD2(AESR);

    u32p = (uint32_t *)out;
    for (i = 0; i < 4; i++) {
        *u32p++ = os_bswap_32(*ks32p ^ CAU->STR_CA[i]);
        ks32p++;
    }
}

void
cau_aes_decrypt(const unsigned char *in, const unsigned char *key_sch,
        const int nr, unsigned char *out)
{
    uint32_t *u32p;
    uint32_t *ks32p;
    uint8_t i, n;

    ks32p = (uint32_t *)key_sch + (4 * nr);

    u32p = (uint32_t *)in;
    for (i = 0; i < 4; i++) {
        CAU->LDR_CA[i] = os_bswap_32(u32p[i]) ^ ks32p[i];
    }
    ks32p -= 4;

    for (n = 0; n < nr - 1; n++) {
        CAU->DIRECT[0] = CAU_CMD1(AESIR) + CAU_CMD2(AESIS + CA3) +
            CAU_CMD3(AESIS + CA2);
        CAU->DIRECT[1] = CAU_CMD1(AESIS + CA1) + CAU_CMD2(AESIS + CA0);
        for (i = 0; i < 4; i++) {
            CAU->AESIC_CA[i] = ks32p[i];
        }
        ks32p -= 4;
    }

    CAU->DIRECT[0] = CAU_CMD1(AESIR) + CAU_CMD2(AESIS + CA3) +
        CAU_CMD3(AESIS + CA2);
    CAU->DIRECT[1] = CAU_CMD1(AESIS + CA1) + CAU_CMD2(AESIS + CA0);

    u32p = (uint32_t *)out;
    for (i = 0; i < 4; i++) {
        *u32p++ = os_bswap_32(ks32p[i] ^ CAU->STR_CA[i]);
    }
}

/* XXX right now only supports AES-128 */
void
cau_aes_set_key(const unsigned char *key, const int key_size,
        unsigned char *key_sch)
{
    const uint32_t *k32p;
    uint32_t *ks32p;
    uint8_t n;

    /* round constants */
    uint32_t rcon[] = {
        0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000,
        0x20000000, 0x40000000, 0x80000000, 0x1b000000, 0x36000000,
    };

    k32p = (const uint32_t *)key;
    ks32p = (uint32_t *)key_sch;

    *ks32p++ = os_bswap_32(k32p[0]);
    *ks32p++ = os_bswap_32(k32p[1]);
    *ks32p++ = os_bswap_32(k32p[2]);
    *ks32p   = os_bswap_32(k32p[3]);

    for (n = 0; n < 10; n++) {
        CAU->LDR_CAA = (*ks32p << 8) | (*ks32p >> 24);
        CAU->DIRECT[0] = CAU_CMD1(AESS + CAA);
        ks32p++;
        *ks32p = *(ks32p - 4) ^ CAU->STR_CAA ^ rcon[n];
        ks32p++;
        *ks32p = *(ks32p - 4) ^ *(ks32p - 1);
        ks32p++;
        *ks32p = *(ks32p - 4) ^ *(ks32p - 1);
        ks32p++;
        *ks32p = *(ks32p - 4) ^ *(ks32p - 1);
    }
}
