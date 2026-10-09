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
 * Mynewt extensions to TinyCBOR encoder API.
 */

#ifndef H_TINYCBOR_CBOR_MYNEWT_
#define H_TINYCBOR_CBOR_MYNEWT_

#include <stddef.h>
#include <cbor.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Base of Mynewt CBOR writers. Encoder using writer is initialized with
 * cbor_writer_encoder_init() or with cbor_encoder_init_writer() with writer
 * as token.
 */
struct cbor_writer {
    /** Write function */
    CborEncoderWriteFunction write;
    /** Number of bytes written so far */
    size_t bytes_written;
};

/**
 * Initializes encoder to use Mynewt CBOR writer.
 *
 * @param writer        Initialized writer.
 * @param encoder       Encoder to initialize.
 */
void cbor_writer_encoder_init(struct cbor_writer *writer, CborEncoder *encoder);

/**
 * Returns number of bytes written by encoder using Mynewt CBOR writer.
 *
 * @param encoder       Encoder (or container encoder) using Mynewt CBOR
 *                      writer.
 *
 * @return              Number of bytes written.
 */
size_t cbor_encode_bytes_written(const CborEncoder *encoder);

/** Data chunk used by cbor_encode_byte_iovec() */
struct cbor_iovec {
    void *iov_base;
    size_t iov_len;
};

/**
 * Encodes byte string with data provided as array of chunks. Encoded string
 * has definite length equal to sum of chunks lengths.
 *
 * @param encoder       Encoder.
 * @param iov           Array of data chunks.
 * @param iov_len       Number of data chunks.
 *
 * @return              CborNoError on success, other value on error.
 */
CborError cbor_encode_byte_iovec(CborEncoder *encoder,
                                 const struct cbor_iovec iov[], int iov_len);

/**
 * Creates indefinite length byte string. Chunks are encoded with
 * cbor_encode_byte_string() on string encoder and string is closed with
 * cbor_encoder_close_container().
 *
 * @param encoder       Encoder.
 * @param string_encoder    String encoder to initialize.
 *
 * @return              CborNoError on success, other value on error.
 */
CborError cbor_encoder_create_indef_byte_string(CborEncoder *encoder,
                                                CborEncoder *string_encoder);

/**
 * Creates indefinite length text string. Chunks are encoded with
 * cbor_encode_text_string() on string encoder and string is closed with
 * cbor_encoder_close_container().
 *
 * @param encoder       Encoder.
 * @param string_encoder    String encoder to initialize.
 *
 * @return              CborNoError on success, other value on error.
 */
CborError cbor_encoder_create_indef_text_string(CborEncoder *encoder,
                                                CborEncoder *string_encoder);

#ifdef __cplusplus
}
#endif

#endif
