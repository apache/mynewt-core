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

#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
static int
log_fcb_init_sector_bmarks(struct fcb_log *fcb_log)
{
    int rc = 0;
    int i = 0;
    int j = 0;
    struct log_fcb_bset *bset = &fcb_log->fl_bset;
    struct log_entry_hdr ueh = {0};
#if MYNEWT_VAL(LOG_FCB)
    struct fcb_entry loc = {0};
    struct flash_area *fa = NULL;

    rc = fcb_getnext(&fcb_log->fl_fcb, &loc);
    if (rc) {
        return SYS_EOK;
    }
#else
    struct flash_sector_range *range = NULL;
    struct fcb2_entry loc = {0};

    rc = fcb2_getnext(&fcb_log->fl_fcb, &loc);
    if (rc) {
        return SYS_EOK;
    }
#endif

    /* Start adding a bookmark from the end of array just before
     * non-sector bookmarks
     */
    bset->lfs_next_sect = bset->lfs_sect_cap - 1;
    for (i = 0; i < bset->lfs_sect_cap; i++) {
        rc = log_read_hdr(fcb_log->fl_log, &loc, &ueh);
        if (rc) {
            /* Read failed, don't add a bookmark, done adding bookmarks */
            rc = SYS_EOK;
            break;
        }

        rc = log_fcb_add_bmark(fcb_log, &loc, ueh.ue_index, true);
        if (rc) {
            return rc;
        }

#if MYNEWT_VAL(LOG_FCB)
        j = 0;
        fa = loc.fe_area;
        /* Keep skipping sectors until lfs_sect_bmark_itvl is reached */
        do {
            fa = fcb_getnext_area(&fcb_log->fl_fcb, fa);
            if (!fa) {
                break;
            }
            j++;
        } while (j < bset->lfs_sect_bmark_itvl);

        if (!fa) {
            break;
        }

        /* First entry in the next area */
        rc = fcb_getnext_in_area(&fcb_log->fl_fcb, fa, &loc);
        if (rc) {
            rc = SYS_EOK;
            break;
        }
#else
        j = 0;

        /* Keep skipping rangess until lfs_sect_bmark_itvl is reached */
        do {
            /* Get next range */
            range = fcb2_getnext_range(&fcb_log->fl_fcb, &loc);
            if (!range) {
                break;
            }
            j++;
        } while (j < bset->lfs_sect_bmark_itvl);

        if (!range) {
            break;
        }

        /* First entry in the next area */
        rc = fcb2_getnext_in_area(&fcb_log->fl_fcb, range, &loc);
        if (rc) {
            rc = SYS_EOK;
            break;
        }
#endif
    }

    return rc;
}
#endif

int
log_fcb_init_bmarks(struct fcb_log *fcb_log,
                    struct log_fcb_bmark *buf, int avl_bmark_cnt,
                    bool en_sect_bmarks)
{
    struct log_fcb_bset *bset = &fcb_log->fl_bset;
    /* Required bookmark count */
    int reqd_bmark_cnt = MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS);

    (void)reqd_bmark_cnt;
    if (!bset || !buf || !avl_bmark_cnt) {
        return SYS_EINVAL;
    }

    memset(buf, 0, sizeof(struct log_fcb_bmark) * avl_bmark_cnt);

    *bset = (struct log_fcb_bset) {
        .lfs_bmarks = buf,
        .lfs_cap = avl_bmark_cnt,
        .lfs_en_sect_bmarks = en_sect_bmarks
    };

#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
    if (en_sect_bmarks) {
        /* Default sector bookmark interval is 1 */
        bset->lfs_sect_bmark_itvl = 1;
        reqd_bmark_cnt += fcb_log->fl_fcb.f_sector_cnt;
        /* Make sure we have allocated the exact number of bookmarks
         * compare available bookmark count Vs required bookmark count
         */
        if (avl_bmark_cnt < reqd_bmark_cnt) {
            /* Not enough space allocated for sector bookmarks,
             * add bookmarks at sector intervals
             */
            bset->lfs_sect_bmark_itvl =
                fcb_log->fl_fcb.f_sector_cnt / avl_bmark_cnt;
            bset->lfs_sect_cap = avl_bmark_cnt -
                                 MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS);
        } else {
            bset->lfs_sect_cap = fcb_log->fl_fcb.f_sector_cnt;
        }

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
#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
    if (fcb_log->fl_bset.lfs_en_sect_bmarks) {
        fcb_log->fl_bset.lfs_next_sect = fcb_log->fl_bset.lfs_sect_cap - 1;
    }
#endif
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

const struct log_fcb_bmark *
log_fcb_closest_bmark(const struct fcb_log *fcb_log, uint32_t index,
                      int *min_diff)
{
    const struct log_fcb_bmark *closest;
    const struct log_fcb_bmark *bmark;
    uint32_t diff;
    int i = 0;
    uint32_t start_idx = 0;

    *min_diff = -1;
    closest = NULL;

    if (!fcb_log->fl_bset.lfs_bmarks) {
        return closest;
    }

#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
#if MYNEWT_VAL(LOG_FCB)
    /* Reason for this is that we want to iterate to the end of the
     * bmarks sector section of the array where a bookmark is valid,
     * this is only for the case where the sector bookmarks have not
     * been fully filled up, if they are filled up, we can iterate
     * normally to the end of the array
     */
    if (!fcb_log->fl_bset.lfs_bmarks[i].lfb_entry.fe_area &&
        fcb_log->fl_bset.lfs_next_sect < (fcb_log->fl_bset.lfs_sect_cap - 1)) {
        start_idx = fcb_log->fl_bset.lfs_next_sect + 1;
    }
#elif MYNEWT_VAL(LOG_FCB2)
    if (!fcb_log->fl_bset.lfs_bmarks[i].lfb_entry.fe_range &&
        fcb_log->fl_bset.lfs_next_sect < (fcb_log->fl_bset.lfs_sect_cap - 1)) {
        start_idx = fcb_log->fl_bset.lfs_next_sect + 1;
    }
#endif
#endif

    /* This works for both sector as well as non-sector bmarks
     * because we calculate the min diff and iterate to the end
     * of the bmarks array keeping track of min diff
     */
    for (i = start_idx; i < (start_idx + fcb_log->fl_bset.lfs_size); i++) {
        bmark = &fcb_log->fl_bset.lfs_bmarks[i];
#if MYNEWT_VAL(LOG_FCB)
        if (!fcb_log->fl_bset.lfs_bmarks[i].lfb_entry.fe_area) {
#if !MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
            /* Empty non-sector bookmark, nothing more to do
             * Previous closest bookmark is the closest one */
            break;
#endif
        }
#elif MYNEWT_VAL(LOG_FCB2)
        if (!fcb_log->fl_bset.lfs_bmarks[i].lfb_entry.fe_range) {
#if !MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
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
                MODLOG_DEBUG(LOG_MODULE_DEFAULT,
                             "index: %u, closest bmark idx: %u, \n",
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
static int
log_fcb_insert_sect_bmark(struct fcb_log *fcb_log, struct fcb_entry *entry,
                          uint32_t index)
#elif MYNEWT_VAL(LOG_FCB2)
static int
log_fcb_insert_sect_bmark(struct fcb_log *fcb_log, struct fcb2_entry *entry,
                          uint32_t index)
#endif
{
    struct log_fcb_bset *bset;

    bset = &fcb_log->fl_bset;

    if (bset->lfs_size < fcb_log->fl_bset.lfs_sect_cap) {
        bset->lfs_bmarks[bset->lfs_next_sect] = (struct log_fcb_bmark) {
            .lfb_entry = *entry,
            .lfb_index = index,
        };

        bset->lfs_size++;
        bset->lfs_next_sect--;
        if (bset->lfs_next_sect >= fcb_log->fl_bset.lfs_sect_cap) {
            bset->lfs_next_sect = fcb_log->fl_bset.lfs_sect_cap - 1;
        }
    }

    return 0;
}
#endif

#if MYNEWT_VAL(LOG_FCB)
static int
log_fcb_replace_non_sect_bmark(struct fcb_log *fcb_log, struct fcb_entry *entry,
                               uint32_t index, int pos)
#elif MYNEWT_VAL(LOG_FCB2)
static int
log_fcb_replace_non_sect_bmark(struct fcb_log *fcb_log, struct fcb2_entry *entry,
                               uint32_t index, int pos)
#endif
{
    int i = 0;
    struct log_fcb_bset *bset = &fcb_log->fl_bset;

#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
    if (bset->lfs_en_sect_bmarks) {
        for (i = bset->lfs_sect_cap;
             i < (bset->lfs_non_sect_size + bset->lfs_sect_cap);
             i++) {
            if (index == bset->lfs_bmarks[i].lfb_index) {
                /* If index matches, no need to replace */
                return SYS_EALREADY;
            }
        }
    } else
#endif
    {
        for (i = 0; i < bset->lfs_non_sect_size; i++) {
            if (index == bset->lfs_bmarks[i].lfb_index) {
                /* If index matches, no need to replace */
                return SYS_EALREADY;
            }
        }
    }

    bset->lfs_bmarks[pos] = (struct log_fcb_bmark) {
        .lfb_entry = *entry,
        .lfb_index = index,
    };

    return SYS_EOK;
}

#if MYNEWT_VAL(LOG_FCB)
int
log_fcb_add_bmark(struct fcb_log *fcb_log, struct fcb_entry *entry,
                  uint32_t index, bool sect_bmark)
#elif MYNEWT_VAL(LOG_FCB2)
int
log_fcb_add_bmark(struct fcb_log *fcb_log, struct fcb2_entry *entry,
                  uint32_t index, bool sect_bmark)
#endif
{
    struct log_fcb_bset *bset = &fcb_log->fl_bset;
    int rc = 0;

    if (bset->lfs_cap == 0) {
        return SYS_ENOMEM;
    }

#if MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS)
    if (sect_bmark & bset->lfs_en_sect_bmarks) {
        rc = log_fcb_insert_sect_bmark(fcb_log, entry, index);
        if (rc) {
            MODLOG_DEBUG(LOG_MODULE_DEFAULT, "insert bmark failure: %u\n",
                         index);
        }
        MODLOG_DEBUG(LOG_MODULE_DEFAULT, "insert bmark index: %u, pos: %u\n",
                     index, bset->lfs_next_sect);
    } else {
        /* Replace oldest non-sector bmark */
        rc = log_fcb_replace_non_sect_bmark(fcb_log, entry, index,
                                            bset->lfs_next_non_sect +
                                            (bset->lfs_en_sect_bmarks ?
                                             bset->lfs_sect_cap : 0));
        if (rc == SYS_EOK) {
            MODLOG_DEBUG(LOG_MODULE_DEFAULT, "replace bmark index: %u, pos: %u\n",
                         index, bset->lfs_next_non_sect +
                         (bset->lfs_en_sect_bmarks ?
                          bset->lfs_sect_cap : 0));

            if (bset->lfs_non_sect_size < MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS)) {
                bset->lfs_non_sect_size++;
                bset->lfs_size++;
            }

            bset->lfs_next_non_sect = (bset->lfs_next_non_sect + 1) %
                                      MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS);
        }
    }
#else
    if (!sect_bmark) {
        if (bset->lfs_size >= MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS)) {
            /* Replace oldest non-sector bmark */
            rc = log_fcb_replace_non_sect_bmark(fcb_log, entry, index,
                                                bset->lfs_next_non_sect);
            if (rc == SYS_EOK) {
                MODLOG_DEBUG(LOG_MODULE_DEFAULT, "replace bmark index: %u, pos: %u\n",
                             index, bset->lfs_next_non_sect);
                bset->lfs_next_non_sect = (bset->lfs_next_non_sect + 1) %
                                          MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS);
            }
        } else {
            rc = log_fcb_replace_non_sect_bmark(fcb_log, entry, index,
                                                bset->lfs_size);
            if (rc == SYS_EOK) {
                MODLOG_DEBUG(LOG_MODULE_DEFAULT, "replace bmark index: %u, pos: %u\n",
                             index, bset->lfs_size);
                if (!bset->lfs_size) {
                    /* First non-sector bmark position */
                    bset->lfs_next_non_sect = 0;
                }
                bset->lfs_size++;
            }
        }

        assert(bset->lfs_size <= MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS));
    }
#endif
    return SYS_EOK;
}

#endif /* MYNEWT_VAL(LOG_FCB_BOOKMARKS) */
