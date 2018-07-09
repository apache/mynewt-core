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
mltca_run(bool use_mbufs)
{
    struct mltu_log_arg mla1;
    struct mltu_log_arg mla2;
    struct mltu_log_arg mla3;
    struct log log1;
    struct log log2;
    struct log log3;
    uint8_t handle1;
    uint8_t handle2;
    uint8_t handle3;
    uint8_t byte;
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
    rc = modlog_register(1, &log1, 1, &handle1);
    TEST_ASSERT_FATAL(rc == 0);

    rc = modlog_register(2, &log2, 2, &handle2);
    TEST_ASSERT_FATAL(rc == 0);

    rc = modlog_register(3, &log3, 3, &handle3);
    TEST_ASSERT_FATAL(rc == 0);

    /* Ensure unmapped module with no default causes no write. */
    mltu_append(100, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);
    TEST_ASSERT(mla1.num_entries == 0);
    TEST_ASSERT(mla2.num_entries == 0);
    TEST_ASSERT(mla3.num_entries == 0);

    /* Write a different entry to each module */
    byte = '1';
    mltu_append(1, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);

    byte = '2';
    mltu_append(2, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);

    byte = '3';
    mltu_append(3, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);

    TEST_ASSERT(mla1.num_entries == 1);
    TEST_ASSERT(mla1.entries[0].len == 1);
    TEST_ASSERT(mla1.entries[0].body[0] == '1');
    TEST_ASSERT(mla1.entries[0].hdr.ue_module == 1);

    TEST_ASSERT(mla2.num_entries == 1);
    TEST_ASSERT(mla2.entries[0].len == 1);
    TEST_ASSERT(mla2.entries[0].body[0] == '2');
    TEST_ASSERT(mla2.entries[0].hdr.ue_module == 2);

    TEST_ASSERT(mla3.num_entries == 1);
    TEST_ASSERT(mla3.entries[0].len == 1);
    TEST_ASSERT(mla3.entries[0].body[0] == '3');
    TEST_ASSERT(mla3.entries[0].hdr.ue_module == 3);

    /* Clear logs. */
    mla1.num_entries = 0;
    mla2.num_entries = 0;
    mla3.num_entries = 0;

    /* Point module 3 at log 2. */
    rc = modlog_delete(handle3);
    TEST_ASSERT_FATAL(rc == 0);
    rc = modlog_register(3, &log2, 3, &handle3);
    TEST_ASSERT_FATAL(rc == 0);

    /* Write to modules 2 and 3; verify log 2 written twice. */
    mltu_append(2, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);
    mltu_append(3, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);

    TEST_ASSERT(mla1.num_entries == 0);
    TEST_ASSERT(mla2.num_entries == 2);
    TEST_ASSERT(mla3.num_entries == 0);

    /* Clear logs. */
    mla1.num_entries = 0;
    mla2.num_entries = 0;
    mla3.num_entries = 0;

    /* Point module 1 at all three logs. */
    rc = modlog_delete(handle2);
    TEST_ASSERT_FATAL(rc == 0);

    rc = modlog_delete(handle3);
    TEST_ASSERT_FATAL(rc == 0);

    rc = modlog_register(1, &log2, 2, &handle2);
    TEST_ASSERT_FATAL(rc == 0);

    rc = modlog_register(1, &log3, 3, &handle3);
    TEST_ASSERT_FATAL(rc == 0);

    /* Write a single entry to module 1; ensure all logs written. */
    mltu_append(1, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);
    TEST_ASSERT(mla1.num_entries == 1);
    TEST_ASSERT(mla2.num_entries == 1);
    TEST_ASSERT(mla3.num_entries == 1);

    /* Clear logs. */
    mla1.num_entries = 0;
    mla2.num_entries = 0;
    mla3.num_entries = 0;

    /* Make mapping1 a default entry. */
    rc = modlog_delete(handle1);
    TEST_ASSERT_FATAL(rc == 0);

    rc = modlog_register(MODLOG_MODULE_DFLT, &log1, 1, &handle1);
    TEST_ASSERT_FATAL(rc == 0);

    mltu_append(99, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);
    mltu_append(123, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(mla1.num_entries == 2);
    TEST_ASSERT(mla2.num_entries == 0);
    TEST_ASSERT(mla3.num_entries == 0);

    TEST_ASSERT(mla1.entries[0].hdr.ue_module == 99);
    TEST_ASSERT(mla1.entries[1].hdr.ue_module == 123);

    /* Clear logs. */
    mla1.num_entries = 0;
    mla2.num_entries = 0;
    mla3.num_entries = 0;

    /* Make all three descs default entries. */
    rc = modlog_delete(handle2);
    TEST_ASSERT_FATAL(rc == 0);

    rc = modlog_delete(handle3);
    TEST_ASSERT_FATAL(rc == 0);

    rc = modlog_register(MODLOG_MODULE_DFLT, &log2, 2, &handle2);
    TEST_ASSERT_FATAL(rc == 0);

    rc = modlog_register(MODLOG_MODULE_DFLT, &log3, 3, &handle3);
    TEST_ASSERT_FATAL(rc == 0);

    mltu_append(103, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);
    mltu_append(144, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(mla1.num_entries == 2);
    TEST_ASSERT(mla2.num_entries == 2);
    TEST_ASSERT(mla3.num_entries == 2);

    /* Clear logs. */
    mla1.num_entries = 0;
    mla2.num_entries = 0;
    mla3.num_entries = 0;

    /* Remove all default entries. */
    rc = modlog_delete(handle1);
    TEST_ASSERT_FATAL(rc == 0);
    rc = modlog_delete(handle2);
    TEST_ASSERT_FATAL(rc == 0);
    rc = modlog_delete(handle3);
    TEST_ASSERT_FATAL(rc == 0);

    /* Ensure an append has no effect. */
    mltu_append(234, 4, LOG_ETYPE_STRING, &byte, 1, use_mbufs);
    TEST_ASSERT(mla1.num_entries == 0);
    TEST_ASSERT(mla2.num_entries == 0);
    TEST_ASSERT(mla3.num_entries == 0);
}

TEST_CASE(modlog_test_case_append)
{
    mltca_run(false);
    mltca_run(true);
}
