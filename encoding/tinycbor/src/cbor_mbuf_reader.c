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
#include <tinycbor/cbor_mbuf_reader.h>
#include "cbor_reader_priv.h"

static size_t
cbor_mbuf_reader_len(const struct cbor_mbuf_reader *cb)
{
    return OS_MBUF_PKTLEN(cb->m) - cb->init_off;
}

static bool
cbor_mbuf_reader_can_read(const struct cbor_reader *r, size_t offset, size_t len)
{
    const struct cbor_mbuf_reader *cb = (const struct cbor_mbuf_reader *)r;
    size_t avail = cbor_mbuf_reader_len(cb);

    return offset <= avail && len <= avail - offset;
}

static void *
cbor_mbuf_reader_read(const struct cbor_reader *r, void *dst, size_t offset, size_t len)
{
    const struct cbor_mbuf_reader *cb = (const struct cbor_mbuf_reader *)r;

    if (!cbor_mbuf_reader_can_read(r, offset, len)) {
        return NULL;
    }

    if (os_mbuf_copydata(cb->m, cb->init_off + offset, len, dst)) {
        return NULL;
    }

    return dst;
}

static const void *
cbor_mbuf_reader_get_ptr(const struct cbor_reader *r, size_t offset, size_t len)
{
    const struct cbor_mbuf_reader *cb = (const struct cbor_mbuf_reader *)r;
    struct os_mbuf *om;
    uint16_t om_off;

    if (!cbor_mbuf_reader_can_read(r, offset, len)) {
        return NULL;
    }

    om = os_mbuf_off(cb->m, cb->init_off + offset, &om_off);
    if (!om || om->om_len - om_off < len) {
        return NULL;
    }

    return om->om_data + om_off;
}

static const struct cbor_reader_ops cbor_mbuf_reader_ops = {
    .can_read = cbor_mbuf_reader_can_read,
    .read = cbor_mbuf_reader_read,
    .get_ptr = cbor_mbuf_reader_get_ptr,
};

void
cbor_mbuf_reader_init(struct cbor_mbuf_reader *cb, struct os_mbuf *m, int initial_offset)
{
    assert(OS_MBUF_IS_PKTHDR(m));

    memset(cb, 0, sizeof(*cb));
    cb->r.ops = &cbor_mbuf_reader_ops;
    cb->m = m;
    cb->init_off = initial_offset;
}

static CborError
cbor_mbuf_value_advance_nested(CborValue *it, int nesting)
{
    CborValue recursed;
    CborError err;
    size_t len;

    if (cbor_value_is_byte_string(it) || cbor_value_is_text_string(it)) {
        len = SIZE_MAX;
        return cbor_reader_copy_string(it, NULL, &len, it);
    }

    if (!cbor_value_is_container(it)) {
        return cbor_value_advance(it);
    }

    if (nesting == 0) {
        return CborErrorNestingTooDeep;
    }

    err = cbor_value_enter_container(it, &recursed);
    if (err) {
        return err;
    }

    while (!cbor_value_at_end(&recursed)) {
        err = cbor_mbuf_value_advance_nested(&recursed, nesting - 1);
        if (err) {
            return err;
        }
    }

    return cbor_value_leave_container(it, &recursed);
}

CborError
cbor_mbuf_value_advance(CborValue *it)
{
    if (!cbor_reader_is_used(it)) {
        return cbor_value_advance(it);
    }

    if (!it->remaining) {
        return CborErrorAdvancePastEOF;
    }

    return cbor_mbuf_value_advance_nested(
        it, MYNEWT_VAL(TINYCBOR_PARSER_MAX_RECURSIONS));
}

CborError
cbor_mbuf_value_copy_byte_string(const CborValue *value, uint8_t *buffer,
                                 size_t *buflen, CborValue *next)
{
    if (!cbor_reader_is_used(value)) {
        return cbor_value_copy_byte_string(value, buffer, buflen, next);
    }

    if (!cbor_value_is_byte_string(value)) {
        return CborErrorIllegalType;
    }

    return cbor_reader_copy_string(value, buffer, buflen, next);
}

CborError
cbor_mbuf_value_copy_text_string(const CborValue *value, char *buffer,
                                 size_t *buflen, CborValue *next)
{
    if (!cbor_reader_is_used(value)) {
        return cbor_value_copy_text_string(value, buffer, buflen, next);
    }

    if (!cbor_value_is_text_string(value)) {
        return CborErrorIllegalType;
    }

    return cbor_reader_copy_string(value, buffer, buflen, next);
}

CborError
cbor_mbuf_value_calculate_string_length(const CborValue *value, size_t *len)
{
    if (!cbor_reader_is_used(value)) {
        return cbor_value_calculate_string_length(value, len);
    }

    *len = SIZE_MAX;
    return cbor_reader_copy_string(value, NULL, len, NULL);
}
