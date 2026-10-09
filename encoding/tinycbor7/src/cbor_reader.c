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

#include <assert.h>
#include <tinycbor7/cbor.h>
#include <tinycbor7/cbor_reader.h>
#include "cbor_reader_priv.h"

static struct cbor_reader *
cbor_reader_get(const CborValue *it)
{
    /* Mynewt reader is passed to TinyCBOR as parser operations */
    return (struct cbor_reader *)(uintptr_t)it->parser->source.ops;
}

static size_t
cbor_reader_pos(const CborValue *it)
{
    return (size_t)(uintptr_t)it->source.token;
}

bool
cbor_reader_can_read_bytes(const struct CborValue *it, size_t len)
{
    const struct cbor_reader *reader = cbor_reader_get(it);

    return reader->ops->can_read(reader, cbor_reader_pos(it), len);
}

void *
cbor_reader_read_bytes(const struct CborValue *it, void *dst, size_t offset, size_t len)
{
    const struct cbor_reader *reader = cbor_reader_get(it);

    return reader->ops->read(reader, dst, cbor_reader_pos(it) + offset, len);
}

int
cbor_reader_transfer_string(struct CborValue *it, const void **ptr,
                            size_t offset, size_t len)
{
    struct cbor_reader *reader = cbor_reader_get(it);
    const void *data = NULL;
    size_t off;

    off = cbor_reader_pos(it) + offset;
    if (len > SIZE_MAX - off || !reader->ops->can_read(reader, off, len)) {
        return CborErrorUnexpectedEOF;
    }

    if (reader->xfer_active) {
        /* String is copied by string helper, see cbor_reader_copy_string() */
        if (reader->xfer_buf && len <= reader->xfer_buf_len) {
            data = reader->ops->read(reader, reader->xfer_buf, off, len);
            if (!data) {
                return CborErrorIO;
            }
            reader->xfer_buf += len;
            reader->xfer_buf_len -= len;
        } else {
            /* Doesn't fit, only calculate length from now on */
            reader->xfer_buf = NULL;
        }
    } else {
        if (reader->ops->get_ptr) {
            data = reader->ops->get_ptr(reader, off, len);
        }
        if (!data && reader->scratch && len <= reader->scratch_size) {
            data = reader->ops->read(reader, reader->scratch, off, len);
        }
        if (!data) {
            return CborErrorIO;
        }
    }

    *ptr = data;
    it->source.token = (void *)(uintptr_t)(off + len);

    return CborNoError;
}

CborError
cbor_reader_parser_init(struct cbor_reader *reader, CborParser *parser, CborValue *it)
{
    reader->xfer_active = false;

    return cbor_parser_init_reader(
        (const struct CborParserOperations *)(const void *)reader, parser, it, NULL);
}

static CborError
cbor_reader_get_chunk(CborValue *it, size_t *len)
{
    const void *ptr;

    if (cbor_value_is_text_string(it)) {
        return cbor_value_get_text_string_chunk(it, (const char **)&ptr, len, it);
    }

    return cbor_value_get_byte_string_chunk(it, (const uint8_t **)&ptr, len, it);
}

CborError
cbor_reader_copy_string(const CborValue *value, void *buffer, size_t *buflen,
                        CborValue *next)
{
    struct cbor_reader *reader;
    size_t total = 0;
    size_t chunk_len;
    CborValue it;
    CborError err;
    bool fits;

    if (!cbor_value_is_byte_string(value) && !cbor_value_is_text_string(value)) {
        return CborErrorIllegalType;
    }

    reader = cbor_reader_get(value);
    it = *value;

    err = cbor_value_begin_string_iteration(&it);
    if (err) {
        return err;
    }

    reader->xfer_buf = buffer;
    reader->xfer_buf_len = buffer ? *buflen : 0;
    reader->xfer_active = true;

    while (1) {
        err = cbor_reader_get_chunk(&it, &chunk_len);
        if (err == CborErrorNoMoreStringChunks) {
            err = CborNoError;
            break;
        }
        if (err) {
            break;
        }

        if (chunk_len > SIZE_MAX - total) {
            err = CborErrorDataTooLarge;
            break;
        }
        total += chunk_len;
    }

    fits = buffer && reader->xfer_buf;
    reader->xfer_active = false;
    reader->xfer_buf = NULL;

    if (err) {
        return err;
    }

    /* NUL terminate if there is space, as TinyCBOR does */
    if (fits && *buflen > total) {
        ((uint8_t *)buffer)[total] = '\0';
    }

    err = cbor_value_finish_string_iteration(&it);
    if (err) {
        return err;
    }

    *buflen = total;
    if (next) {
        *next = it;
    }

    if (buffer && !fits) {
        return CborErrorOutOfMemory;
    }

    return CborNoError;
}
