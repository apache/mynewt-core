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

#include <os/mynewt.h>
#include <log/log.h>
#include <fcb/fcb.h>
#include <simple_fcb_log/simple_fcb_log.h>

#if MYNEWT_VAL(LOG_FCB)
int
simple_fcb_log_register(struct simple_fcb_log *simplelog, int flash_area_id, const char *log_name,
                        uint32_t fcb_magic)
{
    const struct flash_area *fa;
    struct fcb *fcbp;
    int sector_count;
    int rc;

    assert(simplelog != NULL);

    fcbp = &simplelog->fcb_log.fl_fcb;

    if (flash_area_open(flash_area_id, &fa)) {
        return SYS_EUNKNOWN;
    }

    flash_area_to_sectors(flash_area_id, &sector_count, NULL);

    if (simplelog->sector_count == 0) {
        simplelog->sectors = calloc(sector_count, sizeof(*simplelog->sectors));
        if (simplelog->sectors) {
            simplelog->sector_count = sector_count;
        }
    }

    if (simplelog->sectors == 0) {
        rc = SYS_ENOMEM;
    } else {
        flash_area_to_sectors(flash_area_id, &sector_count, simplelog->sectors);
        simplelog->fcb_log.fl_entries = 0;
        fcbp->f_magic = fcb_magic;
        fcbp->f_version = g_log_info.li_version;
        fcbp->f_sector_cnt = sector_count;
        fcbp->f_scratch_cnt = 0;
        fcbp->f_sectors = simplelog->sectors;

        rc = fcb_init(fcbp);
        if (rc) {
            flash_area_erase(fa, 0, fa->fa_size);
            rc = fcb_init(fcbp);
            if (rc) {
                return rc;
            }
        }

        rc = log_register(log_name, &simplelog->log, &log_fcb_handler,
                          &simplelog->fcb_log, LOG_SYSLEVEL);
    }
    return rc;
}

#else

int
simple_fcb_log_register(struct simple_fcb_log *simplelog, int flash_area_id, const char *log_name,
                        uint32_t fcb_magic)
{
    const struct flash_area *fa;
    struct fcb2 *fcbp;
    int range_count = 1;
    int rc;

    assert(simplelog != NULL);

    fcbp = &simplelog->fcb_log.fl_fcb;

    if (flash_area_open(flash_area_id, &fa)) {
        return SYS_EUNKNOWN;
    }

    flash_area_to_sector_ranges(flash_area_id, &range_count, &simplelog->sectors);

    fcbp->f_magic = fcb_magic;
    fcbp->f_version = g_log_info.li_version;
    fcbp->f_sector_cnt = simplelog->sectors.fsr_sector_count;
    fcbp->f_range_cnt = 1;
    fcbp->f_ranges = &simplelog->sectors;

    rc = fcb2_init(fcbp);
    if (rc) {
        flash_area_erase(fa, 0, fa->fa_size);
        rc = fcb2_init(fcbp);
        if (rc) {
            return rc;
        }
    }

    rc = log_register(log_name, &simplelog->log, &log_fcb_handler,
                      &simplelog->fcb_log, LOG_SYSLEVEL);

    return rc;
}

#endif

#if MYNEWT_VAL(SIMPLE_FCB_LOG_0)
static struct simple_fcb_log simple_fcb_log_0;

#if MYNEWT_VAL(SIMPLE_FCB_LOG_0_BOOKMARKS)
#if MYNEWT_VAL(SIMPLE_FCB_LOG_0_BOOKMARK_COUNT)
/* Bookmark count is provided in syscfg */
static struct log_fcb_bmark simple_fcb_log_0_bookmarks[MYNEWT_VAL(SIMPLE_FCB_LOG_0_BOOKMARK_COUNT)];
#elif MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS) == 0
/* Sector bookmarks are not enabled, reserve space based on LOG_FCB_NUM_ABS_BOOKMARKS */
static struct log_fcb_bmark simple_fcb_log_0_bookmarks[MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS)];
#else
/*
 * Bookmarks are enabled including sector bookmarks but size is not set,
 * bookmarks array will be placed on heap.
 */
static struct log_fcb_bmark *simple_fcb_log_0_bookmarks;
#endif
#endif

void
simple_fcb_log_0_init(void)
{
    simple_fcb_log_register(&simple_fcb_log_0, MYNEWT_VAL(SIMPLE_FCB_LOG_0_FLASH_AREA),
                            MYNEWT_VAL(SIMPLE_FCB_LOG_0_NAME), MYNEWT_VAL(SIMPLE_FCB_LOG_0_FCB_MAGIC));

#if MYNEWT_VAL(SIMPLE_FCB_LOG_0_BOOKMARKS)
    int bookmark_count;

#if MYNEWT_VAL(SIMPLE_FCB_LOG_0_BOOKMARK_COUNT) || MYNEWT_VAL(LOG_FCB_SECTOR_BOOKMARKS) == 0
    bookmark_count = ARRAY_SIZE(simple_fcb_log_0_bookmarks);
#else
    bookmark_count = (simple_fcb_log_0.sector_count + MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS));
    simple_fcb_log_0_bookmarks = malloc(bookmark_count * sizeof(simple_fcb_log_0_bookmarks[0]));
#endif
    if (simple_fcb_log_0_bookmarks != NULL) {
        log_fcb_init_bmarks(&simple_fcb_log_0.fcb_log, simple_fcb_log_0_bookmarks, bookmark_count,
                            bookmark_count > MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS));
    }
#endif
}
#endif
