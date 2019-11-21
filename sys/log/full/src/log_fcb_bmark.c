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

#include "log/log_fcb.h"

void
log_fcb_init_bmarks(struct fcb_log *fcb_log,
                    struct log_fcb_bmark *buf, int bmark_count)
{
    fcb_log->fl_bset = (struct log_fcb_bset) {
        .lfs_bmarks = buf,
        .lfs_cap = bmark_count,
    };
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
        bset->lfs_next = bset->lfs_size;
    }
}

void
log_fcb_clear_bmarks(struct fcb_log *fcb_log)
{
    fcb_log->fl_bset.lfs_size = 0;
    fcb_log->fl_bset.lfs_next = 0;
}

const struct log_fcb_bmark *
log_fcb_closest_bmark(const struct fcb_log *fcb_log, uint32_t index)
{
    const struct log_fcb_bmark *closest;
    const struct log_fcb_bmark *bmark;
    uint32_t min_diff;
    uint32_t diff;
    int i;

    min_diff = UINT32_MAX;
    closest = NULL;

    for (i = 0; i < fcb_log->fl_bset.lfs_size; i++) {
        bmark = &fcb_log->fl_bset.lfs_bmarks[i];
        if (bmark->lfb_index <= index) {
            diff = index - bmark->lfb_index;
            if (diff < min_diff) {
                min_diff = diff;
                closest = bmark;
            }
        }
    }

    return closest;
}

#if MYNEWT_VAL(LOG_FCB)
void
log_fcb_add_bmark(struct fcb_log *fcb_log, const struct fcb_entry *entry,
                  uint32_t index)
#elif MYNEWT_VAL(LOG_FCB2)
void
log_fcb_add_bmark(struct fcb_log *fcb_log, const struct fcb2_entry *entry,
                  uint32_t index)
#endif
{
    struct log_fcb_bset *bset;

    bset = &fcb_log->fl_bset;

    if (bset->lfs_cap == 0) {
        return;
    }

    bset->lfs_bmarks[bset->lfs_next] = (struct log_fcb_bmark) {
        .lfb_entry = *entry,
        .lfb_index = index,
    };

    if (bset->lfs_size < bset->lfs_cap) {
        bset->lfs_size++;
    }

    bset->lfs_next++;
    if (bset->lfs_next >= bset->lfs_cap) {
        bset->lfs_next = 0;
    }
}

#endif /* MYNEWT_VAL(LOG_FCB_BOOKMARKS) */
