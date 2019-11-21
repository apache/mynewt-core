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

#if MYNEWT_VAL(LOG_FCB2)

#include <string.h>

#include "flash_map/flash_map.h"
#include "log/log.h"
#include "fcb/fcb2.h"

/* Assume the flash alignment requirement is no stricter than 8. */
#define LOG_FCB2_MAX_ALIGN   8

static int log_fcb2_rtr_erase(struct log *log);

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
log_fcb2_find_gte(struct log *log, struct log_offset *log_offset,
                  struct fcb2_entry *out_entry)
{
#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
    const struct log_fcb_bmark *bmark;
#endif
    struct log_entry_hdr hdr;
    struct fcb_log *fcb_log;
    struct fcb2 *fcb;
    int rc;

    fcb_log = log->l_arg;
    fcb = &fcb_log->fl_fcb;

    /* Attempt to read the last entry.  If this fails, the FCB is empty. */
    memset(out_entry, 0, sizeof(*out_entry));
    rc = fcb2_getprev(fcb, out_entry);
    if (rc == FCB2_ERR_NOVAR) {
        return SYS_ENOENT;
    } else if (rc != 0) {
        return SYS_EUNKNOWN;
    }

    /*
     * if timestamp for request is < 0, return last log entry (already read).
     */
    if (log_offset->lo_ts < 0) {
        return 0;
    }

    /* If the requested index is beyond the end of the log, there is nothing to
     * retrieve.
     */
    rc = log_read_hdr(log, out_entry, &hdr);
    if (rc != 0) {
        return rc;
    }
    if (log_offset->lo_index > hdr.ue_index) {
        return SYS_ENOENT;
    }

    /*
     * Start from beginning.
     */
    memset(out_entry, 0, sizeof(*out_entry));
    rc = fcb2_getnext(fcb, out_entry);
    if (rc != 0) {
        return SYS_EUNKNOWN;
    }
#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
    bmark = log_fcb_closest_bmark(fcb_log, log_offset->lo_index);
    if (bmark != NULL) {
        *out_entry = bmark->lfb_entry;
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
    } while (fcb2_getnext(fcb, out_entry) == 0);

    return SYS_ENOENT;
}

static int
log_fcb2_start_append(struct log *log, int len, struct fcb2_entry *loc)
{
    struct fcb2 *fcb;
    struct fcb_log *fcb_log;
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    int old_sec;
#endif
    int rc = 0;
#if MYNEWT_VAL(LOG_STATS)
    int cnt;
#endif

    fcb_log = (struct fcb_log *)log->l_arg;
    fcb = &fcb_log->fl_fcb;

    while (1) {
        rc = fcb2_append(fcb, len, loc);
        if (rc == 0) {
            break;
        }

        if (rc != FCB2_ERR_NOSPACE) {
            goto err;
        }

        if (fcb_log->fl_entries) {
            rc = log_fcb2_rtr_erase(log);
            if (rc) {
                goto err;
            }
            continue;
        }

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
        old_sec = fcb->f_oldest_sec;
#endif

#if MYNEWT_VAL(LOG_STATS)
        rc = fcb2_area_info(fcb, FCB2_SECTOR_OLDEST, &cnt, NULL);
        if (rc == 0) {
            LOG_STATS_INCN(log, lost, cnt);
        }
#endif

#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
        /* The FCB needs to be rotated. */
        log_fcb_rotate_bmarks(fcb_log);
#endif

        rc = fcb2_rotate(fcb);
        if (rc) {
            goto err;
        }

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
        /*
         * FCB was rotated successfully so let's check if watermark was within
         * oldest flash area which was erased. If yes, then move watermark to
         * beginning of current oldest area.
         */
        if (fcb_log->fl_watermark_sec == old_sec) {
            fcb_log->fl_watermark_sec = fcb->f_oldest_sec;
            fcb_log->fl_watermark_off = 0;
        }
#endif
    }

err:
    return (rc);
}

/**
 * Calculates the number of message body bytes that should be included after
 * the entry header in the first write.  Inclusion of body bytes is necessary
 * to satisfy the flash hardware's write alignment restrictions.
 */
static int
log_fcb2_hdr_body_bytes(uint8_t align, uint8_t hdr_len)
{
    uint8_t mod;

    /* Assume power-of-two alignment for faster modulo calculation. */
    assert((align & (align - 1)) == 0);

    mod = hdr_len & (align - 1);
    if (mod == 0) {
        return 0;
    }

    return align - mod;
}

static int
log_fcb2_append_body(struct log *log, const struct log_entry_hdr *hdr,
                     const void *body, int body_len)
{
    uint8_t buf[LOG_BASE_ENTRY_HDR_SIZE + LOG_IMG_HASHLEN +
                LOG_FCB2_MAX_ALIGN - 1];
    struct fcb2_entry loc;
    const uint8_t *u8p;
    int hdr_alignment;
    int chunk_sz;
    int rc;
    uint16_t hdr_len;

    hdr_len = log_hdr_len(hdr);

    rc = log_fcb2_start_append(log, hdr_len + body_len, &loc);
    if (rc != 0) {
        return rc;
    }

    /* Append the first chunk (header + x-bytes of body, where x is however
     * many bytes are required to increase the chunk size up to a multiple of
     * the flash alignment). If the hash flag is set, we have to account for
     * appending the hash right after the header.
     */
    hdr_alignment = log_fcb2_hdr_body_bytes(loc.fe_range->fsr_align, hdr_len);
    if (hdr_alignment > body_len) {
        chunk_sz = hdr_len + body_len;
    } else {
        chunk_sz = hdr_len + hdr_alignment;
    }

    /*
     * Based on the flags being set in the log header, we need to write
     * specific fields to the flash
     */

    u8p = body;

    memcpy(buf, hdr, LOG_BASE_ENTRY_HDR_SIZE);
#if MYNEWT_VAL(LOG_VERSION) > 2
    if (hdr->ue_flags & LOG_FLAGS_IMG_HASH) {
        memcpy(buf + LOG_BASE_ENTRY_HDR_SIZE, hdr->ue_imghash, LOG_IMG_HASHLEN);
    }
#endif
    memcpy(buf + hdr_len, u8p, hdr_alignment);

    rc = fcb2_write(&loc, 0, buf, chunk_sz);
    if (rc != 0) {
        return rc;
    }

    /* Append the remainder of the message body. */

    u8p += hdr_alignment;
    body_len -= hdr_alignment;

    if (body_len > 0) {
        rc = fcb2_write(&loc, chunk_sz, u8p, body_len);
        if (rc != 0) {
            return rc;
        }
    }

    rc = fcb2_append_finish(&loc);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static int
log_fcb2_append(struct log *log, void *buf, int len)
{
    int hdr_len;

    hdr_len = log_hdr_len(buf);

    return log_fcb2_append_body(log, buf, (uint8_t *)buf + hdr_len,
                                len - hdr_len);
}

static int
log_fcb2_write_mbuf(struct fcb2_entry *loc, struct os_mbuf *om, int off)
{
    int rc;

    while (om) {
        rc = fcb2_write(loc, off, om->om_data, om->om_len);
        if (rc != 0) {
            return SYS_EIO;
        }

        off += om->om_len;
        om = SLIST_NEXT(om, om_next);
    }

    return 0;
}

static int
log_fcb2_append_mbuf_body(struct log *log, const struct log_entry_hdr *hdr,
                          struct os_mbuf *om)
{
    struct fcb2_entry loc;
    int len;
    int rc;

#if 0 /* XXXX */
    fcb_log = (struct fcb_log *)log->l_arg;
    fcb = &fcb_log->fl_fcb;

    /* This function expects to be able to write each mbuf without any
     * buffering.
     */
    if (fcb->f_align != 1) {
        return SYS_ENOTSUP;
    }
#endif

    len = log_hdr_len(hdr) + os_mbuf_len(om);
    rc = log_fcb2_start_append(log, len, &loc);
    if (rc != 0) {
        return rc;
    }

    rc = fcb2_write(&loc, 0, hdr, LOG_BASE_ENTRY_HDR_SIZE);
    if (rc != 0) {
        return rc;
    }
    len = LOG_BASE_ENTRY_HDR_SIZE;

#if MYNEWT_VAL(LOG_VERSION) > 2
    if (hdr->ue_flags & LOG_FLAGS_IMG_HASH) {
        /* Write LOG_IMG_HASHLEN bytes of image hash */
        rc = fcb2_write(&loc, len, hdr->ue_imghash, LOG_IMG_HASHLEN);
        if (rc != 0) {
            return rc;
        }
        len += LOG_IMG_HASHLEN;
    }
#endif
    rc = log_fcb2_write_mbuf(&loc, om, len);
    if (rc != 0) {
        return rc;
    }

    rc = fcb2_append_finish(&loc);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static int
log_fcb2_append_mbuf(struct log *log, struct os_mbuf *om)
{
    int rc;
    uint16_t mlen;
    uint16_t hdr_len;
    struct log_entry_hdr hdr;

    mlen = os_mbuf_len(om);
    if (mlen < LOG_BASE_ENTRY_HDR_SIZE) {
        return SYS_ENOMEM;
    }

    /*
     * We do a pull up twice, once so that the base header is
     * contiguous, so that we read the flags correctly, second
     * time is so that we account for the image hash as well.
     */
    om = os_mbuf_pullup(om, LOG_BASE_ENTRY_HDR_SIZE);

    /*
     * We can just pass the om->om_data ptr as the log_entry_hdr
     * because the log_entry_hdr is a packed struct and does not
     * cause any alignment or padding issues
     */
    hdr_len = log_hdr_len((struct log_entry_hdr *)om->om_data);

    om = os_mbuf_pullup(om, hdr_len);

    memcpy(&hdr, om->om_data, hdr_len);

    os_mbuf_adj(om, hdr_len);

    rc = log_fcb2_append_mbuf_body(log, &hdr, om);

    os_mbuf_prepend(om, hdr_len);

    memcpy(om->om_data, &hdr, hdr_len);

    return rc;
}

static int
log_fcb2_read(struct log *log, const void *dptr, void *buf, uint16_t off, uint16_t len)
{
    struct fcb2_entry *loc;
    int rc;

    loc = (struct fcb2_entry *)dptr;

    if (off + len > loc->fe_data_len) {
        len = loc->fe_data_len - off;
    }
    rc = fcb2_read(loc, off, buf, len);
    if (rc == 0) {
        return len;
    } else {
        return 0;
    }
}

static int
log_fcb2_read_mbuf(struct log *log, const void *dptr, struct os_mbuf *om,
                   uint16_t off, uint16_t len)
{
    struct fcb2_entry *loc;
    uint8_t data[128];
    uint16_t read_len;
    uint16_t rem_len;
    int rc;

    loc = (struct fcb2_entry *)dptr;

    if (off + len > loc->fe_data_len) {
        len = loc->fe_data_len - off;
    }

    rem_len = len;

    while (rem_len > 0) {
        read_len = min(rem_len, sizeof(data));
        rc = fcb2_read(loc, off, data, read_len);
        if (rc) {
            goto done;
        }

        rc = os_mbuf_append(om, data, read_len);
        if (rc) {
            goto done;
        }

        rem_len -= read_len;
        off += read_len;
    }

done:
    return len - rem_len;
}

static int
log_fcb2_walk(struct log *log, log_walk_func_t walk_func,
              struct log_offset *log_off)
{
    struct fcb2 *fcb;
    struct fcb_log *fcb_log;
    struct fcb2_entry loc;
    int rc;

    fcb_log = log->l_arg;
    fcb = &fcb_log->fl_fcb;

    /* Locate the starting point of the walk. */
    rc = log_fcb2_find_gte(log, log_off, &loc);
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
    if (log_off->lo_ts >= 0) {
        log_fcb_add_bmark(fcb_log, &loc, log_off->lo_index);
    }
#endif

    do {
        rc = walk_func(log, log_off, &loc, loc.fe_data_len);
        if (rc != 0) {
            if (rc < 0) {
                return rc;
            } else {
                return 0;
            }
        }
    } while (fcb2_getnext(fcb, &loc) == 0);

    return 0;
}

static int
log_fcb2_flush(struct log *log)
{
    struct fcb_log *fcb_log;
    struct fcb2 *fcb;

    fcb_log = (struct fcb_log *)log->l_arg;
    fcb = &fcb_log->fl_fcb;

#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
    log_fcb_clear_bmarks(fcb_log);
#endif

    return fcb2_clear(fcb);
}

static int
log_fcb2_registered(struct log *log)
{
    struct fcb2 *fcb;
    int i;
    struct fcb_log *fl;
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
#if MYNEWT_VAL(LOG_PERSIST_WATERMARK)
    struct fcb2_entry loc;
#endif
#endif

    fl = (struct fcb_log *)log->l_arg;
    fcb = &fl->fl_fcb;

    for (i = 0; i < fcb->f_range_cnt; i++) {
        if (fcb->f_ranges[i].fsr_align > LOG_FCB2_MAX_ALIGN) {
            return SYS_ENOTSUP;
        }
    }

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
#if MYNEWT_VAL(LOG_PERSIST_WATERMARK)
    /* Set watermark to first element */
    memset(&loc, 0, sizeof(loc));

    if (fcb2_getnext(fcb, &loc)) {
        fl->fl_watermark_sec = loc.fe_sector;
        fl->fl_watermark_off = loc.fe_data_off;
    } else {
        fl->fl_watermark_sec = fcb->f_oldest_sec;
        fl->fl_watermark_off = 0;
    }
#else
    /* Initialize watermark to designated unknown value*/
    fl->fl_watermark_sec = FCB2_SECTOR_OLDEST;
    fl->fl_watermark_off = 0xffffffff;
#endif
#endif
    return 0;
}

#if MYNEWT_VAL(LOG_STORAGE_INFO)
static int
log_fcb2_storage_info(struct log *log, struct log_storage_info *info)
{
    struct fcb_log *fl;
    struct fcb2 *fcb;
    int i;
    int j;
    int sec;
    uint16_t el_min_sec;
    uint16_t el_max_sec;
    int rc;

    fl = (struct fcb_log *)log->l_arg;
    fcb = &fl->fl_fcb;

    rc = os_mutex_pend(&fcb->f_mtx, OS_WAIT_FOREVER);
    if (rc && rc != OS_NOT_STARTED) {
        return FCB2_ERR_ARGS;
    }

    el_min_sec = fcb->f_oldest_sec;
    el_max_sec = fcb->f_active.fe_sector;

    info->size = 0;
    info->used = 0;
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    info->used_unread = 0;
#endif

    sec = 0;
    for (i = 0; i < fcb->f_range_cnt; i++) {
        info->size += fcb->f_ranges[i].fsr_flash_area.fa_size;
        for (j = 0; j < fcb->f_ranges[i].fsr_sector_count; j++, sec++) {
            if (el_min_sec < el_max_sec) {
                if (sec >= el_min_sec && sec < el_max_sec) {
                    info->used += fcb->f_ranges[i].fsr_sector_size;
                }
            } else if (el_max_sec < el_min_sec) {
                if (sec < el_max_sec || sec >= el_min_sec) {
                    info->used += fcb->f_ranges[i].fsr_sector_size;
                }
            }
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
            if (fl->fl_watermark_sec < el_max_sec) {
                if (sec > fl->fl_watermark_sec && sec < el_max_sec) {
                    info->used_unread += fcb->f_ranges[i].fsr_sector_size;
                }
            } else if (fl->fl_watermark_sec > el_max_sec) {
                if (sec < el_max_sec || sec > fl->fl_watermark_sec) {
                    info->used_unread += fcb->f_ranges[i].fsr_sector_size;
                }
            }
#endif
            if (sec == el_max_sec) {
                info->used += fcb->f_active.fe_data_off;
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
                if (sec != fl->fl_watermark_sec) {
                    info->used_unread += fcb->f_active.fe_data_off;
                } else {
                    info->used_unread += fcb->f_active.fe_data_off -
                                         fl->fl_watermark_off;
                }
#endif
            } else {
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
                if (sec == fl->fl_watermark_sec) {
                    info->used_unread += fcb->f_ranges[i].fsr_sector_size -
                                         fl->fl_watermark_off;
                }
#endif
            }
        }
    }
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    if (fl->fl_watermark_sec == FCB2_SECTOR_OLDEST) {
        info->used_unread = 0xffffffff;
    }
#endif

    os_mutex_release(&fcb->f_mtx);

    return 0;
}
#endif

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
static int
log_fcb2_new_watermark_index(struct log *log, struct log_offset *log_off,
                             const void *dptr, uint16_t len)
{
    struct fcb2_entry *loc;
    struct fcb_log *fl;
    struct log_entry_hdr ueh;
    int rc;

    loc = (struct fcb2_entry *)dptr;
    fl = (struct fcb_log *)log->l_arg;

    rc = log_fcb2_read(log, loc, &ueh, 0, sizeof(ueh));

    if (rc != sizeof(ueh)) {
        return -1;
    }

    /* Set log watermark to end of this element */
    if (ueh.ue_index >= log_off->lo_index) {
        fl->fl_watermark_sec = loc->fe_sector;
        fl->fl_watermark_off = loc->fe_data_off + loc->fe_data_len;
        return 1;
    } else {
        return 0;
    }
}

static int
log_fcb2_set_watermark(struct log *log, uint32_t index)
{
    int rc;
    struct log_offset log_offset;
    struct fcb_log *fl;
    struct fcb2 *fcb;

    fl = (struct fcb_log *)log->l_arg;
    fcb = &fl->fl_fcb;

    log_offset.lo_arg = NULL;
    log_offset.lo_ts = 0;
    log_offset.lo_index = index;
    log_offset.lo_data_len = 0;

    /* Find where to start the walk, and set watermark accordingly */
    rc = log_fcb2_walk(log, log_fcb2_new_watermark_index, &log_offset);
    if (rc != 0) {
        goto done;
    }

    /* If there are no entries to read and the watermark has not been set */
    if (fl->fl_watermark_off == 0xffffffff) {
        fl->fl_watermark_sec = fcb->f_oldest_sec;
        fl->fl_watermark_off = 0;
    }

    return (0);
done:
    return (rc);
}
#endif

/**
 * Copies one log entry from source fcb to destination fcb
 *
 * @param log      Log this operation applies to
 * @param entry    FCB2 location for the entry being copied
 * @param dst_fcb  FCB2 area where data is getting copied to.
 *
 * @return 0 on success; non-zero on error
 */
static int
log_fcb2_copy_entry(struct log *log, struct fcb2_entry *entry,
                    struct fcb2 *dst_fcb)
{
    struct log_entry_hdr ueh;
    char data[LOG_PRINTF_MAX_ENTRY_LEN + LOG_BASE_ENTRY_HDR_SIZE +
              LOG_IMG_HASHLEN];
    uint16_t hdr_len;
    int dlen;
    int rc;
    struct fcb2 *fcb_tmp;

    rc = log_fcb2_read(log, entry, &ueh, 0, LOG_BASE_ENTRY_HDR_SIZE);
    if (rc != LOG_BASE_ENTRY_HDR_SIZE) {
        goto err;
    }

    hdr_len = log_hdr_len(&ueh);

    dlen = min(entry->fe_data_len, LOG_PRINTF_MAX_ENTRY_LEN + hdr_len);

    rc = log_fcb2_read(log, entry, data, 0, dlen);
    if (rc < 0) {
        goto err;
    }

    /*
     * Changing the fcb to be logged to be dst fcb.
     */
    fcb_tmp = log->l_arg;
    log->l_arg = dst_fcb;
    rc = log_fcb2_append(log, data, dlen);
    log->l_arg = fcb_tmp;
    if (rc) {
        goto err;
    }

err:
    return (rc);
}

/**
 * Copies log entries from source fcb to destination fcb
 *
 * @param log      Log this operation applies to
 * @param src_fcb  FCB2 area which is the source of data
 * @param dst_fcb  FCB2 area which is the target
 * @param from     FCB2 location where to start the copy
 *
 * @return 0 on success; non-zero on error
 */
static int
log_fcb2_copy(struct log *log, struct fcb2 *src_fcb, struct fcb2 *dst_fcb,
              struct fcb2_entry *from)
{
    struct fcb2_entry entry;
    int rc;

    rc = 0;

    entry = *from;
    do {
        rc = log_fcb2_copy_entry(log, &entry, dst_fcb);
        if (rc) {
            break;
        }
        rc = fcb2_getnext(src_fcb, &entry);
        if (rc == FCB2_ERR_NOVAR) {
            rc = 0;
            break;
        }
    } while (rc == 0);

    return (rc);
}

/**
 * Flushes the log while keeping the specified number of entries
 * using image scratch
 *
 * @param log      Log this operation applies to
 *
 * @return 0 on success; non-zero on error
 */
static int
log_fcb2_rtr_erase(struct log *log)
{
    struct fcb_log *fcb_log;
    struct fcb2 fcb_scratch;
    struct fcb2 *fcb;
    struct fcb2_entry entry;
    int rc;
    struct flash_sector_range range;
    int range_cnt;

    rc = 0;
    if (!log) {
        rc = -1;
        goto err;
    }

    fcb_log = log->l_arg;
    fcb = &fcb_log->fl_fcb;

    memset(&fcb_scratch, 0, sizeof(fcb_scratch));

    range_cnt = 1;
    if (flash_area_to_sector_ranges(FLASH_AREA_IMAGE_SCRATCH, &range_cnt,
                                    &range)) {
        goto err;
    }
    fcb_scratch.f_ranges = &range;
    fcb_scratch.f_sector_cnt = 1;
    fcb_scratch.f_range_cnt = 1;
    fcb_scratch.f_magic = 0x7EADBAE0;
    fcb_scratch.f_version = g_log_info.li_version;

    flash_area_erase(&range.fsr_flash_area, 0, range.fsr_flash_area.fa_size);
    rc = fcb2_init(&fcb_scratch);
    if (rc) {
        goto err;
    }

    /* Calculate offset of n-th last entry */
    rc = fcb2_offset_last_n(fcb, fcb_log->fl_entries, &entry);
    if (rc) {
        goto err;
    }

    /* Copy to scratch */
    rc = log_fcb2_copy(log, fcb, &fcb_scratch, &entry);
    if (rc) {
        goto err;
    }

    /* Flush log */
    rc = log_fcb2_flush(log);
    if (rc) {
        goto err;
    }

    memset(&entry, 0, sizeof(entry));
    rc = fcb2_getnext(&fcb_scratch, &entry);
    if (rc) {
        goto err;
    }
    /* Copy back from scratch */
    rc = log_fcb2_copy(log, &fcb_scratch, fcb, &entry);

err:
    return (rc);
}

const struct log_handler log_fcb_handler = {
    .log_type = LOG_TYPE_STORAGE,
    .log_read = log_fcb2_read,
    .log_read_mbuf = log_fcb2_read_mbuf,
    .log_append = log_fcb2_append,
    .log_append_body = log_fcb2_append_body,
    .log_append_mbuf = log_fcb2_append_mbuf,
    .log_append_mbuf_body = log_fcb2_append_mbuf_body,
    .log_walk = log_fcb2_walk,
    .log_flush = log_fcb2_flush,
#if MYNEWT_VAL(LOG_STORAGE_INFO)
    .log_storage_info = log_fcb2_storage_info,
#endif
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    .log_set_watermark = log_fcb2_set_watermark,
#endif
    .log_registered = log_fcb2_registered,
};

#endif
