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

/* Assume the flash alignment requirement is no stricter than 8. */
#define LOG_FCB_MAX_ALIGN   8

static struct flash_area sector;

static int log_fcb_rtr_erase(struct log *log, void *arg);

/**
 * Finds the first log entry whose "offset" is >= the one specified.  A log
 * offset consists of two parts:
 *     o timestamp
 *     o index
 *
 * The "timestamp" field is misnamed.  If it has a value of -1, then the offset
 * always points to the latest entry.  If this value is not -1, then it is
 * ignored; the "index" field is used instead.
 *
 * XXX: We should rename "timestamp" or make it an actual timestamp.
 *
 * The "index" field corresponds to a log entry index.
 *
 * If bookmarks are enabled, this function uses them in the search.
 *
 * @return                      0 if an entry was found
 *                              SYS_ENOENT if there are no suitable entries.
 *                              Other error on failure.
 */
static int
log_fcb_find_gte(struct log *log, struct log_offset *log_offset,
                 struct fcb_entry *out_entry)
{
#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
    const struct fcb_log_bmark *bmark;
#endif
    struct log_entry_hdr hdr;
    struct fcb_log *fcb_log;
    struct fcb *fcb;
    int rc;

    fcb_log = log->l_arg;
    fcb = &fcb_log->fl_fcb;

    /* Attempt to read the first entry.  If this fails, the FCB is empty. */
    memset(out_entry, 0, sizeof *out_entry);
    rc = fcb_getnext(fcb, out_entry);
    if (rc == FCB_ERR_NOVAR) {
        return SYS_ENOENT;
    } else if (rc != 0) {
        return SYS_EUNKNOWN;
    }

    /*
     * if timestamp for request is < 0, return last log entry
     */
    if (log_offset->lo_ts < 0) {
        *out_entry = fcb->f_active;
        return 0;
    }

    /* If the requested index is beyond the end of the log, there is nothing to
     * retrieve.
     */
    rc = log_read_hdr(log, &fcb->f_active, &hdr);
    if (rc != 0) {
        return rc;
    }
    if (log_offset->lo_index > hdr.ue_index) {
        return SYS_ENOENT;
    }

#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
    bmark = fcb_log_closest_bmark(fcb_log, log_offset->lo_index);
    if (bmark != NULL) {
        *out_entry = bmark->flb_entry;
    }
#endif

    /* Keep advancing until we find an entry with a great enough index. */
    do {
        rc = log_read_hdr(log, out_entry, &hdr);
        if (rc != 0) {
            return rc;
        }

        if (hdr.ue_index >= log_offset->lo_index) {
            return 0;
        }
    } while (fcb_getnext(fcb, out_entry) == 0);

    return SYS_ENOENT;
}

static int
log_fcb_start_append(struct log *log, int len, struct fcb_entry *loc)
{
    struct fcb *fcb;
    struct fcb_log *fcb_log;
    struct flash_area *old_fa;
    int rc = 0;
#if MYNEWT_VAL(LOG_STATS)
    int cnt;
#endif

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

        old_fa = fcb->f_oldest;
        (void)old_fa; /* to avoid #ifdefs everywhere... */

#if MYNEWT_VAL(LOG_STATS)
        rc = fcb_area_info(fcb, NULL, &cnt, NULL);
        if (rc == 0) {
            LOG_STATS_INCN(log, lost, cnt);
        }
#endif

#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
        /* The FCB needs to be rotated.  Invalidate all bookmarks. */
        fcb_log_clear_bmarks(fcb_log);
#endif

        rc = fcb_rotate(fcb);
        if (rc) {
            goto err;
        }

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
        /*
         * FCB was rotated successfully so let's check if watermark was within
         * oldest flash area which was erased. If yes, then move watermark to
         * beginning of current oldest area.
         */
        if ((fcb_log->fl_watermark_off >= old_fa->fa_off) &&
            (fcb_log->fl_watermark_off < old_fa->fa_off + old_fa->fa_size)) {
            fcb_log->fl_watermark_off = fcb->f_oldest->fa_off;
        }
#endif


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

/**
 * Calculates the number of message body bytes that should be included after
 * the entry header in the first write.  Inclusion of body bytes is necessary
 * to satisfy the flash hardware's write alignment restrictions.
 */
static int
log_fcb_hdr_body_bytes(uint8_t align)
{
    uint8_t mod;

    /* Assume power-of-two alignment for faster modulo calculation. */
    assert((align & (align - 1)) == 0);

    mod = sizeof (struct log_entry_hdr) & (align - 1);
    if (mod == 0) {
        return 0;
    }

    return align - mod;
}

static int
log_fcb_append_body(struct log *log, const struct log_entry_hdr *hdr,
                    const void *body, int body_len)
{
    uint8_t buf[sizeof (struct log_entry_hdr) + LOG_FCB_MAX_ALIGN - 1];
    struct fcb *fcb;
    struct fcb_entry loc;
    struct fcb_log *fcb_log;
    const uint8_t *u8p;
    int hdr_alignment;
    int chunk_sz;
    int rc;

    fcb_log = (struct fcb_log *)log->l_arg;
    fcb = &fcb_log->fl_fcb;

    if (fcb->f_align > LOG_FCB_MAX_ALIGN) {
        return SYS_ENOTSUP;
    }

    rc = log_fcb_start_append(log, sizeof *hdr + body_len, &loc);
    if (rc != 0) {
        return rc;
    }

    /* Append the first chunk (header + x-bytes of body, where x is however
     * many bytes are required to increase the chunk size up to a multiple of
     * the flash alignment).
     */
    hdr_alignment = log_fcb_hdr_body_bytes(fcb->f_align);
    if (hdr_alignment > body_len) {
        chunk_sz = sizeof *hdr + body_len;
    } else {
        chunk_sz = sizeof *hdr + hdr_alignment;
    }

    u8p = body;

    memcpy(buf, hdr, sizeof *hdr);
    memcpy(buf + sizeof *hdr, u8p, hdr_alignment);

    rc = flash_area_write(loc.fe_area, loc.fe_data_off, buf, chunk_sz);
    if (rc != 0) {
        return rc;
    }

    /* Append the remainder of the message body. */

    u8p += hdr_alignment;
    body_len -= hdr_alignment;

    if (body_len > 0) {
        rc = flash_area_write(loc.fe_area, loc.fe_data_off + chunk_sz, u8p,
                              body_len);
        if (rc != 0) {
            return rc;
        }
    }

    rc = fcb_append_finish(fcb, &loc);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static int
log_fcb_write_mbuf(struct fcb_entry *loc, const struct os_mbuf *om)
{
    int rc;

    while (om) {
        rc = flash_area_write(loc->fe_area, loc->fe_data_off,
                              om->om_data, om->om_len);
        if (rc != 0) {
            return SYS_EIO;
        }

        loc->fe_data_off += om->om_len;
        om = SLIST_NEXT(om, om_next);
    }

    return 0;
}

static int
log_fcb_append_mbuf(struct log *log, const struct os_mbuf *om)
{
    struct fcb *fcb;
    struct fcb_entry loc;
    struct fcb_log *fcb_log;
    int len;
    int rc;

    fcb_log = (struct fcb_log *)log->l_arg;
    fcb = &fcb_log->fl_fcb;

    /* This function expects to be able to write each mbuf without any
     * buffering.
     */
    /* XXX: Fix this; buffer mbuf writes to satisfy flash alignment. */
    if (fcb->f_align != 1) {
        return SYS_ENOTSUP;
    }

    len = os_mbuf_len(om);
    rc = log_fcb_start_append(log, len, &loc);
    if (rc != 0) {
        return rc;
    }

    rc = log_fcb_write_mbuf(&loc, om);
    if (rc != 0) {
        return rc;
    }

    rc = fcb_append_finish(fcb, &loc);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static int
log_fcb_append_mbuf_body(struct log *log, const struct log_entry_hdr *hdr,
                         const struct os_mbuf *om)
{
    struct fcb *fcb;
    struct fcb_entry loc;
    struct fcb_log *fcb_log;
    int len;
    int rc;

    fcb_log = (struct fcb_log *)log->l_arg;
    fcb = &fcb_log->fl_fcb;

    /* This function expects to be able to write each mbuf without any
     * buffering.
     */
    if (fcb->f_align != 1) {
        return SYS_ENOTSUP;
    }

    len = sizeof *hdr + os_mbuf_len(om);
    rc = log_fcb_start_append(log, len, &loc);
    if (rc != 0) {
        return rc;
    }

    rc = flash_area_write(loc.fe_area, loc.fe_data_off, hdr, sizeof *hdr);
    if (rc != 0) {
        return rc;
    }
    loc.fe_data_off += sizeof *hdr;

    rc = log_fcb_write_mbuf(&loc, om);
    if (rc != 0) {
        return rc;
    }

    rc = fcb_append_finish(fcb, &loc);
    if (rc != 0) {
        return rc;
    }

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
    struct fcb_log *fcb_log;
    struct fcb_entry loc;
    int rc;

    fcb_log = log->l_arg;
    fcb = &fcb_log->fl_fcb;

    /* Locate the starting point of the walk. */
    rc = log_fcb_find_gte(log, log_offset, &loc);
    switch (rc) {
    case 0:
        /* Found a starting point. */
        break;
    case SYS_ENOENT:
        /* No entries match the offset criteria; nothing to walk. */
        return 0;
    default:
        return rc;
    }

#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
    /* If a minimum index was specified (i.e., we are not just retrieving the
     * last entry), add a bookmark pointing to this walk's start location.
     */
    if (log_offset->lo_ts >= 0) {
        fcb_log_add_bmark(fcb_log, &loc, log_offset->lo_index);
    }
#endif

    do {
        rc = walk_func(log, log_offset, &loc, loc.fe_data_len);
        if (rc != 0) {
            return rc;
        }
    } while (fcb_getnext(fcb, &loc) == 0);

    return 0;
}

static int
log_fcb_flush(struct log *log)
{
    struct fcb_log *fcb_log;
    struct fcb *fcb;

    fcb_log = (struct fcb_log *)log->l_arg;
    fcb = &fcb_log->fl_fcb;

#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
    fcb_log_clear_bmarks(fcb_log);
#endif

    return fcb_clear(fcb);
}

static int
log_fcb_registered(struct log *log)
{
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    struct fcb_log *fl;
    struct fcb *fcb;
    struct fcb_entry loc;

    fl = (struct fcb_log *)log->l_arg;
    fcb = &fl->fl_fcb;

    /* Set watermark to first element */
    memset(&loc, 0, sizeof(loc));
    if (fcb_getnext(fcb, &loc)) {
        fl->fl_watermark_off = loc.fe_area->fa_off + loc.fe_elem_off;
    } else {
        fl->fl_watermark_off = fcb->f_oldest->fa_off;
    }
#endif
    return 0;
}

#if MYNEWT_VAL(LOG_STORAGE_INFO)
static int
log_fcb_storage_info(struct log *log, struct log_storage_info *info)
{
    struct fcb_log *fl;
    struct fcb *fcb;
    struct flash_area *fa;
    uint32_t el_min;
    uint32_t el_max;
    uint32_t fa_min;
    uint32_t fa_max;
    uint32_t fa_size;
    uint32_t fa_used;
    int rc;

    fl = (struct fcb_log *)log->l_arg;
    fcb = &fl->fl_fcb;

    rc = os_mutex_pend(&fcb->f_mtx, OS_WAIT_FOREVER);
    if (rc && rc != OS_NOT_STARTED) {
        return FCB_ERR_ARGS;
    }

    /*
     * Calculate location of 1st entry.
     * We assume 1st log entry starts at beginning of oldest sector in FCB.
     * This is because even if 1st entry is in the middle of sector (is this
     * even possible?) we will never use free space before it thus that space
     * can be also considered used.
     */
    el_min = fcb->f_oldest->fa_off;

    /* Calculate end location of last entry */
    el_max = fcb->f_active.fe_area->fa_off + fcb->f_active.fe_elem_off;

    /* Sectors assigned to FCB are guaranteed to be contiguous */
    fa = &fcb->f_sectors[0];
    fa_min = fa->fa_off;
    fa = &fcb->f_sectors[fcb->f_sector_cnt - 1];
    fa_max = fa->fa_off + fa->fa_size;
    fa_size = fa_max - fa_min;

    /* Calculate used size */
    fa_used = el_max - el_min;
    if ((int32_t)fa_used < 0) {
        fa_used += fa_size;
    }

    info->size = fa_size;
    info->used = fa_used;

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    /* Calculate used size */
    fa_used = el_max - fl->fl_watermark_off;
    if ((int32_t)fa_used < 0) {
        fa_used += fa_size;
    }
    info->used_unread = fa_used;
#endif

    os_mutex_release(&fcb->f_mtx);

    return 0;
}
#endif

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
static int
log_fcb_set_watermark(struct log *log, uint32_t index)
{
    struct fcb_log *fl;
    struct fcb *fcb;
    struct log_entry_hdr ueh;
    struct fcb_entry loc;
    uint32_t end_off;
    int rc;

    fl = (struct fcb_log *)log->l_arg;
    fcb = &fl->fl_fcb;

    memset(&loc, 0, sizeof(loc));
    end_off = fcb->f_oldest->fa_off;
    rc = 0;

    while (fcb_getnext(fcb, &loc) == 0) {
        rc = log_fcb_read(log, &loc, &ueh, 0, sizeof(ueh));

        if (rc != sizeof(ueh)) {
            break;
        }

        if (ueh.ue_index > index) {
            break;
        }

        /* Move end offset max pointer to end of this element */
        end_off = loc.fe_area->fa_off + loc.fe_data_off + loc.fe_data_len;
    }

    /* End of last element found is now our watermark */
    fl->fl_watermark_off = end_off;

    return rc;
}
#endif

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
    .log_append_body = log_fcb_append_body,
    .log_append_mbuf = log_fcb_append_mbuf,
    .log_append_mbuf_body = log_fcb_append_mbuf_body,
    .log_walk = log_fcb_walk,
    .log_flush = log_fcb_flush,
#if MYNEWT_VAL(LOG_STORAGE_INFO)
    .log_storage_info = log_fcb_storage_info,
#endif
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    .log_set_watermark = log_fcb_set_watermark,
#endif
    .log_registered = log_fcb_registered,
};

#endif
