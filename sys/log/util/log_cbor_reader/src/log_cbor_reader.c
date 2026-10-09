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
#include "log_cbor_reader/log_cbor_reader.h"

static bool
log_cbor_reader_can_read(const struct cbor_reader *r, size_t offset, size_t len)
{
    const struct log_cbor_reader *cbr = (const struct log_cbor_reader *)r;

    return offset <= cbr->len && len <= cbr->len - offset;
}

static void *
log_cbor_reader_read(const struct cbor_reader *r, void *dst, size_t offset, size_t len)
{
    const struct log_cbor_reader *cbr = (const struct log_cbor_reader *)r;
    int rc;

    if (!log_cbor_reader_can_read(r, offset, len)) {
        return NULL;
    }

    rc = log_read_body(cbr->log, cbr->dptr, dst, offset, len);
    if (rc != len) {
        return NULL;
    }

    return dst;
}

static const struct cbor_reader_ops log_cbor_reader_ops = {
    .can_read = log_cbor_reader_can_read,
    .read = log_cbor_reader_read,
};

void
log_cbor_reader_init(struct log_cbor_reader *cbr, struct log *log,
                     const void *dptr, uint16_t len)
{
    memset(&cbr->r, 0, sizeof(cbr->r));
    cbr->r.ops = &log_cbor_reader_ops;
    cbr->r.scratch = cbr->str_buf;
    cbr->r.scratch_size = sizeof(cbr->str_buf);
    cbr->log = log;
    cbr->dptr = dptr;
    cbr->len = len;
}
