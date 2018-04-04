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

#include <string.h>
#include "log/log.h"
#include "log/log_fcb_slot1.h"

#if MYNEWT_VAL(LOG_FCB_SLOT1)

static bool g_slot1_locked;

static int
log_fcb_slot1_append(struct log *log, void *buf, int len)
{
    if (g_slot1_locked) {
        return OS_EBUSY;
    }

    return log_fcb_handler.log_append(log, buf, len);
}

static int
log_fcb_slot1_append_mbuf(struct log *log, struct os_mbuf *om)
{
    if (g_slot1_locked) {
        return OS_EBUSY;
    }

    return log_fcb_handler.log_append_mbuf(log, om);
}

static int
log_fcb_slot1_read(struct log *log, void *dptr, void *buf, uint16_t offset,
                   uint16_t len)
{
    if (g_slot1_locked) {
        return OS_EBUSY;
    }

    return log_fcb_handler.log_read(log, dptr, buf, offset, len);
}

static int
log_fcb_slot1_walk(struct log *log, log_walk_func_t walk_func,
                   struct log_offset *log_offset)
{
    if (g_slot1_locked) {
        return OS_EBUSY;
    }

    return log_fcb_handler.log_walk(log, walk_func, log_offset);
}

static int
log_fcb_slot1_flush(struct log *log)
{
    if (g_slot1_locked) {
        return OS_EBUSY;
    }

    return log_fcb_handler.log_flush(log);
}

const struct log_handler log_fcb_slot1_handler = {
    .log_type = LOG_TYPE_STORAGE,
    .log_read = log_fcb_slot1_read,
    .log_append = log_fcb_slot1_append,
    .log_append_mbuf = log_fcb_slot1_append_mbuf,
    .log_walk = log_fcb_slot1_walk,
    .log_flush = log_fcb_slot1_flush,
};

void log_fcb_slot1_lock(void)
{
    g_slot1_locked = true;
}

bool log_fcb_slot1_is_locked(void)
{
    return g_slot1_locked;
}

#endif
