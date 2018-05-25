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

#if MYNEWT_VAL(LOG_FCB)

#include <string.h>

#include "flash_map/flash_map.h"
#include "fcb/fcb.h"
#include "log/log.h"

static struct flash_area sector;

static int log_fcb_rtr_erase(struct log *log, void *arg);

static int
log_fcb_start_append(struct log *log, int len, struct fcb_entry *loc)
{
    struct fcb *fcb;
    struct fcb_log *fcb_log;
    int rc = 0;

    fcb_log = (struct fcb_log *)log->l_arg;
    fcb = &fcb_log->fl_fcb;

    while (1) {
        rc = fcb_append(fcb, len, loc);
        if (rc == 0) {
            break;
        }

        if (rc != FCB_ERR_NOSPACE) {
            goto err;
        }

        if (fcb_log->fl_entries) {
            rc = log_fcb_rtr_erase(log, fcb_log);
            if (rc) {
                goto err;
            }
            continue;
        }

        rc = fcb_rotate(fcb);
        if (rc) {
            goto err;
        }
    }

err:
    return (rc);
}

static int
log_fcb_append(struct log *log, void *buf, int len)
{
    struct fcb *fcb;
    struct fcb_entry loc;
    struct fcb_log *fcb_log;
    int rc;

    fcb_log = (struct fcb_log *)log->l_arg;
    fcb = &fcb_log->fl_fcb;

    rc = log_fcb_start_append(log, len, &loc);
    if (rc) {
        goto err;
    }

    rc = flash_area_write(loc.fe_area, loc.fe_data_off, buf, len);
    if (rc) {
        goto err;
    }

    rc = fcb_append_finish(fcb, &loc);

err:
    return (rc);
}

static int
log_fcb_append_mbuf(struct log *log, struct os_mbuf *om)
{
    struct fcb *fcb;
    struct fcb_entry loc;
    struct fcb_log *fcb_log;
    struct os_mbuf *om_tmp;
    int len;
    int rc;

    fcb_log = (struct fcb_log *)log->l_arg;
    fcb = &fcb_log->fl_fcb;

    len = 0;

    om_tmp = om;
    while (om_tmp) {
        len += om_tmp->om_len;
        om_tmp = SLIST_NEXT(om_tmp, om_next);
    }

    rc = log_fcb_start_append(log, len, &loc);
    if (rc) {
        goto err;
    }

    while (om) {
        rc = flash_area_write(loc.fe_area, loc.fe_data_off, om->om_data,
                              om->om_len);
        if (rc) {
            goto err;
        }

        loc.fe_data_off += om->om_len;
        om = SLIST_NEXT(om, om_next);
    }

    rc = fcb_append_finish(fcb, &loc);

err:
    return (rc);
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
log_fcb_read_mbuf(struct log *log, void *dptr, struct os_mbuf *om,
                  uint16_t offset, uint16_t len)
{
    struct fcb_entry *loc;
    uint8_t data[128];
    uint16_t read_len;
    uint16_t rem_len;
    int rc;

    loc = (struct fcb_entry *)dptr;

    if (offset + len > loc->fe_data_len) {
        len = loc->fe_data_len - offset;
    }

    rem_len = len;

    while (rem_len > 0) {
        read_len = min(rem_len, sizeof(data));
        rc = flash_area_read(loc->fe_area, loc->fe_data_off + offset, data,
                             read_len);
        if (rc) {
            goto done;
        }

        rc = os_mbuf_append(om, data, read_len);
        if (rc) {
            goto done;
        }

        rem_len -= read_len;
        offset += read_len;
    }

done:
    return len - rem_len;
}

static int
log_fcb_walk(struct log *log, log_walk_func_t walk_func,
             struct log_offset *log_offset)
{
    struct fcb *fcb;
    struct fcb_entry loc;
    struct fcb_entry *locp;
    int rc;

    rc = 0;
    fcb = &((struct fcb_log *)log->l_arg)->fl_fcb;

    memset(&loc, 0, sizeof(loc));

    /*
     * if timestamp for request is < 0, return last log entry
     */
    if (log_offset->lo_ts < 0) {
        locp = &fcb->f_active;
        rc = walk_func(log, log_offset, (void *)locp, locp->fe_data_len);
    } else {
        while (fcb_getnext(fcb, &loc) == 0) {
            rc = walk_func(log, log_offset, (void *) &loc, loc.fe_data_len);
            if (rc) {
                break;
            }
        }
    }
    return (rc);
}

static int
log_fcb_flush(struct log *log)
{
    return fcb_clear(&((struct fcb_log *)log->l_arg)->fl_fcb);
}

/**
 * Copies one log entry from source fcb to destination fcb
 * @param src_fcb, dst_fcb
 * @return 0 on success; non-zero on error
 */
static int
log_fcb_copy_entry(struct log *log, struct fcb_entry *entry,
                   struct fcb *dst_fcb)
{
    struct log_entry_hdr ueh;
    char data[LOG_PRINTF_MAX_ENTRY_LEN + sizeof(ueh)];
    int dlen;
    int rc;
    struct fcb *fcb_tmp;

    rc = log_fcb_read(log, entry, &ueh, 0, sizeof(ueh));
    if (rc != sizeof(ueh)) {
        goto err;
    }

    dlen = min(entry->fe_data_len, LOG_PRINTF_MAX_ENTRY_LEN + sizeof(ueh));

    rc = log_fcb_read(log, entry, data, 0, dlen);
    if (rc < 0) {
        goto err;
    }
    data[rc] = '\0';

    /* Changing the fcb to be logged to be dst fcb */
    fcb_tmp = &((struct fcb_log *)log->l_arg)->fl_fcb;

    log->l_arg = dst_fcb;
    rc = log_fcb_append(log, data, dlen);
    log->l_arg = fcb_tmp;
    if (rc) {
        goto err;
    }

err:
    return (rc);
}

/**
 * Copies log entries from source fcb to destination fcb
 * @param src_fcb, dst_fcb, element offset to start copying
 * @return 0 on success; non-zero on error
 */
static int
log_fcb_copy(struct log *log, struct fcb *src_fcb, struct fcb *dst_fcb,
             uint32_t offset)
{
    struct fcb_entry entry;
    int rc;

    rc = 0;

    memset(&entry, 0, sizeof(entry));
    while (!fcb_getnext(src_fcb, &entry)) {
        if (entry.fe_elem_off < offset) {
            continue;
        }
        rc = log_fcb_copy_entry(log, &entry, dst_fcb);
        if (rc) {
            break;
        }
    }

    return (rc);
}

/**
 * Flushes the log while restoring specified number of entries
 * using image scratch
 * @param src_fcb, dst_fcb
 * @return 0 on success; non-zero on error
 */
static int
log_fcb_rtr_erase(struct log *log, void *arg)
{
    struct fcb_log *fcb_log;
    struct fcb fcb_scratch;
    struct fcb *fcb;
    const struct flash_area *ptr;
    struct fcb_entry entry;
    int rc;

    rc = 0;
    if (!log) {
        rc = -1;
        goto err;
    }

    fcb_log = (struct fcb_log *)arg;
    fcb = (struct fcb *)fcb_log;

    memset(&fcb_scratch, 0, sizeof(fcb_scratch));

    if (flash_area_open(FLASH_AREA_IMAGE_SCRATCH, &ptr)) {
        goto err;
    }
    sector = *ptr;
    fcb_scratch.f_sectors = &sector;
    fcb_scratch.f_sector_cnt = 1;
    fcb_scratch.f_magic = 0x7EADBADF;
    fcb_scratch.f_version = g_log_info.li_version;

    flash_area_erase(&sector, 0, sector.fa_size);
    rc = fcb_init(&fcb_scratch);
    if (rc) {
        goto err;
    }

    /* Calculate offset of n-th last entry */
    rc = fcb_offset_last_n(fcb, fcb_log->fl_entries, &entry);
    if (rc) {
        goto err;
    }

    /* Copy to scratch */
    rc = log_fcb_copy(log, fcb, &fcb_scratch, entry.fe_elem_off);
    if (rc) {
        goto err;
    }

    /* Flush log */
    rc = log_fcb_flush(log);
    if (rc) {
        goto err;
    }

    /* Copy back from scratch */
    rc = log_fcb_copy(log, &fcb_scratch, fcb, 0);

err:
    return (rc);
}

const struct log_handler log_fcb_handler = {
    .log_type = LOG_TYPE_STORAGE,
    .log_read = log_fcb_read,
    .log_read_mbuf = log_fcb_read_mbuf,
    .log_append = log_fcb_append,
    .log_append_mbuf = log_fcb_append_mbuf,
    .log_walk = log_fcb_walk,
    .log_flush = log_fcb_flush,
};

#endif
