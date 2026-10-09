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

#include <tinycbor7/cbor_mbuf_writer.h>

void
cbor_mbuf_writer_init(struct cbor_mbuf_writer *cb, struct os_mbuf *m)
{
    cb->enc.write = cbor_mbuf_writer;
    cb->enc.bytes_written = 0;
    cb->m = m;
}

CborError
cbor_mbuf_writer(void *token, const void *data, size_t len, CborEncoderAppendType append)
{
    struct cbor_mbuf_writer *cb = token;

    if (len > UINT16_MAX || os_mbuf_append(cb->m, data, len)) {
        return CborErrorOutOfMemory;
    }

    cb->enc.bytes_written += len;

    return CborNoError;
}
