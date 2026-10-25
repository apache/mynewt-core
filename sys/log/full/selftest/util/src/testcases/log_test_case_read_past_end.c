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

static int
read_past_end_walk_cb(struct log *log, struct log_offset *log_offset,
                      const struct log_entry_hdr *hdr, const void *dptr, uint16_t len)
{
    struct os_mbuf *om;
    uint8_t buf[16];
    int rc;

    /* Reading at or past end of entry must not read anything */
    memset(buf, 0xa5, sizeof(buf));
    rc = log_read(log, dptr, buf, len + log_hdr_len(hdr) + 8, sizeof(buf));
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(buf[0] == 0xa5);

    om = os_msys_get_pkthdr(0, 0);
    TEST_ASSERT_FATAL(om != NULL);
    rc = log_read_mbuf(log, dptr, om, len + log_hdr_len(hdr) + 8, sizeof(buf));
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(OS_MBUF_PKTLEN(om) == 0);
    os_mbuf_free_chain(om);

    (*(int *)log_offset->lo_arg)++;

    return 0;
}

TEST_CASE_SELF(log_test_case_read_past_end)
{
    struct log_offset log_offset = { 0 };
    struct fcb_log fcb_log;
    struct log log = { 0 };
    int cnt = 0;
    int rc;

    ltu_setup_fcb(&fcb_log, &log);

    rc = log_append_body(&log, 0, 0, LOG_ETYPE_STRING, "abc", 3);
    TEST_ASSERT_FATAL(rc == 0);

    log_offset.lo_arg = &cnt;
    rc = log_walk_body(&log, read_past_end_walk_cb, &log_offset);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(cnt == 1);
}
