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

#ifndef H_TINYCBOR_CBOR_CNT_WRITER_
#define H_TINYCBOR_CBOR_CNT_WRITER_

#include <tinycbor7/cbor.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CBOR writer that only counts encoded bytes, can be used to check encoded
 * data length before encoding it to destination.
 */
struct cbor_cnt_writer {
    struct cbor_writer enc;
};

/**
 * Initializes counting writer. Encoder is initialized with
 * cbor_writer_encoder_init(&writer->enc, encoder).
 *
 * @param cb            Writer to initialize.
 */
void cbor_cnt_writer_init(struct cbor_cnt_writer *cb);

/** Write function for cbor_encoder_init_writer() */
CborError cbor_cnt_writer(void *token, const void *data, size_t len,
                          CborEncoderAppendType append);

#ifdef __cplusplus
}
#endif

#endif
