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

#include "modlog_test.h"

static void
mltcp_run(bool use_mbufs)
{
    struct mltu_log_arg mla1;
    struct mltu_log_arg mla2;
    struct mltu_log_arg mla3;
    struct log log1;
    struct log log2;
    struct log log3;
    uint8_t buf[sizeof (struct log_entry_hdr) + 16];
    int rc;

    sysinit();

    /* Initialize three logs. */
    memset(&mla1, 0, sizeof mla1);
    mltu_register_log(&log1, &mla1, "log1", 0);

    memset(&mla2, 0, sizeof mla2);
    mltu_register_log(&log2, &mla2, "log2", 0);

    memset(&mla3, 0, sizeof mla3);
    mltu_register_log(&log3, &mla3, "log3", 0);

    /* Map a different module to each log. */
    rc = modlog_register(1, &log1, 1, NULL);
    TEST_ASSERT_FATAL(rc == 0);

    rc = modlog_register(2, &log2, 2, NULL);
    TEST_ASSERT_FATAL(rc == 0);

    rc = modlog_register(3, &log3, 3, NULL);
    TEST_ASSERT_FATAL(rc == 0);

    /* Ensure too-low priorities are discarded. */
    mltu_append(1, 0, LOG_ETYPE_STRING, buf, 1, use_mbufs);
    mltu_append(2, 0, LOG_ETYPE_STRING, buf, 1, use_mbufs);
    mltu_append(3, 0, LOG_ETYPE_STRING, buf, 1, use_mbufs);

    TEST_ASSERT(mla1.num_entries == 0);
    TEST_ASSERT(mla2.num_entries == 0);
    TEST_ASSERT(mla3.num_entries == 0);

    mltu_append(1, 1, LOG_ETYPE_STRING, buf, 1, use_mbufs);
    mltu_append(2, 1, LOG_ETYPE_STRING, buf, 1, use_mbufs);
    mltu_append(3, 1, LOG_ETYPE_STRING, buf, 1, use_mbufs);

    TEST_ASSERT(mla1.num_entries == 1);
    TEST_ASSERT(mla2.num_entries == 0);
    TEST_ASSERT(mla3.num_entries == 0);

    mltu_append(1, 2, LOG_ETYPE_STRING, buf, 1, use_mbufs);
    mltu_append(2, 2, LOG_ETYPE_STRING, buf, 1, use_mbufs);
    mltu_append(3, 2, LOG_ETYPE_STRING, buf, 1, use_mbufs);

    TEST_ASSERT(mla1.num_entries == 2);
    TEST_ASSERT(mla2.num_entries == 1);
    TEST_ASSERT(mla3.num_entries == 0);

    mltu_append(1, 3, LOG_ETYPE_STRING, buf, 1, use_mbufs);
    mltu_append(2, 3, LOG_ETYPE_STRING, buf, 1, use_mbufs);
    mltu_append(3, 3, LOG_ETYPE_STRING, buf, 1, use_mbufs);

    TEST_ASSERT(mla1.num_entries == 3);
    TEST_ASSERT(mla2.num_entries == 2);
    TEST_ASSERT(mla3.num_entries == 1);
}

TEST_CASE(modlog_test_case_prio)
{
    mltcp_run(false);
    mltcp_run(true);
}
