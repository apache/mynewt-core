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
log_cbmem_append_mbuf(struct log *log, struct os_mbuf *om)
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

const struct log_handler log_cbmem_handler = {
    .log_type = LOG_TYPE_MEMORY,
    .log_read = log_cbmem_read,
    .log_read_mbuf = log_cbmem_read_mbuf,
    .log_append = log_cbmem_append,
    .log_append_mbuf = log_cbmem_append_mbuf,
    .log_walk = log_cbmem_walk,
    .log_flush = log_cbmem_flush,
};
