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

#ifndef _UTIL_BIT_SET_H_
#define _UTIL_BIT_SET_H_

#include <stdint.h>
#include <stdlib.h>
#include <os/endian.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set bit from other bit set
 *
 * @param bit_set - pointer to first byte of a bit set
 * @param start_bit - first bit to set
 * @param bit_count - number of bits to set
 * @param src_bits - source bits (starting from bit 0)
 */
void bit_set_set_bits(void *bit_set, int start_bit, int bit_count, const void *src_bits);

/**
 * Get bits from bit set
 *
 * @param bit_set - pointer to first byte of a bit set
 * @param start_bit - first bit to extract
 * @param bit_count - number of bits to extract
 * @param dst_bits - buffer for extracted bits (little endian buffer)
 */
void bit_set_get_bits(const void *bit_set, int start_bit, int bit_count, void *dst_bits);

/**
 * Get single bit value from bit set in little endian order
 *
 * @param bit_set - pointer to first byte of a bit set
 * @param bit_num - bit to get
 * @return value of bit_num bit
 */
uint32_t bit_set_get_bit(const void *bit_set, int bit_num);

/**
 * Set bit in bit set
 *
 * @param bit_set - pointer to first byte of a bit set
 * @param bit_num - bit to set value
 * @param val - 0 or 1 value to set
 */
void bit_set_set_bit(void *bit_set, int bit_num, uint32_t val);

/**
 * Set bit in bit set
 *
 * @param bit_set - pointer to first byte of a bit set
 * @param bit_num - bit to set value
 * @return bit value after flip
 */
uint32_t bit_set_flip_bit(void *bit_set, int bit_num);

/**
 * Reverse bytes in bit set
 * This can be useful if original buffer contains bytes in bit endian order
 *
 * @param bit_set - bit set to revers order
 * @param bit_count - number of bits in bit set
 */
static inline void
bit_set_reverse(void *bit_set, int bit_count)
{
    swap_in_place(bit_set, ((bit_count + 7) >> 3));
}

/**
 * Get 32 bit value from bit set starting from given bit
 * This function is wrapper to ease extraction of bit fields
 *
 * @param bit_set - pointer to first byte of a bit set
 * @param start_bit - first bit to extract
 * @param bit_count - number of bits to extract
 * @return 32 bit value in host order
 */
static inline uint32_t
bit_set_get_uint32(const void *bit_set, int start_bit, int bit_count)
{
    uint32_t val = 0;

    bit_set_get_bits(bit_set, start_bit, bit_count, &val);

    return le32toh(val);
}

/**
 * Set number of bit in bit set from host order integer value
 *
 * @param bit_set - pointer to first byte of a bit set
 * @param start_bit - first bit to set
 * @param bit_count - number of bits to set
 * @param val - host order val to set in bit field
 */
static inline void
bit_set_set_uint32(void *bit_set, int start_bit, int bit_count, uint32_t val)
{
    val = htole32(val);

    bit_set_set_bits(bit_set, start_bit, bit_count, &val);
}

#ifdef __cplusplus
}
#endif

#endif
