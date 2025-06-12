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

#include "log_test_util/log_test_util.h"

struct log_walk_data {
    struct log_offset log_offset;
    size_t walk_count;
    int idx;
};

static int
log_last_walk(struct log *log, struct log_offset *log_offset,
              const void *dptr, uint16_t len)
{
    struct log_walk_data *wd = (struct log_walk_data *)log_offset;
    struct log_entry_hdr hdr;

    log_read(log, dptr, &hdr, 0, sizeof(hdr));

    wd->idx = hdr.ue_index;
    wd->walk_count++;

    return 0;
}

static uint32_t
log_last(struct log *log, struct log_walk_data *wd)
{
    memset(wd, 0, sizeof(*wd));

    log_walk(log, log_last_walk, &wd->log_offset);

    return wd->walk_count;
}

TEST_CASE_SELF(log_test_case_level)
{
    struct cbmem cbmem;
    struct log log;
    struct log_walk_data wd;
    size_t log_count;
    int rc;
    int i;

    ltu_setup_cbmem(&cbmem, &log);

    /* Ensure all modules initialized to 0. */
    for (i = 0; i < 256; i++) {
        TEST_ASSERT(log_level_get(i) == 0);
    }

    /* Level too great.  Ensure saturation at LOG_LEVEL_MAX. */
    rc = log_level_set(100, LOG_LEVEL_MAX + 1);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(log_level_get(100) == LOG_LEVEL_MAX);

    /* Ensure all modules can be configured. */
    for (i = 0; i < 256; i++) {
        rc = log_level_set(i, i % 16);
        TEST_ASSERT(rc == 0);
    }
    for (i = 0; i < 256; i++) {
        TEST_ASSERT(log_level_get(i) == i % 16);
    }

    /* Ensure no log write when level is too low. */
    rc = log_level_set(100, 4);
    TEST_ASSERT(rc == 0);

    /* wd is used several times and it is cleared at the beginning of log_last */
    log_count = log_last(&log, &wd);
    /* Log is empty */
    TEST_ASSERT(log_count == 0);
    /* Write logs that should be dropped due to level */
    for (uint8_t level = 0; level < 4; ++level) {
        log_printf(&log, 100, 1, "hello");
        /* Log is still empty? */
        log_count = log_last(&log, &wd);
        TEST_ASSERT(log_count == 0);
    }

    /* Ensure log write when level is equal. */
    log_printf(&log, 100, 4, "hello1");
    log_count = log_last(&log, &wd);
    int log_idx = wd.idx;
    TEST_ASSERT(log_count == 1);
    /* Ensure log write when level is equal. */
    log_printf(&log, 100, 4, "hello2");
    log_count = log_last(&log, &wd);
    TEST_ASSERT(log_count == 2);
    TEST_ASSERT(log_idx < wd.idx);
    /* Write log with higher level */
    log_idx = wd.idx;
    log_printf(&log, 100, 5, "hello3");
    log_count = log_last(&log, &wd);
    TEST_ASSERT(log_count == 3);
    TEST_ASSERT(log_idx < wd.idx);
}
