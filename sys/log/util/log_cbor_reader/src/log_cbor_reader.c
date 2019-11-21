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

#include "os/mynewt.h"
#include "log/log.h"
#include "tinycbor/cbor.h"
#include "log_cbor_reader/log_cbor_reader.h"
#include "tinycbor/compilersupport_p.h"

static uint8_t
log_cbor_reader_get8(struct cbor_decoder_reader *d, int offset)
{
    struct log_cbor_reader *cbr = (struct log_cbor_reader *)d;
    uint8_t val = 0;

    (void)log_read_body(cbr->log, cbr->dptr, &val, offset, sizeof(val));

    return val;
}

static uint16_t
log_cbor_reader_get16(struct cbor_decoder_reader *d, int offset)
{
    struct log_cbor_reader *cbr = (struct log_cbor_reader *)d;
    uint16_t val = 0;

    (void)log_read_body(cbr->log, cbr->dptr, &val, offset, sizeof(val));

    return cbor_ntohs(val);
}

static uint32_t
log_cbor_reader_get32(struct cbor_decoder_reader *d, int offset)
{
    struct log_cbor_reader *cbr = (struct log_cbor_reader *)d;
    uint32_t val = 0;

    (void)log_read_body(cbr->log, cbr->dptr, &val, offset, sizeof(val));

    return cbor_ntohl(val);
}

static uint64_t
log_cbor_reader_get64(struct cbor_decoder_reader *d, int offset)
{
    struct log_cbor_reader *cbr = (struct log_cbor_reader *)d;
    uint64_t val = 0;

    (void)log_read_body(cbr->log, cbr->dptr, &val, offset, sizeof(val));

    return cbor_ntohll(val);
}

static uintptr_t
log_cbor_reader_cmp(struct cbor_decoder_reader *d, char *dst,
                    int src_offset, size_t len)
{
    struct log_cbor_reader *cbr = (struct log_cbor_reader *)d;
    uint8_t buf[16];
    int chunk_len;
    int offset;
    int rc;

    offset = 0;

    while (offset < len) {
        chunk_len = min(len - offset, sizeof(buf));

        log_read_body(cbr->log, cbr->dptr, buf, src_offset + offset, chunk_len);

        rc = memcmp(&dst[offset], buf, chunk_len);
        if (rc) {
            return rc;
        }

        offset += chunk_len;
    }

    return 0;
}

static uintptr_t
log_cbor_reader_cpy(struct cbor_decoder_reader *d, char *dst,
                    int src_offset, size_t len)
{
    struct log_cbor_reader *cbr = (struct log_cbor_reader *)d;

    log_read_body(cbr->log, cbr->dptr, dst, src_offset, len);

    return (uintptr_t)dst;
}

void
log_cbor_reader_init(struct log_cbor_reader *cbr, struct log *log,
                     const void *dptr, uint16_t len)
{
    cbr->r.get8 = &log_cbor_reader_get8;
    cbr->r.get16 = &log_cbor_reader_get16;
    cbr->r.get32 = &log_cbor_reader_get32;
    cbr->r.get64 = &log_cbor_reader_get64;
    cbr->r.cmp = &log_cbor_reader_cmp;
    cbr->r.cpy = &log_cbor_reader_cpy;
    cbr->r.message_size = len;
    cbr->log = log;
    cbr->dptr = dptr;
}
