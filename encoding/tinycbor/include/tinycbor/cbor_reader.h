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
 * Mynewt CBOR readers.
 *
 * Mynewt readers allow parsing CBOR data that is not available as single
 * contiguous buffer (e.g. data in mbuf chain or in flash). Unlike TinyCBOR
 * reader API, reading position is kept in iterator so iterators are
 * independent from each other.
 *
 * TinyCBOR accesses strings by pointer, also when only skipping them, so
 * TinyCBOR functions that read or skip strings (e.g.
 * cbor_value_copy_text_string(), cbor_value_advance(),
 * cbor_value_map_find_value(), cbor_value_validate(),
 * cbor_value_to_pretty()) fail with CborErrorIO for strings that are not
 * contiguous in source data, unless reader has scratch buffer big enough to
 * hold such string. Use cbor_mbuf_value_advance(),
 * cbor_mbuf_value_copy_text_string() and related helpers to handle strings
 * regardless of source data layout.
 */

#ifndef H_TINYCBOR_CBOR_READER_
#define H_TINYCBOR_CBOR_READER_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <cbor.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cbor_reader;

/** Mynewt CBOR reader operations, offsets are relative to start of data */
struct cbor_reader_ops {
    /** Checks if len bytes are available at offset */
    bool (*can_read)(const struct cbor_reader *reader, size_t offset, size_t len);
    /** Copies len bytes at offset to dst, returns dst or NULL on failure */
    void *(*read)(const struct cbor_reader *reader, void *dst, size_t offset,
                  size_t len);
    /**
     * Returns pointer to len contiguous bytes at offset or NULL if data is not
     * contiguous (optional, may be NULL).
     */
    const void *(*get_ptr)(const struct cbor_reader *reader, size_t offset, size_t len);
};

/** Base of Mynewt CBOR readers */
struct cbor_reader {
    const struct cbor_reader_ops *ops;

    /**
     * Optional scratch buffer used to provide strings that are not
     * contiguous in source data to TinyCBOR string functions.
     */
    uint8_t *scratch;
    size_t scratch_size;

    /* Used internally by string helpers */
    uint8_t *xfer_buf;
    size_t xfer_buf_len;
    bool xfer_active;
};

/**
 * Initializes parser for data provided by Mynewt CBOR reader.
 *
 * @param reader        Initialized reader, must remain valid while parsing.
 * @param parser        Parser to initialize.
 * @param it            Iterator to first element.
 *
 * @return              CborNoError on success, other value on error.
 */
CborError cbor_reader_parser_init(struct cbor_reader *reader,
                                  CborParser *parser, CborValue *it);

#ifdef __cplusplus
}
#endif

#endif
