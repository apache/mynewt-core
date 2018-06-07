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

TEST_CASE(modlog_test_case_printf)
{
    struct mltu_log_arg mla;
    struct log log;
    char buf[MYNEWT_VAL(MODLOG_MAX_PRINTF_LEN) * 2];
    int rc;
    int i;

    sysinit();

    memset(&mla, 0, sizeof mla);
    mltu_register_log(&log, &mla, "log", 0);

    rc = modlog_register(1, &log, 1, NULL);
    TEST_ASSERT_FATAL(rc == 0);

    modlog_printf(1, 1, "hello %d %s %c", 99, "abc", 'x');

    TEST_ASSERT_FATAL(mla.num_entries == 1);
    TEST_ASSERT(strcmp((char *)mla.entries[0].body, "hello 99 abc x") == 0);

    /* Ensure oversized write gets truncated. */
    for (i = 0; i < sizeof buf - 1; i++) {
        buf[i] = '0' + i % 10;
    }
    buf[sizeof buf - 1] = '\0';

    modlog_printf(1, 1, "%s", buf);
    TEST_ASSERT_FATAL(mla.num_entries == 2);
    TEST_ASSERT(mla.entries[1].len == MYNEWT_VAL(MODLOG_MAX_PRINTF_LEN) - 1);
    TEST_ASSERT(memcmp(mla.entries[1].body, buf,
                       MYNEWT_VAL(MODLOG_MAX_PRINTF_LEN) - 1) == 0);
}
