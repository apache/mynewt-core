/**
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

#include <os/os.h>
#include <testutil/testutil.h>
#include <fcb/fcb.h>
#include "log/log.h"

static struct flash_area fcb_areas[] = {
    [0] = {
        .fa_off = 0x00000000,
        .fa_size = 16 * 1024
    },
    [1] = {
        .fa_off = 0x00004000,
        .fa_size = 16 * 1024
    }
};
static struct log_handler log_fcb_handler;
static struct fcb log_fcb;
static struct log my_log;

static char *str_logs[] = {
    "testdata",
    "1testdata2",
    NULL
};
static int str_idx = 0;
static int str_max_idx = 0;

TEST_CASE(log_setup_fcb)
{
    int rc;
    int i;

    log_fcb.f_sectors = fcb_areas;
    log_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);
    log_fcb.f_magic = 0x7EADBADF;
    log_fcb.f_version = 0;

    for (i = 0; i < log_fcb.f_sector_cnt; i++) {
        rc = flash_area_erase(&fcb_areas[i], 0, fcb_areas[i].fa_size);
        TEST_ASSERT(rc == 0);
    }
    rc = fcb_init(&log_fcb);
    TEST_ASSERT(rc == 0);
    rc = log_fcb_handler_init(&log_fcb_handler, &log_fcb, 0);
    TEST_ASSERT(rc == 0);

    log_register("log", &my_log, &log_fcb_handler);
}

TEST_CASE(log_append_fcb)
{
    char *str;

    while (1) {
        str = str_logs[str_max_idx];
        if (!str) {
            break;
        }
        log_printf(&my_log, 0, 0, str, strlen(str));
        str_max_idx++;
    }
}

static int
log_test_walk1(struct log *log, void *arg, void *dptr, uint16_t len)
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

TEST_CASE(log_walk_fcb)
{
    int rc;

    str_idx = 0;

    rc = log_walk(&my_log, log_test_walk1, NULL);
    TEST_ASSERT(rc == 0);
}

static int
log_test_walk2(struct log *log, void *arg, void *dptr, uint16_t len)
{
    TEST_ASSERT(0);
    return 0;
}

TEST_CASE(log_flush_fcb)
{
    int rc;

    rc = log_flush(&my_log);
    TEST_ASSERT(rc == 0);

    rc = log_walk(&my_log, log_test_walk2, NULL);
    TEST_ASSERT(rc == 0);
}

TEST_SUITE(log_test_all)
{
    log_setup_fcb();
    log_append_fcb();
    log_walk_fcb();
    log_flush_fcb();
}

#ifdef MYNEWT_SELFTEST

int
main(int argc, char **argv)
{
    tu_config.tc_print_results = 1;
    tu_init();

    log_init();
    log_test_all();

    return tu_any_failed;
}

#endif
