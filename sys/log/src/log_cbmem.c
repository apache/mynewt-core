/**
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

#include <os/os.h>

#include <util/cbmem.h>

#include "log/log.h"


static int
log_cbmem_append(struct log *log, void *buf, int len)
{
    struct cbmem *cbmem;
    int rc;

    cbmem = (struct cbmem *) log->l_log->log_arg;

    rc = cbmem_append(cbmem, buf, len);
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

    cbmem = (struct cbmem *) log->l_log->log_arg;
    hdr = (struct cbmem_entry_hdr *) dptr;

    rc = cbmem_read(cbmem, hdr, buf, offset, len);

    return (rc);
}

static int
log_cbmem_walk(struct log *log, log_walk_func_t walk_func, void *arg)
{
    struct cbmem *cbmem;
    struct cbmem_entry_hdr *hdr;
    struct cbmem_iter iter;
    int rc;

    cbmem = (struct cbmem *) log->l_log->log_arg;

    rc = cbmem_lock_acquire(cbmem);
    if (rc != 0) {
        goto err;
    }

    cbmem_iter_start(cbmem, &iter);
    while (1) {
        hdr = cbmem_iter_next(cbmem, &iter);
        if (!hdr) {
            break;
        }

        rc = walk_func(log, arg, (void *)hdr, hdr->ceh_len);
        if (rc == 1) {
            break;
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

    cbmem = (struct cbmem *) log->l_log->log_arg;

    rc = cbmem_flush(cbmem);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
log_cbmem_handler_init(struct log_handler *handler, struct cbmem *cbmem)
{
    handler->log_type = LOG_TYPE_MEMORY;
    handler->log_read = log_cbmem_read;
    handler->log_append = log_cbmem_append;
    handler->log_walk = log_cbmem_walk;
    handler->log_flush = log_cbmem_flush;
    handler->log_arg = (void *) cbmem;
    handler->log_rtr_erase = NULL;

    return (0);
}
