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
#include <ctype.h>
#include <base62/base62.h>

/**
 * Function that returns symbol character from it's ordinal value.
 */
typedef uint8_t encode_symbol_t(unsigned int ordinal);

/**
 * Function to convert symbol from encoded text to it's ordinal number.
 */
typedef int decode_symbol_t(uint8_t digit_symbol);

static int
base_n_normalize(const uint8_t *data, size_t size, decode_symbol_t *decoder,
                 uint8_t *normalized)
{
    size_t i;
    int symbol_ordinal_number;

    for (i = 0; i < size; ++i) {
        symbol_ordinal_number = decoder(data[i]);
        if (symbol_ordinal_number < 0) {
            return -1;
        }
        normalized[i] = symbol_ordinal_number;
    }
    return 0;
}

static int
base_n_encode(const uint8_t *data, unsigned int data_size,
              unsigned int src_base, decode_symbol_t *symbol_decoder,
              unsigned int dst_base, encode_symbol_t *symbol_encoder,
              uint8_t *encoded, unsigned int *encoded_size)
{
    size_t i;
    const uint8_t *dividend;
    uint8_t *quotient;
    size_t dividend_size;
    size_t reminder;
    size_t accumulator;
    uint8_t *limit;
    uint8_t *result;
    size_t extra_bytes_needed = 0;

    if (encoded_size == NULL || data == NULL || encoded == NULL || *encoded_size < data_size) {
        return BASE62_INVALID_ARG;
    }

    if (base_n_normalize(data, data_size, symbol_decoder, encoded)) {
        return BASE62_DECODE_ERROR;
    }

    limit = encoded + *encoded_size;
    result = limit;
    dividend_size = data_size;

    while (dividend_size) {
        reminder = 0;
        dividend = encoded;
        quotient = encoded;
        for (i = 0; i < dividend_size; ++i) {
            accumulator = *dividend++ + (reminder * src_base);
            reminder = accumulator % dst_base;
            *quotient = (uint8_t)(accumulator / dst_base);
            if (*quotient || quotient != encoded) {
                quotient++;
            }
        }
        if (result > quotient) {
            *--result = symbol_encoder(reminder);
        } else {
            extra_bytes_needed++;
        }
        dividend_size = quotient - encoded;
    }
    *encoded_size = limit - result + extra_bytes_needed;

    if (0 == extra_bytes_needed) {
        memmove(encoded, result, *encoded_size);
    }

    return extra_bytes_needed ? BASE62_INSUFFICIENT_MEM : BASE62_DECODE_OK;
}

static uint8_t
encode_62(unsigned int ordinal)
{
    static const char base62_chars[62] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    return base62_chars[ordinal];
}

static int
decode_256(uint8_t digit_symbol)
{
    return digit_symbol;
}

static int
decode_62(uint8_t digit_symbol)
{
    if (isupper(digit_symbol)) {
        return digit_symbol - 'A' + 10;
    } else if (islower(digit_symbol)) {
        return digit_symbol - 'a' + 36;
    } else if (isdigit(digit_symbol)) {
        return digit_symbol - '0';
    }
    return -1;
}

static uint8_t
encode_256(unsigned int ordinal)
{
    return (uint8_t)ordinal;
}

int
base62_encode(const void *data, unsigned int input_size,
              char *encoded_text, unsigned int *output_size)
{
    return base_n_encode(data, input_size, 256, decode_256,
                         62, encode_62, (uint8_t *)encoded_text, output_size);
}

int
base62_decode(const char *encoded_text, unsigned int length,
              void *output_data, unsigned int *output_size)
{
    return base_n_encode((const uint8_t *)encoded_text, length, 62, decode_62,
                         256, encode_256, output_data, output_size);
}
