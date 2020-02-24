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
#ifndef __UTIL_BASE62_H
#define __UTIL_BASE62_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BASE62_DECODE_OK         0
#define BASE62_DECODE_ERROR     -1
#define BASE62_INVALID_ARG      -2
#define BASE62_INSUFFICIENT_MEM -3

/**
 * @brief Function transforms any data to base 62 encoded text.
 *
 * @note null terminator is not added
 *
 * @param data          data to encode
 * @param input_size    number of bytes to encode
 * @param encoded_text  pointer to output buffer with encoded text, must be at least
 *                      of the same size as input_size
 * @param output_size   maximum output buffer length
 *                      on exit this variable holds encoded text size.
 *
 * @return              0 when output buffer was filled
 *                      BASE62_INSUFFICIENT_MEM if output buffer was too small
 *                      BASE62_INVALID_ARG if one of the pointers is NULL
 */
int base62_encode(const void *data, unsigned int input_size,
                  char *encoded_text, unsigned int *output_size);

/**
 * @brief Decode base62 encoded text
 *
 * @param encoded_text  base62 encoded text
 * @param text_length   length of text to decode
 * @param output_data   pointer to output buffer for decoded data
 * @param output_size   pointer to variable that holds output buffer size on input
 *                      and actual decoded data length on output
 *
 * @return  BASE62_DECODE_OK on success
 *          BASE62_INVALID_ARG when output_size is NULL
 *          BASE62_INSUFFICIENT_MEM if output size is insufficient for decoded text
 *          BASE62_DECODE_ERROR if input text is base62 malformed
 *
 */
int base62_decode(const char *encoded_text, unsigned int text_length,
                  void *output_data, unsigned int *output_size);

#ifdef __cplusplus
}
#endif

#endif /* __UTIL_BASE62_H__ */
