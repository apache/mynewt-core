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
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANYĄ
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stdint.h>
#include <bit_set/bit_set.h>
#include <os/endian.h>
#include <os/os.h>

void
bit_set_set_bits(void *bit_set, int start_bit, int bit_count, const void *src_bits)
{
    const uint8_t *src_bytes = (uint8_t *)src_bits;
    uint32_t bits = *src_bytes++;
    int start_byte = start_bit >> 3;

    uint8_t *bytes = start_byte + (uint8_t *)bit_set;
    start_bit -= start_byte << 3;
    int src_bit_offset = start_bit;

    while (bit_count > 0) {
        int n = min(bit_count, 8);
        uint8_t mask = ((1 << n) - 1) << start_bit;
        uint8_t b = (*bytes & ~mask) | ((bits << start_bit) & mask);
        *bytes++ = b;
        bit_count -= 8 - start_bit;
        bits >>= 8 - start_bit;
        start_bit = 0;
        bits |= (*src_bytes++) << src_bit_offset;
    }
}

void
bit_set_get_bits(const void *bit_set, int start_bit, int bit_count, void *dst_bits)
{
    uint8_t *dst_bytes = (uint8_t *)dst_bits;
    int start_byte = start_bit >> 3;
    const uint8_t *bytes = start_byte + (const uint8_t *)bit_set;
    start_bit -= start_byte << 3;
    uint32_t bits = 0;
    uint32_t dst_bit_offset = 0;
    int dst_bit_count = 0;

    while (dst_bit_count > 0 || bit_count > 0) {
        if (bit_count) {
            int n = min(bit_count, 8 - start_bit);
            bits |= ((*bytes++) >> start_bit) << dst_bit_offset;
            if (start_bit) {
                dst_bit_offset = 8 - start_bit;
                start_bit = 0;
            }
            dst_bit_count += n;
            bit_count -= n;
        }
        if (bit_count <= 0 || dst_bit_count >= 8) {
            if (dst_bit_count < 8) {
                uint8_t mask = (1 << dst_bit_count) - 1;
                bits &= (1 << dst_bit_count) - 1;
                *dst_bytes = (*dst_bytes & ~mask) | (bits & mask);
            } else {
                *dst_bytes++ = (uint8_t)bits;
            }
            dst_bit_count -= 8;
            bits >>= 8;
        }
    }
}

uint32_t
bit_set_get_bit(const void *bit_set, int bit_num)
{
    uint8_t b;

    bit_set_get_bits(bit_set, bit_num, 1, &b);

    return b;
}

void
bit_set_set_bit(void *bit_set, int bit_num, uint32_t val)
{
    uint8_t b = val & 1;

    bit_set_set_bits(bit_set, bit_num, 1, &b);
}

uint32_t
bit_set_flip_bit(void *bit_set, int bit_num)
{
    uint8_t b;

    bit_set_get_bits(bit_set, bit_num, 1, &b);
    b ^= 1;
    bit_set_set_bits(bit_set, bit_num, 1, &b);

    return b;
}
