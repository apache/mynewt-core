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

#ifndef H_TINYCBOR_CBOR_MBUF_WRITER_
#define H_TINYCBOR_CBOR_MBUF_WRITER_

#include "os/mynewt.h"
#include <tinycbor/cbor.h>

#ifdef __cplusplus
extern "C" {
#endif

/** CBOR writer appending encoded data to mbuf chain */
struct cbor_mbuf_writer {
    struct cbor_writer enc;
    struct os_mbuf *m;
};

/**
 * Initializes mbuf writer. Encoder is initialized with
 * cbor_writer_encoder_init(&writer->enc, encoder).
 *
 * @param cb            Writer to initialize.
 * @param m             Mbuf to append encoded data to.
 */
void cbor_mbuf_writer_init(struct cbor_mbuf_writer *cb, struct os_mbuf *m);

/** Write function for cbor_encoder_init_writer() */
CborError cbor_mbuf_writer(void *token, const void *data, size_t len,
                           CborEncoderAppendType append);

#ifdef __cplusplus
}
#endif

#endif
