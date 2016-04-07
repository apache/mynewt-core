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
#include <string.h>

#include <os/os.h>

#include <hal/flash_map.h>
#include <fcb/fcb.h>

#include "log/log.h"

static int
log_fcb_append(struct log *log, void *buf, int len)
{
    struct fcb *fcb;
    struct fcb_entry loc;
    int rc;

    fcb = (struct fcb *)log->l_log->log_arg;

    while (1) {
        rc = fcb_append(fcb, len, &loc);
        if (rc == 0) {
            break;
        }
        if (rc != FCB_ERR_NOSPACE) {
            return rc;
        }
        rc = fcb_rotate(fcb);
        if (rc) {
            return rc;
        }
    }
    rc = flash_area_write(loc.fe_area, loc.fe_data_off, buf, len);
    if (rc) {
        return rc;
    }
    fcb_append_finish(fcb, &loc);
    return 0;
}

static int
log_fcb_read(struct log *log, void *dptr, void *buf, uint16_t offset,
  uint16_t len)
{
    struct fcb_entry *loc;
    int rc;

    loc = (struct fcb_entry *)dptr;

    if (offset + len > loc->fe_data_len) {
        len = loc->fe_data_len - offset;
    }
    rc = flash_area_read(loc->fe_area, loc->fe_data_off + offset, buf, len);
    if (rc == 0) {
        return len;
    } else {
        return 0;
    }
}

static int
log_fcb_walk(struct log *log, log_walk_func_t walk_func, void *arg)
{
    struct fcb *fcb;
    struct fcb_entry loc;
    int rc;

    fcb = (struct fcb *)log->l_log->log_arg;

    memset(&loc, 0, sizeof(loc));

    while (fcb_getnext(fcb, &loc) == 0) {
        rc = walk_func(log, arg, (void *) &loc, loc.fe_data_len);
        if (rc == 1) {
            break;
        }
    }
    return 0;
}

static int
log_fcb_flush(struct log *log)
{
    struct fcb *fcb;
    int rc;

    fcb = (struct fcb *)log->l_log->log_arg;
    rc = 0;

    while (!fcb_is_empty(fcb)) {
        rc = fcb_rotate(fcb);
        if (rc) {
            break;
        }
    }
    return rc;
}

int
log_fcb_handler_init(struct log_handler *handler, struct fcb *fcb)
{
    handler->log_type = LOG_TYPE_STORAGE;
    handler->log_read = log_fcb_read;
    handler->log_append = log_fcb_append;
    handler->log_walk = log_fcb_walk;
    handler->log_flush = log_fcb_flush;
    handler->log_arg = fcb;

    return 0;
}
