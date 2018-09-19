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
#include <cbmem/cbmem.h>
#include "log/log.h"

static int
log_cbmem_append(struct log *log, void *buf, int len)
{
    struct cbmem *cbmem;
    int rc;

    cbmem = (struct cbmem *) log->l_arg;

    rc = cbmem_append(cbmem, buf, len);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int
log_cbmem_append_body(struct log *log, const struct log_entry_hdr *hdr,
                      const void *body, int body_len)
{
    struct cbmem *cbmem;

    struct cbmem_scat_gath sg = {
        .entries = (struct cbmem_scat_gath_entry[2]) {
            {
                .flat_buf = hdr,
                .flat_len = sizeof *hdr,
            },
            {
                .flat_buf = body,
                .flat_len = body_len,
            },
        },
        .count = 2,
    };

    cbmem = (struct cbmem *) log->l_arg;

    return cbmem_append_scat_gath(cbmem, &sg);
}

static int
log_cbmem_append_mbuf(struct log *log, const struct os_mbuf *om)
{
    struct cbmem *cbmem;
    int rc;

    cbmem = (struct cbmem *) log->l_arg;

    rc = cbmem_append_mbuf(cbmem, om);
    if (rc != 0) {
        goto err;
    }

    return (0);

err:
    return (rc);
}

static int
log_cbmem_append_mbuf_body(struct log *log, const struct log_entry_hdr *hdr,
                           const struct os_mbuf *om)
{
    struct cbmem *cbmem;

    struct cbmem_scat_gath sg = {
        .entries = (struct cbmem_scat_gath_entry[2]) {
            {
                .flat_buf = hdr,
                .flat_len = sizeof *hdr,
            },
            {
                .om = om,
            },
        },
        .count = 2,
    };

    cbmem = (struct cbmem *) log->l_arg;

    return cbmem_append_scat_gath(cbmem, &sg);
}

static int
log_cbmem_read(struct log *log, void *dptr, void *buf, uint16_t offset,
        uint16_t len)
{
    struct cbmem *cbmem;
    struct cbmem_entry_hdr *hdr;
    int rc;

    cbmem = (struct cbmem *) log->l_arg;
    hdr = (struct cbmem_entry_hdr *) dptr;

    rc = cbmem_read(cbmem, hdr, buf, offset, len);

    return (rc);
}

static int
log_cbmem_read_mbuf(struct log *log, void *dptr, struct os_mbuf *om,
                    uint16_t offset, uint16_t len)
{
    struct cbmem *cbmem;
    struct cbmem_entry_hdr *hdr;
    int rc;

    cbmem = (struct cbmem *) log->l_arg;
    hdr = (struct cbmem_entry_hdr *) dptr;

    rc = cbmem_read_mbuf(cbmem, hdr, om, offset, len);

    return (rc);
}

static int
log_cbmem_walk(struct log *log, log_walk_func_t walk_func,
               struct log_offset *log_offset)
{
    struct cbmem *cbmem;
    struct cbmem_entry_hdr *hdr;
    struct cbmem_iter iter;
    int rc;

    cbmem = (struct cbmem *) log->l_arg;

    rc = cbmem_lock_acquire(cbmem);
    if (rc != 0) {
        goto err;
    }

    /*
     * if timestamp for request is < 1, return last log entry
     */
    if (log_offset->lo_ts < 0) {
        hdr = cbmem->c_entry_end;
        if (hdr != NULL) {
            rc = walk_func(log, log_offset, (void *)hdr, hdr->ceh_len);
        }
    } else {
        cbmem_iter_start(cbmem, &iter);
        while (1) {
            hdr = cbmem_iter_next(cbmem, &iter);
            if (!hdr) {
                break;
            }

            rc = walk_func(log, log_offset, (void *)hdr, hdr->ceh_len);
            if (rc == 1) {
                break;
            }
        }
    }

    rc = cbmem_lock_release(cbmem);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int
log_cbmem_flush(struct log *log)
{
    struct cbmem *cbmem;
    int rc;

    cbmem = (struct cbmem *) log->l_arg;

    rc = cbmem_flush(cbmem);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

#if MYNEWT_VAL(LOG_STORAGE_INFO)
static int
log_cbmem_storage_info(struct log *log, struct log_storage_info *info)
{
    struct cbmem *cbmem;
    uint32_t size;
    uint32_t used;

    cbmem = (struct cbmem *)log->l_arg;

    size = cbmem->c_buf_end - cbmem->c_buf;

    used = (uint32_t)cbmem->c_entry_end + cbmem->c_entry_end->ceh_len -
           (uint32_t)cbmem->c_entry_start;
    if ((int32_t)used < 0) {
        used += size;
    }

    info->size = size;
    info->used = used;

    return 0;
}
#endif

const struct log_handler log_cbmem_handler = {
    .log_type = LOG_TYPE_MEMORY,
    .log_read = log_cbmem_read,
    .log_read_mbuf = log_cbmem_read_mbuf,
    .log_append = log_cbmem_append,
    .log_append_body = log_cbmem_append_body,
    .log_append_mbuf = log_cbmem_append_mbuf,
    .log_append_mbuf_body = log_cbmem_append_mbuf_body,
    .log_walk = log_cbmem_walk,
    .log_flush = log_cbmem_flush,
#if MYNEWT_VAL(LOG_STORAGE_INFO)
    .log_storage_info = log_cbmem_storage_info,
#endif
};
