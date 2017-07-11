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
#include <string.h>

#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "os/os.h"
#include "testutil/testutil.h"
#include "fcb/fcb.h"
#include "log/log.h"

struct flash_area fcb_areas[] = {
    [0] = {
        .fa_off = 0x00000000,
        .fa_size = 16 * 1024
    },
    [1] = {
        .fa_off = 0x00004000,
        .fa_size = 16 * 1024
    }
};
struct fcb log_fcb;
struct log my_log;

char *str_logs[] = {
    "testdata",
    "1testdata2",
    NULL
};
int str_idx = 0;
int str_max_idx = 0;

int
log_test_walk1(struct log *log, struct log_offset *log_offset,
               void *dptr, uint16_t len)
{
    int rc;
    struct log_entry_hdr ueh;
    char data[128];
    int dlen;

    TEST_ASSERT(str_idx < str_max_idx);

    rc = log_read(log, dptr, &ueh, 0, sizeof(ueh));
    TEST_ASSERT(rc == sizeof(ueh));

    dlen = len - sizeof(ueh);
    TEST_ASSERT(dlen < sizeof(data));

    rc = log_read(log, dptr, data, sizeof(ueh), dlen);
    TEST_ASSERT(rc == dlen);

    data[rc] = '\0';

    TEST_ASSERT(strlen(str_logs[str_idx]) == dlen);
    TEST_ASSERT(!memcmp(str_logs[str_idx], data, dlen));
    str_idx++;

    return 0;
}

int
log_test_walk2(struct log *log, struct log_offset *log_offset,
               void *dptr, uint16_t len)
{
    TEST_ASSERT(0);
    return 0;
}

TEST_CASE_DECL(log_setup_fcb)
TEST_CASE_DECL(log_append_fcb)
TEST_CASE_DECL(log_walk_fcb)
TEST_CASE_DECL(log_flush_fcb)

TEST_SUITE(log_test_all)
{
    log_setup_fcb();
    log_append_fcb();
    log_walk_fcb();
    log_flush_fcb();
}

#if MYNEWT_VAL(SELFTEST)

int
main(int argc, char **argv)
{
    sysinit();

    log_test_all();

    return tu_any_failed;
}

#endif
