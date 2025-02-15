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

#include "os/mynewt.h"

#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
#include "log/log.h"
#include "log/log_fcb.h"
#include <console/console.h>

#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
static int
log_fcb_init_sector_bmarks(struct fcb_log *fcb_log)
{
    int rc = 0;
    int i = 0;
    struct fcb_entry loc = {0};
    struct log_entry_hdr ueh = {0};
    struct flash_area *fa = NULL;

    rc = fcb_getnext(&fcb_log->fl_fcb, &loc);
    if (rc) {
        return -1;
    }

    for (i = 0; i < fcb_log->fl_fcb.f_sector_cnt; i++) {
        rc = log_read_hdr(fcb_log->fl_log, &loc, &ueh);
        if (rc) {
            /* Read failed, don't add a bookmark, done adding bookmarks */
            rc = SYS_EOK;
            break;
        }

        log_fcb_add_bmark(fcb_log, &loc, ueh.ue_index, true);

        fa = fcb_getnext_area(&fcb_log->fl_fcb, loc.fe_area);
        if (!fa) {
            break;
        }

        /* First entry in the next area */
        rc = fcb_getnext_in_area(&fcb_log->fl_fcb, fa, &loc);
        if (rc) {
            break;
        }
    }

    return rc;
}
#endif

int
log_fcb_init_bmarks(struct fcb_log *fcb_log,
                    struct log_fcb_bmark *buf, int bmark_count,
                    bool en_sect_bmarks)
{
    memset(&fcb_log->fl_bset, 0, sizeof(fcb_log->fl_bset));
    memset(buf, 0, sizeof(struct log_fcb_bmark) * bmark_count);

    fcb_log->fl_bset = (struct log_fcb_bset) {
        .lfs_bmarks = buf,
        .lfs_cap = bmark_count,
        .lfs_en_sect_bmarks = en_sect_bmarks
    };

#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
    if (en_sect_bmarks) {
        return log_fcb_init_sector_bmarks(fcb_log);
    }
#endif
    return 0;
}

void
log_fcb_rotate_bmarks(struct fcb_log *fcb_log)
{
    int i;
    struct log_fcb_bmark *bmark;
    struct log_fcb_bset *bset;

    bset = &fcb_log->fl_bset;

    for (i = 0; i < bset->lfs_size; i++) {
        bmark = &bset->lfs_bmarks[i];
#if MYNEWT_VAL(LOG_FCB)
        if (bmark->lfb_entry.fe_area != fcb_log->fl_fcb.f_oldest) {
            continue;
        }
#endif
#if MYNEWT_VAL(LOG_FCB2)
        if (bmark->lfb_entry.fe_sector != fcb_log->fl_fcb.f_oldest_sec) {
            continue;
        }
#endif
        if (i != bset->lfs_size - 1) {
            *bmark = bset->lfs_bmarks[bset->lfs_size - 1];
            i--;
        }
        bset->lfs_size--;
    }
}

void
log_fcb_clear_bmarks(struct fcb_log *fcb_log)
{
    fcb_log->fl_bset.lfs_size = 0;
    fcb_log->fl_bset.lfs_next_non_sect = 0;
    fcb_log->fl_bset.lfs_non_sect_size = 0;
    memset(fcb_log->fl_bset.lfs_bmarks, 0,
           sizeof(struct log_fcb_bmark) *
           fcb_log->fl_bset.lfs_cap);
}

struct log_fcb_bmark *
log_fcb_get_bmarks(struct log *log, uint32_t *bmarks_size)
{
    struct fcb_log *fcb_log = (struct fcb_log *)log->l_arg;

    *bmarks_size = fcb_log->fl_bset.lfs_cap;

    return fcb_log->fl_bset.lfs_bmarks;
}

struct log_fcb_bmark *
log_fcb_closest_bmark(struct fcb_log *fcb_log, uint32_t index,
                      int *min_diff)
{
    struct log_fcb_bmark *closest;
    struct log_fcb_bmark *bmark;
    uint32_t diff;
    int i;

    *min_diff = -1;
    closest = NULL;

    /* This works for both sector as well as non-sector bmarks
     * because we calculate the min diff and iterate to the end
     * of the bmarks array keeping track of min diff
     */
    for (i = 0; i < fcb_log->fl_bset.lfs_size; i++) {
        bmark = &fcb_log->fl_bset.lfs_bmarks[i];
#if MYNEWT_VAL(LOG_FCB)
        if (!fcb_log->fl_bset.lfs_bmarks[i].lfb_entry.fe_area) {
#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
            if (i < fcb_log->fl_fcb.f_sector_cnt) {
                /* Jump to the non-sector bookmarks since sector
                 * bookmarks are empty here on
                 */
                i = fcb_log->fl_fcb.f_sector_cnt - 1;
                continue;
            }
#else
            /* Empty non-sector bookmark, nothing more to do
             * Previous closest bookmark is the closest one */
            break;
#endif
        }
#elif MYNEWT_VAL(LOG_FCB2)
        if (!fcb_log->fl_bset.lfs_bmarks[i].lfb_entry.fe_range) {
#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
            if (i < fcb_log->fl_fcb.f_sector_cnt) {
                /* Jump to the non-sector bookmarks since sector
                 * bookmarks are empty here on
                 */
                i = fcb_log->fl_fcb.f_sector_cnt - 1;
                continue;
            }
#else
            /* Empty non-sector bookmark, nothing more to do
             * Previous closest bookmark is the closest one */
            break;
#endif
        }
#endif
        if (bmark->lfb_index <= index) {
            diff = index - bmark->lfb_index;
            if (diff < *min_diff) {
                *min_diff = diff;
                closest = bmark;
                MODLOG_DEBUG(LOG_MODULE_DEFAULT, "index: %u, closest bmark idx: %u, \n",
                             (unsigned int)index,
                             (unsigned int)bmark->lfb_index);
                /* We found the exact match, no need to keep searching for a
                 * better match
                 */
                if (*min_diff == 0) {
                    break;
                }
            }
        }
    }

    return closest;
}

#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
#if MYNEWT_VAL(LOG_FCB)
static void
log_fcb_insert_sect_bmark(struct fcb_log *fcb_log, struct fcb_entry *entry,
                          uint32_t index, int pos)
#elif MYNEWT_VAL(LOG_FCB2)
static void
log_fcb_insert_sect_bmark(struct fcb_log *fcb_log, fcb2_entry *entry,
                          uint32_t index, int pos)
#endif
{
    struct log_fcb_bset *bset;

    bset = &fcb_log->fl_bset;

    if (bset->lfs_size && bset->lfs_size > pos && pos != (bset->lfs_cap - 1)) {
        memmove(&bset->lfs_bmarks[pos + 1], &bset->lfs_bmarks[pos],
                sizeof(bset->lfs_bmarks[0]) * (bset->lfs_size - pos));
    }

    bset->lfs_bmarks[pos] = (struct log_fcb_bmark) {
        .lfb_entry = *entry,
        .lfb_index = index,
    };

    if (bset->lfs_size < bset->lfs_cap) {
        bset->lfs_size++;
    }
}
#endif

#if MYNEWT_VAL(LOG_FCB)
static void
log_fcb_replace_non_sect_bmark(struct fcb_log *fcb_log, struct fcb_entry *entry,
                               uint32_t index, int pos)
#elif MYNEWT_VAL(LOG_FCB2)
static void
log_fcb_replace_non_sect_bmark(struct fcb_log *fcb_log, struct fcb2_entry *entry,
                               uint32_t index, int pos)
#endif
{
    int i = 0;
    struct log_fcb_bset *bset = &fcb_log->fl_bset;

#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
    if (bset->lfs_en_sect_bmarks) {
        for (i = fcb_log->fl_fcb.f_sector_cnt;
             i < (bset->lfs_non_sect_size + fcb_log->fl_fcb.f_sector_cnt);
             i++) {
            if (index == bset->lfs_bmarks[i].lfb_index) {
                /* If index matches, no need to replace */
                return;
            }
        }
    } else
#endif
    {
        for (i = 0; i < bset->lfs_non_sect_size; i++) {
            if (index == bset->lfs_bmarks[i].lfb_index) {
                /* If index matches, no need to replace */
                return;
            }
        }
    }

    bset->lfs_bmarks[pos] = (struct log_fcb_bmark) {
        .lfb_entry = *entry,
        .lfb_index = index,
    };
}

#if MYNEWT_VAL(LOG_FCB)
void
log_fcb_add_bmark(struct fcb_log *fcb_log, struct fcb_entry *entry,
                  uint32_t index, bool sect_bmark)
#elif MYNEWT_VAL(LOG_FCB2)
void
log_fcb_add_bmark(struct fcb_log *fcb_log, struct fcb2_entry *entry,
                  uint32_t index, bool sect_bmark)
#endif
{
    struct log_fcb_bset *bset = &fcb_log->fl_bset;
    int i = 0;
    int pos = 0;
    bool en_sect_bmark = false;

    (void)i;
    (void)pos;

    if (bset->lfs_cap == 0) {
        return;
    }

    en_sect_bmark = sect_bmark & bset->lfs_en_sect_bmarks;
    (void)en_sect_bmark;

#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
    if (en_sect_bmark) {
        pos = bset->lfs_size;
        for (i = 0; i < bset->lfs_size; i++) {
            if (index > bset->lfs_bmarks[i].lfb_index) {
                pos = i;
                break;
            }
        }
        log_fcb_insert_sect_bmark(fcb_log, entry, index, pos);
        MODLOG_DEBUG(LOG_MODULE_DEFAULT, "insert bmark index: %u, pos: %u, \n",
                     (unsigned int)index,
                     (unsigned int)pos);
    } else {
        /* Replace oldest non-sector bmark */
        log_fcb_replace_non_sect_bmark(fcb_log, entry, index,
                                       bset->lfs_next_non_sect +
                                       (bset->lfs_en_sect_bmarks ?
                                        fcb_log->fl_fcb.f_sector_cnt : 0));
        MODLOG_DEBUG(LOG_MODULE_DEFAULT, "replace bmark index: %u, pos: %u, \n",
                     (unsigned int)index,
                     (unsigned int)bset->lfs_next_non_sect +
                     (bset->lfs_en_sect_bmarks ?
                      fcb_log->fl_fcb.f_sector_cnt : 0));

        if (bset->lfs_non_sect_size < MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS)) {
            bset->lfs_non_sect_size++;
            bset->lfs_size++;
        }

        bset->lfs_next_non_sect = (bset->lfs_next_non_sect + 1) %
                                  MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS);
    }
#else
    if (!sect_bmark) {
        if (bset->lfs_size >= MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS)) {
            /* Replace oldest non-sector bmark */
            log_fcb_replace_non_sect_bmark(fcb_log, entry, index,
                                           bset->lfs_next_non_sect);
            MODLOG_DEBUG(LOG_MODULE_DEFAULT, "replace bmark index: %u, pos: %u, \n",
                         (unsigned int)index,
                         (unsigned int)bset->lfs_next_non_sect);
            bset->lfs_next_non_sect = (bset->lfs_next_non_sect + 1) %
                                      MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS);
        } else {
            log_fcb_replace_non_sect_bmark(fcb_log, entry, index,
                                           bset->lfs_size);
            MODLOG_DEBUG(LOG_MODULE_DEFAULT, "replace bmark index: %u, pos: %u, \n",
                         (unsigned int)index,
                         bset->lfs_size);
            if (!bset->lfs_size) {
                /* First non-sector bmark position */
                bset->lfs_next_non_sect = 0;
            }
            bset->lfs_size++;
        }

        assert(bset->lfs_size <= MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS));
    }
#endif
}

#endif /* MYNEWT_VAL(LOG_FCB_BOOKMARKS) */
