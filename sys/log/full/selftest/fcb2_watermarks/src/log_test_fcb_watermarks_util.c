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
#include "log_test_fcb_watermarks.h"

#define LTFWU_MAX_ENTRY_IDXS    20480
#define LTFWU_MAX_BODY_LEN      1024

#define LTFWU_SECTOR_SIZE       (2048)

static struct ltfwu_cfg ltfwu_cfg;

static struct fcb_log ltfwu_fcb_log;
static struct log ltfwu_log;

static struct flash_sector_range ltfwu_fcb_range = {
    .fsr_flash_area = {
        .fa_off = 0 * LTFWU_SECTOR_SIZE,
        .fa_size = 8 * LTFWU_SECTOR_SIZE,
    },
    .fsr_sector_count = 8,
    .fsr_sector_size = LTFWU_SECTOR_SIZE,
    .fsr_align = MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE)
};

static int
ltfwu_skip_amount(void)
{
    if (ltfwu_cfg.skip_mod == 0) {
        return 0;
    } else {
        return rand() % ltfwu_cfg.skip_mod;
    }
}

static void
ltfwu_skip(void)
{
    g_log_info.li_next_index += ltfwu_skip_amount();
}

static void
ltfwu_write_entry(void)
{
    uint8_t body[LTFWU_MAX_BODY_LEN];
    uint32_t idx;
    int rc;

    ltfwu_skip();
    idx = g_log_info.li_next_index;

    memset(body, idx, ltfwu_cfg.body_len);

    rc = log_append_body(&ltfwu_log, 0, 255, LOG_ETYPE_BINARY, body,
                         ltfwu_cfg.body_len);
    TEST_ASSERT_FATAL(rc == 0);
}

static void
ltfwu_populate_log(int count)
{
    int i;

    for (i = 0; i < count; i++) {
        ltfwu_write_entry();
    }
}

struct ltfwu_walk_arg {
    int start_idx;
    int amount_before;
    int amount_total;
};

static int
ltfwu_verify_log_walk(struct log *log, struct log_offset *log_offset,
                      const struct log_entry_hdr *hdr, const void *dptr,
                      uint16_t len)
{
    struct ltfwu_walk_arg *arg;
    struct log_storage_info info;
    int rc;

    arg = log_offset->lo_arg;
    if (arg->start_idx == 0) {
        arg->start_idx = hdr->ue_index;
    }

    rc = log_set_watermark(&ltfwu_log, hdr->ue_index);
    TEST_ASSERT_FATAL(rc == 0);

    rc = log_storage_info(&ltfwu_log, &info);
    TEST_ASSERT_FATAL(rc == 0);

    TEST_ASSERT(info.size - info.used_unread >= arg->amount_before);
    TEST_ASSERT(info.used_unread < arg->amount_total);
    TEST_ASSERT(info.used_unread <= info.used);
    TEST_ASSERT(info.size == arg->amount_total);

    arg->amount_before += len;

    return 0;
}

static void
ltfwu_verify_log(void)
{
    struct log_offset log_offset;
    struct log_storage_info info;
    struct ltfwu_walk_arg arg;
    int rc;

    memset(&arg, 0, sizeof(arg));
    log_offset = (struct log_offset) {
        .lo_arg = &arg,
        .lo_index = 0,
        .lo_ts = 0,
        .lo_data_len = 0,
    };

    rc = log_storage_info(&ltfwu_log, &info);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT(info.size > 0);
    TEST_ASSERT(info.size == 8 * LTFWU_SECTOR_SIZE);
    TEST_ASSERT(info.used > 0);
    TEST_ASSERT(info.used < info.size);
    arg.amount_total = info.size;

    rc = log_walk_body(&ltfwu_log, ltfwu_verify_log_walk, &log_offset);
    TEST_ASSERT_FATAL(rc == 0);
}

static void
ltfwu_init(const struct ltfwu_cfg *cfg)
{
    int rc;

    /* Ensure tests are repeatable. */
    srand(0);

    ltfwu_cfg = *cfg;

    ltfwu_fcb_log = (struct fcb_log) {
        .fl_fcb.f_scratch_cnt = 1,
        .fl_fcb.f_range_cnt = 1,
        .fl_fcb.f_sector_cnt = ltfwu_fcb_range.fsr_sector_count,
        .fl_fcb.f_ranges = &ltfwu_fcb_range,
        .fl_fcb.f_magic = 0xBEADBAFA,
        .fl_fcb.f_version = 0,
    };

    rc = flash_area_erase(&ltfwu_fcb_range.fsr_flash_area, 0,
                          ltfwu_fcb_range.fsr_flash_area.fa_size);
    TEST_ASSERT_FATAL(rc == 0);

    rc = fcb2_init(&ltfwu_fcb_log.fl_fcb);
    TEST_ASSERT_FATAL(rc == 0);

    log_register("log", &ltfwu_log, &log_fcb_handler, &ltfwu_fcb_log,
                 LOG_SYSLEVEL);
}

void
ltfwu_test_once(const struct ltfwu_cfg *cfg)
{
    int i;

    ltfwu_init(cfg);

    /* Do this three times:
     * 1. Write a bunch of entries to the log.
     * 2. Walk the log, watermark spot and check that storage report
     *    gives sane looking values.
     *
     * This procedure is repeated three times to ensure that the FCB is rotated
     * between walks.
     */
    for (i = 0; i < 3; i++) {
        ltfwu_populate_log(cfg->pop_count);

        ltfwu_verify_log();
    }
}
