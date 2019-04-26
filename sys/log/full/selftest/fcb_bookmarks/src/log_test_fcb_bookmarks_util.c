/*
 * Licensed to the Apache Software Foundation (ASF) under one * or more contributor license agreements.  See the NOTICE file
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

#include "log_test_util/log_test_util.h"
#include "log_test_fcb_bookmarks.h"

#define LTFBU_MAX_ENTRY_IDXS    20480
#define LTFBU_MAX_BODY_LEN      1024
#define LTFBU_MAX_BMARKS        1024

#define LTFBU_SECTOR_SIZE       (16 * 1024)

struct ltfbu_slice {
    const uint32_t *idxs;
    int count;
};

static struct ltfbu_cfg ltfbu_cfg;

static uint32_t ltfbu_entry_idxs[LTFBU_MAX_ENTRY_IDXS];
static int ltfbu_num_entry_idxs;

static struct fcb_log ltfbu_fcb_log;
static struct log ltfbu_log;

static struct fcb_log_bmark ltfbu_bmarks[LTFBU_MAX_BMARKS];

static struct flash_area ltfbu_fcb_areas[] = {
    [0] = {
        .fa_off = 0 * LTFBU_SECTOR_SIZE,
        .fa_size = LTFBU_SECTOR_SIZE,
    },
    [1] = {
        .fa_off = 1 * LTFBU_SECTOR_SIZE,
        .fa_size = LTFBU_SECTOR_SIZE,
    }
};

static int
ltfbu_max_entries(void)
{
    int entry_space;
    int entry_size;
    int len_size;
    int crc_size;

    if (ltfbu_cfg.body_len > 127) {
        len_size = 2;
    } else {
        len_size = 1;
    }
    crc_size = 1;

    /* "+ 1" for CRC. */
    entry_size = sizeof (struct log_entry_hdr) + ltfbu_cfg.body_len +
                 len_size + crc_size;
    entry_space = LTFBU_SECTOR_SIZE - 8;

    return entry_space / entry_size;
}

static void
ltfbu_expected_entry_range(int *first, int *count)
{
    int max_entries;
    int rollovers;

    max_entries = ltfbu_max_entries();
    rollovers = ltfbu_num_entry_idxs / max_entries;

    *first = rollovers * max_entries;
    *count = ltfbu_num_entry_idxs % max_entries;
}

static struct ltfbu_slice
ltfbu_expected_idxs(uint32_t start_idx)
{
    int first;
    int count;

    ltfbu_expected_entry_range(&first, &count);

    while (ltfbu_entry_idxs[first] < start_idx) {
        first++;
        count--;
    }
    
    return (struct ltfbu_slice) {
        .idxs = &ltfbu_entry_idxs[first],
        .count = count,
    };
}

static int
ltfbu_skip_amount(void)
{
    if (ltfbu_cfg.skip_mod == 0) {
        return 0;
    } else {
        return rand() % ltfbu_cfg.skip_mod;
    }
}

static void
ltfbu_skip(void)
{
    g_log_info.li_next_index += ltfbu_skip_amount();
}

static void
ltfbu_write_entry(void)
{
    uint8_t body[LTFBU_MAX_BODY_LEN];
    uint32_t idx;
    int rc;

    TEST_ASSERT_FATAL(ltfbu_num_entry_idxs < LTFBU_MAX_ENTRY_IDXS);

    ltfbu_skip();
    idx = g_log_info.li_next_index;

    memset(body, idx, ltfbu_cfg.body_len);

    rc = log_append_body(&ltfbu_log, 0, 255, LOG_ETYPE_BINARY, body,
                         ltfbu_cfg.body_len);
    TEST_ASSERT_FATAL(rc == 0);

    ltfbu_entry_idxs[ltfbu_num_entry_idxs++] = idx;
}

void
ltfbu_populate_log(int count)
{
    int i;

    for (i = 0; i < count; i++) {
        ltfbu_write_entry();
    }
}

struct ltfbu_walk_arg {
    struct ltfbu_slice slice;
    int cur;
};

static int
ltfbu_verify_log_walk(struct log *log, struct log_offset *log_offset,
                      const struct log_entry_hdr *hdr, void *dptr,
                      uint16_t len)
{
    struct ltfbu_walk_arg *arg;

    arg = log_offset->lo_arg;

    TEST_ASSERT_FATAL(arg->cur < arg->slice.count);
    TEST_ASSERT_FATAL(hdr->ue_index == arg->slice.idxs[arg->cur]);

    arg->cur++;

    return 0;
}

void
ltfbu_verify_log(uint32_t start_idx)
{
    struct ltfbu_walk_arg arg;
    struct ltfbu_slice slice;
    struct log_offset log_offset;
    int rc;

    slice = ltfbu_expected_idxs(start_idx);
    arg = (struct ltfbu_walk_arg) {
        .slice = slice,
        .cur = 0,
    };

    log_offset = (struct log_offset) {
        .lo_arg = &arg,
        .lo_index = start_idx,
        .lo_ts = 0,
        .lo_data_len = 0,
    };

    rc = log_walk_body(&ltfbu_log, ltfbu_verify_log_walk, &log_offset);
    TEST_ASSERT_FATAL(rc == 0);

    TEST_ASSERT_FATAL(arg.cur == slice.count);
}

void
ltfbu_init(const struct ltfbu_cfg *cfg)
{
    int rc;
    int i;

    /* Ensure tests are repeatable. */
    srand(0);

    ltfbu_cfg = *cfg;
    ltfbu_num_entry_idxs = 0;

    ltfbu_fcb_log = (struct fcb_log) {
        .fl_fcb.f_scratch_cnt = 1,
        .fl_fcb.f_sectors = ltfbu_fcb_areas,
        .fl_fcb.f_sector_cnt = sizeof(ltfbu_fcb_areas) / sizeof(ltfbu_fcb_areas[0]),
        .fl_fcb.f_magic = 0x7EADBADF,
        .fl_fcb.f_version = 0,
    };

    for (i = 0; i < ltfbu_fcb_log.fl_fcb.f_sector_cnt; i++) {
        rc = flash_area_erase(&ltfbu_fcb_areas[i], 0,
                              ltfbu_fcb_areas[i].fa_size);
        TEST_ASSERT_FATAL(rc == 0);
    }
    rc = fcb_init(&ltfbu_fcb_log.fl_fcb);
    TEST_ASSERT_FATAL(rc == 0);

    if (cfg->bmark_count > 0) {
        fcb_log_init_bmarks(&ltfbu_fcb_log, ltfbu_bmarks, cfg->bmark_count);
    }

    log_register("log", &ltfbu_log, &log_fcb_handler, &ltfbu_fcb_log,
                 LOG_SYSLEVEL);
}

void
ltfbu_test_once(const struct ltfbu_cfg *cfg)
{
    uint32_t start_idx;
    int i;

    ltfbu_init(cfg);

    /* Do this three times:
     * 1. Write a bunch of entries to the log.
     * 2. Walk the log, starting from various entry indices
     * 3. Verify results of walk:
     *     a. All expected entries are visited.
     *     b. No extra entries are visited.
     *
     * This procedure is repeated three times to ensure that the FCB is rotated
     * between walks.
     */
    for (i = 0; i < 3; i++) {
        ltfbu_populate_log(cfg->pop_count);

        start_idx = 0;
        while (start_idx < ltfbu_entry_idxs[ltfbu_num_entry_idxs - 1]) {
            ltfbu_verify_log(start_idx);
            start_idx += ltfbu_skip_amount() + 1;
        }
    }
}
