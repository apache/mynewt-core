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

#if MYNEWT_VAL(LOG_STDOUT)

#include <cbmem/cbmem.h>
#include "log/log.h"
#include "stdio.h"

static int
log_stdout_append(struct log *log, void *buf, int len)
{
    struct log_entry_hdr *hdr;

    hdr = (struct log_entry_hdr *) buf;
    printf("[ts=%llu, mod=%u level=%u] ",
            hdr->ue_ts, hdr->ue_module, hdr->ue_level);

    fwrite((char *) buf + LOG_ENTRY_HDR_SIZE, len - LOG_ENTRY_HDR_SIZE, 1, stdout);
    return (0);
}

static int
log_stdout_read(struct log *log, void *dptr, void *buf, uint16_t offset,
        uint16_t len)
{
    /* You don't read console, console read you */
    return (OS_EINVAL);
}

static int
log_stdout_walk(struct log *log, log_walk_func_t walk_func,
        struct log_offset *log_offset)
{
    /* You don't walk console, console walk you. */
    return (OS_EINVAL);
}

static int
log_stdout_flush(struct log *log)
{
    /* You don't flush console, console flush you. */
    return (OS_EINVAL);
}

const struct log_handler log_stdout_handler = {
    .log_type = LOG_TYPE_STREAM,
    .log_read = log_stdout_read,
    .log_append = log_stdout_append,
    .log_walk = log_stdout_walk,
    .log_flush = log_stdout_flush,
};

#endif
