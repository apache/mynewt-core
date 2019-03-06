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

#include "os_test_priv.h"

#define OTTC_MAX_ENTRIES    16

struct ottc_entry {
    struct os_timeval prev_tv;
    struct os_timeval cur_tv;
    struct os_timezone prev_tz;
    struct os_timezone cur_tz;
    bool newly_synced;
    void *arg;
};

/** Holds records of listener callback calls. */
static struct ottc_entry ottc_entries[OTTC_MAX_ENTRIES];
static int ottc_num_entries;

static void
ottc_time_change_cb(const struct os_time_change_info *info, void *arg)
{
    struct ottc_entry *entry;

    TEST_ASSERT_FATAL(ottc_num_entries < OTTC_MAX_ENTRIES);
    entry = &ottc_entries[ottc_num_entries++];

    entry->prev_tv = *info->tci_prev_tv;
    entry->cur_tv = *info->tci_cur_tv;
    entry->prev_tz = *info->tci_prev_tz;
    entry->cur_tz = *info->tci_cur_tz;
    entry->newly_synced = info->tci_newly_synced;
    entry->arg = arg;
}

TEST_CASE_SELF(os_time_test_change)
{
    struct os_timezone tz1;
    struct os_timezone tz2;
    struct os_timeval tv1;
    struct os_timeval tv2;
    int rc;
    int i;

    struct os_time_change_listener listeners[3] = {
        [0] = {
            .tcl_fn = ottc_time_change_cb,
            .tcl_arg = (void *)(uintptr_t)0,
        },
        [1] = {
            .tcl_fn = ottc_time_change_cb,
            .tcl_arg = (void *)(uintptr_t)1,
        },
        [2] = {
            .tcl_fn = ottc_time_change_cb,
            .tcl_arg = (void *)(uintptr_t)2,
        },
    };

    /* Register one listener. */
    os_time_change_listen(&listeners[0]);

    /* Set time; ensure single listener called. */
    tv1.tv_sec = 123;
    tv1.tv_usec = 456;
    tz1.tz_minuteswest = 555;
    tz1.tz_dsttime = 666;

    rc = os_settimeofday(&tv1, &tz1);
    TEST_ASSERT_FATAL(rc == 0);

    TEST_ASSERT_FATAL(ottc_num_entries == 1);
    TEST_ASSERT(memcmp(&ottc_entries[0].cur_tv, &tv1, sizeof tv1) == 0);
    TEST_ASSERT(memcmp(&ottc_entries[0].cur_tz, &tz1, sizeof tz1) == 0);
    TEST_ASSERT(ottc_entries[0].newly_synced);
    TEST_ASSERT(ottc_entries[0].arg == (void *)(uintptr_t)0);

    /* Register two more listeners. */
    os_time_change_listen(&listeners[1]);
    os_time_change_listen(&listeners[2]);

    /* Set time; ensure all three listeners called. */
    tv2.tv_sec = 234;
    tv2.tv_usec = 567;
    tz2.tz_minuteswest = 777;
    tz2.tz_dsttime = 888;

    rc = os_settimeofday(&tv2, &tz2);
    TEST_ASSERT_FATAL(rc == 0);

    TEST_ASSERT_FATAL(ottc_num_entries == 4);
    for (i = 1; i < 4; i++) {
        TEST_ASSERT(memcmp(&ottc_entries[i].cur_tv, &tv2, sizeof tv2) == 0);
        TEST_ASSERT(memcmp(&ottc_entries[i].cur_tz, &tz2, sizeof tz2) == 0);
        TEST_ASSERT(memcmp(&ottc_entries[i].prev_tv, &tv2, sizeof tv2) == 0);
        TEST_ASSERT(memcmp(&ottc_entries[i].prev_tz, &tz2, sizeof tz2) == 0);
        TEST_ASSERT(!ottc_entries[i].newly_synced);
        TEST_ASSERT(ottc_entries[i].arg == (void *)(uintptr_t)(i - 1));
    }

    /* Remove all three listeners. */
    for (i = 0; i < 3; i++) {
        rc = os_time_change_remove(&listeners[i]);
        TEST_ASSERT(rc == 0);
    }

    /* Set time; ensure no listeners called. */
    tv2.tv_sec = 345;
    tv2.tv_usec = 678;
    tz2.tz_minuteswest = 888;
    tz2.tz_dsttime = 999;

    rc = os_settimeofday(&tv2, &tz2);
    TEST_ASSERT_FATAL(rc == 0);

    TEST_ASSERT_FATAL(ottc_num_entries == 4);
}
