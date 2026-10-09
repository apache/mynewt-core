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

#ifndef H_TINYCBOR_CBOR_MBUF_READER_
#define H_TINYCBOR_CBOR_MBUF_READER_

#include "os/mynewt.h"
#include <tinycbor7/cbor.h>
#include <tinycbor7/cbor_reader.h>

#ifdef __cplusplus
extern "C" {
#endif

/** CBOR reader for data in mbuf chain */
struct cbor_mbuf_reader {
    struct cbor_reader r;
    /** Offset of CBOR data in mbuf chain */
    int init_off;
    struct os_mbuf *m;
};

/**
 * Initializes mbuf reader. Parser is initialized with
 * cbor_reader_parser_init(&reader->r, ...).
 *
 * @param cb            Reader to initialize.
 * @param m             Packet header mbuf containing CBOR data.
 * @param initial_offset    Offset of CBOR data in mbuf chain.
 */
void cbor_mbuf_reader_init(struct cbor_mbuf_reader *cb, struct os_mbuf *m,
                           int initial_offset);

/**
 * Advances iterator to next element, see cbor_value_advance(). Works for
 * strings that are not contiguous in mbuf chain (cbor_value_advance() needs
 * strings to be contiguous to skip them).
 *
 * Can be used with values from any parser, for parsers not using Mynewt
 * CBOR reader TinyCBOR function is used.
 */
CborError cbor_mbuf_value_advance(CborValue *it);

/**
 * Copies byte string pointed to by value to buffer, see
 * cbor_value_copy_byte_string(). Works for strings that are not contiguous in
 * mbuf chain.
 *
 * Can be used with values from any parser, for parsers not using Mynewt
 * CBOR reader TinyCBOR string function is used.
 */
CborError cbor_mbuf_value_copy_byte_string(const CborValue *value, uint8_t *buffer,
                                           size_t *buflen, CborValue *next);

/**
 * Copies text string pointed to by value to buffer, see
 * cbor_value_copy_text_string(). Works for strings that are not contiguous in
 * mbuf chain.
 *
 * Can be used with values from any parser, for parsers not using Mynewt
 * CBOR reader TinyCBOR string function is used.
 */
CborError cbor_mbuf_value_copy_text_string(const CborValue *value, char *buffer,
                                           size_t *buflen, CborValue *next);

/**
 * Calculates length of string pointed to by value, see
 * cbor_value_calculate_string_length(). Works for strings that are not
 * contiguous in mbuf chain.
 *
 * Can be used with values from any parser, for parsers not using Mynewt
 * CBOR reader TinyCBOR string function is used.
 */
CborError cbor_mbuf_value_calculate_string_length(const CborValue *value, size_t *len);

#ifdef __cplusplus
}
#endif

#endif
