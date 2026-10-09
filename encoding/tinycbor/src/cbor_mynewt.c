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
#include <tinycbor/cbor.h>

#define CBOR_MAJOR_TYPE_BYTE_STRING (2 << 5)
#define CBOR_MAJOR_TYPE_TEXT_STRING (3 << 5)
#define CBOR_INDEFINITE_LENGTH      (31)

void
cbor_writer_encoder_init(struct cbor_writer *writer, CborEncoder *encoder)
{
    cbor_encoder_init_writer(encoder, writer->write, writer);
}

size_t
cbor_encode_bytes_written(const CborEncoder *encoder)
{
    const struct cbor_writer *writer;

    assert(encoder->flags & CborIteratorFlag_WriterFunction);

    /* Writer token is stored in encoder end pointer */
    writer = (const struct cbor_writer *)(const void *)encoder->end;

    return writer->bytes_written;
}

/* Counts item in encoder as done by TinyCBOR for encoded values */
static void
cbor_encoder_count_item(CborEncoder *encoder)
{
    if (encoder->remaining) {
        encoder->remaining--;
    }
}

static size_t
cbor_encode_header(uint8_t *buf, uint8_t major_type, uint64_t val)
{
    size_t len;
    size_t i;

    if (val < 24) {
        buf[0] = major_type | (uint8_t)val;
        return 1;
    }

    if (val <= UINT8_MAX) {
        buf[0] = major_type | 24;
        len = 1;
    } else if (val <= UINT16_MAX) {
        buf[0] = major_type | 25;
        len = 2;
    } else if (val <= UINT32_MAX) {
        buf[0] = major_type | 26;
        len = 4;
    } else {
        buf[0] = major_type | 27;
        len = 8;
    }

    for (i = 0; i < len; i++) {
        buf[len - i] = (uint8_t)(val >> (8 * i));
    }

    return len + 1;
}

CborError
cbor_encode_byte_iovec(CborEncoder *encoder, const struct cbor_iovec iov[], int iov_len)
{
    uint8_t hdr[9];
    uint64_t len = 0;
    CborError err;
    size_t hdr_len;
    int i;

    for (i = 0; i < iov_len; i++) {
        len += iov[i].iov_len;
    }

    hdr_len = cbor_encode_header(hdr, CBOR_MAJOR_TYPE_BYTE_STRING, len);

    err = cbor_encode_raw(encoder, hdr, hdr_len);
    for (i = 0; i < iov_len && !err; i++) {
        err = cbor_encode_raw(encoder, iov[i].iov_base, iov[i].iov_len);
    }

    if (err && err != CborErrorOutOfMemory) {
        return err;
    }

    cbor_encoder_count_item(encoder);

    return err;
}

/*
 * Creates indefinite length string container in the same way TinyCBOR creates
 * indefinite length array or map, so that cbor_encoder_close_container() can
 * be used to close it.
 */
static CborError
cbor_encoder_create_indef_string(CborEncoder *encoder,
                                 CborEncoder *string_encoder, uint8_t major_type)
{
    uint8_t hdr = major_type | CBOR_INDEFINITE_LENGTH;

    string_encoder->data = encoder->data;
    string_encoder->end = encoder->end;
    string_encoder->remaining = 0;
    string_encoder->flags = CborIteratorFlag_UnknownLength |
                            (encoder->flags & CborIteratorFlag_WriterFunction);

    cbor_encoder_count_item(encoder);

    return cbor_encode_raw(string_encoder, &hdr, 1);
}

CborError
cbor_encoder_create_indef_byte_string(CborEncoder *encoder, CborEncoder *string_encoder)
{
    return cbor_encoder_create_indef_string(encoder, string_encoder,
                                            CBOR_MAJOR_TYPE_BYTE_STRING);
}

CborError
cbor_encoder_create_indef_text_string(CborEncoder *encoder, CborEncoder *string_encoder)
{
    return cbor_encoder_create_indef_string(encoder, string_encoder,
                                            CBOR_MAJOR_TYPE_TEXT_STRING);
}
