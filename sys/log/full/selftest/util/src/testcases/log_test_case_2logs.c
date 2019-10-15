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

struct ltc2l_entry {
    struct log *log;
    uint32_t idx;
};

#define LTC2L_MAX_ENTRIES   16
static struct ltc2l_entry ltc2l_entries[LTC2L_MAX_ENTRIES];
static int ltc2l_num_entries;

static void
ltc2l_append_cb(struct log *log, uint32_t idx)
{
    struct ltc2l_entry *entry;

    TEST_ASSERT_FATAL(ltc2l_num_entries < LTC2L_MAX_ENTRIES);

    entry = &ltc2l_entries[ltc2l_num_entries++];
    entry->log = log;
    entry->idx = idx;
}

TEST_CASE_SELF(log_test_case_2logs)
{
    int rc;
    struct fcb_log fcb_log1;
    struct log log1;
    struct fcb_log fcb_log2;
    struct log log2;

    ltu_setup_2fcbs(&fcb_log1, &log1, &fcb_log2, &log2);

    log_set_append_cb(&log1, ltc2l_append_cb);
    log_set_append_cb(&log2, ltc2l_append_cb);

    rc = log_append_body(&log1, 0, 0, LOG_ETYPE_STRING, "0", 1);
    TEST_ASSERT(rc == 0);
    rc = log_append_body(&log1, 0, 0, LOG_ETYPE_STRING, "1", 1);
    TEST_ASSERT(rc == 0);
    rc = log_append_body(&log2, 0, 0, LOG_ETYPE_STRING, "2", 1);
    TEST_ASSERT(rc == 0);
    rc = log_append_body(&log2, 0, 0, LOG_ETYPE_STRING, "3", 1);
    TEST_ASSERT(rc == 0);
    rc = log_append_body(&log1, 0, 0, LOG_ETYPE_STRING, "4", 1);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(ltc2l_num_entries == 5);
    TEST_ASSERT(ltc2l_entries[0].log == &log1);
    TEST_ASSERT(ltc2l_entries[1].log == &log1);
    TEST_ASSERT(ltc2l_entries[2].log == &log2);
    TEST_ASSERT(ltc2l_entries[3].log == &log2);
    TEST_ASSERT(ltc2l_entries[4].log == &log1);
#if MYNEWT_VAL(LOG_GLOBAL_IDX)
    /* global index space */
    TEST_ASSERT(ltc2l_entries[0].idx == 0);
    TEST_ASSERT(ltc2l_entries[1].idx == 1);
    TEST_ASSERT(ltc2l_entries[2].idx == 2);
    TEST_ASSERT(ltc2l_entries[3].idx == 3);
    TEST_ASSERT(ltc2l_entries[4].idx == 4);
#else
    /* per-log index */
    TEST_ASSERT(ltc2l_entries[0].idx == 0);
    TEST_ASSERT(ltc2l_entries[1].idx == 1);
    TEST_ASSERT(ltc2l_entries[2].idx == 0);
    TEST_ASSERT(ltc2l_entries[3].idx == 1);
    TEST_ASSERT(ltc2l_entries[4].idx == 2);
#endif
}
