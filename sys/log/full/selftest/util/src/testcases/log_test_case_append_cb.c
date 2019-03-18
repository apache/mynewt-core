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

struct ltcwc_entry {
    struct log *log;
    uint32_t idx;
};

#define LTCWC_MAX_ENTRIES   16
struct ltcwc_entry ltcwc_entries[LTCWC_MAX_ENTRIES];
int ltcwc_num_entries;

static void
ltcwc_append_cb(struct log *log, uint32_t idx)
{
    struct ltcwc_entry *entry;

    TEST_ASSERT_FATAL(ltcwc_num_entries < LTCWC_MAX_ENTRIES);

    entry = &ltcwc_entries[ltcwc_num_entries++];
    entry->log = log;
    entry->idx = idx;
}

TEST_CASE_SELF(log_test_case_append_cb)
{
    struct fcb_log fcb_log;
    struct log log;

    ltu_setup_fcb(&fcb_log, &log);

    /*** No callback; num entries remains 0. */

    log_append_body(&log, 0, 0, LOG_ETYPE_STRING, "0", 1);
    TEST_ASSERT(ltcwc_num_entries == 0);

    /*** Set callback; num entries increases to 1. */

    log_set_append_cb(&log, ltcwc_append_cb);

    log_append_body(&log, 0, 0, LOG_ETYPE_STRING, "1", 1);
    TEST_ASSERT(ltcwc_num_entries == 1);
    TEST_ASSERT(ltcwc_entries[0].log == &log);
    TEST_ASSERT(ltcwc_entries[0].idx == 1);

    /*** No callback; num entries remains 1. */

    log_set_append_cb(&log, NULL);

    log_append_body(&log, 0, 0, LOG_ETYPE_STRING, "2", 1);
    TEST_ASSERT(ltcwc_num_entries == 1);
}
