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

/*
 * TinyCBOR configuration for Mynewt. This file is included by TinyCBOR cbor.h
 * if CBOR_EXTERNAL_CFG is defined.
 */

#ifndef H_TINYCBOR_CBOR_CFG_
#define H_TINYCBOR_CBOR_CFG_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "syscfg/syscfg.h"

#if !MYNEWT_VAL(TINYCBOR_FLOAT_SUPPORT)
#define CBOR_NO_FLOATING_POINT
#endif

#if !MYNEWT_VAL(TINYCBOR_HALF_FLOAT_SUPPORT)
#define CBOR_NO_HALF_FLOAT_TYPE
#endif

#define CBOR_PARSER_MAX_RECURSIONS MYNEWT_VAL(TINYCBOR_PARSER_MAX_RECURSIONS)

#if !MYNEWT_VAL(TINYCBOR_PRETTY_API)
#define CBOR_NO_PRETTY_API
#endif

#if !MYNEWT_VAL(TINYCBOR_VALIDATION_API)
#define CBOR_NO_VALIDATION_API
#endif

/*
 * Parser with external source (cbor_parser_init_reader()) is used with Mynewt
 * readers only (see tinycbor7/cbor_reader.h). TinyCBOR reader API keeps reading
 * position in reader, shared by all iterators, while Mynewt users expect
 * iterators to be independent (e.g. copy of iterator can be advanced without
 * affecting original). To achieve that TinyCBOR reader functions are replaced
 * with functions that use iterator token as offset into source data. Those
 * functions are only used for TinyCBOR internal helpers with iterator named
 * 'it'.
 */
struct CborValue;

bool cbor_reader_can_read_bytes(const struct CborValue *it, size_t len);
void *cbor_reader_read_bytes(const struct CborValue *it, void *dst,
                             size_t offset, size_t len);
int cbor_reader_transfer_string(struct CborValue *it, const void **ptr,
                                size_t offset, size_t len);

#define CBOR_PARSER_CAN_READ_BYTES_FUNCTION(token, len)                       \
    cbor_reader_can_read_bytes(it, len)
#define CBOR_PARSER_READ_BYTES_FUNCTION(token, dst, offset, len)              \
    cbor_reader_read_bytes(it, dst, offset, len)
#define CBOR_PARSER_ADVANCE_BYTES_FUNCTION(token, len)                        \
    ((token) = (void *)((uintptr_t)(token) + (len)))
#define CBOR_PARSER_TRANSFER_STRING_FUNCTION(token, ptr, offset, len)         \
    cbor_reader_transfer_string(it, ptr, offset, len)

#endif
