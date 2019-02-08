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

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "console/console.h"
#include "trng/trng.h"

static void
print_buffer(void *ptr, size_t size)
{
    while (size) {
        console_printf("%02x", *((uint8_t *) ptr));

        size--;
        ptr++;
    }

    console_printf("\n");
}

/*
 * Implement basic statistics based on descriptions in NIST 800-22r1a.
 *
 * See: 2.1 Frequency monobit and 2.2 Frequency test within a block.
 */

static bool
test_monobit_distribution(uint32_t len, int sum)
{
    float sobs;
    float pvalue;

    sobs = (float)abs(sum) / sqrtf((float)len);
    pvalue = erfc(sobs / sqrtf(2.0f));
    if (pvalue < 0.01f) {
        return false;
    }
    return true;
}

static uint16_t
bits_set(void *buf, size_t len)
{
    uint8_t *u8;
    int i;
    uint8_t bit;
    uint16_t total;

    u8 = (uint8_t *)buf;
    i = total = 0;
    while (i < len) {
        for (bit = 0; bit < 8; bit++) {
            total += (*u8 & (1 << bit)) ? 1 : 0;
        }
        u8++;
        i++;
    }
    return total;
}

/*
 * The values below represent maximums allowed when doing a regularized upper
 * incomplete gamma function for 4-bit and 8-bit blocks. Those values would
 * result in a p-value < 0.01. Since all math libraries found that provide
 * igamc are either GPL or LGPL, the expected values were calculated using
 * scipy (NOTE: scipy.special.gammaincc)
 */
#define NIBBLE_CHISQ_DIV_2 46.66f
#define BYTE_CHISQ_DIV_2   26.75f

static bool
test_nibble_distribution(uint8_t *buf, uint32_t bitlen)
{
    uint32_t N;
    int M;
    uint8_t val;
    int i;
    float chisq;

    M = 4;
    N = bitlen / M;
    chisq = 0.0f;
    for (i = 0; i < N/2; i++) {
        val = buf[i] & 0xf0;
        chisq += powf(((float)bits_set(&val, 1) / (float)M) - 0.5f, 2.0f);

        val = buf[i] & 0x0f;
        chisq += powf(((float)bits_set(&val, 1) / (float)M) - 0.5f, 2.0f);
    }
    chisq *= 4.0f * (float)M;

    if (chisq/2.0f >= NIBBLE_CHISQ_DIV_2) {
        return false;
    }

    return true;
}

static bool
test_byte_distribution(uint8_t *buf, uint32_t bitlen)
{
    uint32_t N;
    int M;
    uint8_t val;
    int i;
    float chisq;

    M = 8;
    N = bitlen / M;
    chisq = 0.0f;
    for (i = 0; i < N; i++) {
        val = buf[i];
        chisq += powf(((float)bits_set(&val, 1) / (float)M) - 0.5f, 2.0f);
    }
    chisq *= 4.0f * (float)M;

    if (chisq/2.0f >= BYTE_CHISQ_DIV_2) {
        return false;
    }

    return true;
}

int
main(void)
{
    struct trng_dev *trng;
    size_t size;
    uint32_t bitlen;
    uint32_t blksz;
    uint8_t i, j;
    uint8_t val8;
    uint32_t val32;
    uint8_t buf[32];
    uint16_t idx;
    uint16_t monobit_fails;
    uint16_t block4_fails;
    uint16_t block8_fails;
    bool failed;
    int Sn;

    sysinit();

    trng = (struct trng_dev *) os_dev_open(MYNEWT_VAL(APP_TRNG_DEV),
                                           OS_TIMEOUT_NEVER, NULL);
    assert(trng);

    os_time_delay(OS_TICKS_PER_SEC);

    size = trng_read(trng, buf, sizeof(buf));
    console_printf("trng - requested %d, available %d:\n", sizeof(buf), size);
    print_buffer(buf, size);

    for (i = 0; i < 8; i++) {
        console_printf("os_dev -> %08x\n", (unsigned) trng_get_u32(trng));
    }

    console_printf("Running statistics tests...\n");
    bitlen = 0;
    Sn = 0;
    idx = 0;
    monobit_fails = 0;
    block4_fails = 0;
    block8_fails = 0;
    while (true) {
        val32 = trng_get_u32(trng);
        for (i = 0; i < 32; i += 8) {
            val8 = (val32 >> i) & 0xff;
            for (j = 0; j < 8; j++) {
                if (val8 & (1 << j)) {
                    ++Sn;
                } else {
                    --Sn;
                }
            }
            buf[idx++] = val8;
        }

        bitlen += 4 * 8;

        if ((bitlen % 256) == 0) {
            blksz = bitlen / 256;
            failed = false;

            if (!test_monobit_distribution(bitlen, Sn)) {
                monobit_fails++;
                failed = true;
            }

            if (!test_nibble_distribution(buf, 256)) {
                block4_fails++;
                failed = true;
            }

            if (!test_byte_distribution(buf, 256)) {
                block8_fails++;
                failed = true;
            }

            if (failed) {
                console_printf("monobit: (%ld/%d) %f, block4: (%ld/%d) %f, block8: (%ld/%d) %f\n",
                        blksz, monobit_fails, ((float)blksz - monobit_fails) / blksz,
                        blksz, block4_fails, ((float)blksz - block4_fails) / blksz,
                        blksz, block8_fails, ((float)blksz - block8_fails) / blksz);
            }

            idx = 0;
        }
    }

    return 0;
}
