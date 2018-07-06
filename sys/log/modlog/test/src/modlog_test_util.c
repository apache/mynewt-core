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
#include "os/mynewt.h"
#include "modlog_test.h"

static int
mltu_log_append_body(struct log *log, const struct log_entry_hdr *hdr,
                     const void *buf, int len)
{
    struct mltu_log_entry *entry;
    struct mltu_log_arg *mla;

    mla = log->l_arg;

    TEST_ASSERT_FATAL(mla->num_entries < MLTU_LOG_ARG_MAX_ENTRIES);
    TEST_ASSERT_FATAL(len <= MLTU_LOG_ENTRY_MAX_LEN);

    entry = mla->entries + mla->num_entries++;
    entry->hdr = *hdr;
    entry->len = len;
    memcpy(entry->body, buf, len);

    return 0;
}

static int
mltu_log_append_mbuf_body(struct log *log, const struct log_entry_hdr *hdr,
                          const struct os_mbuf *om)
{
    struct mltu_log_entry *entry;
    struct mltu_log_arg *mla;
    int rc;

    mla = log->l_arg;

    TEST_ASSERT_FATAL(mla->num_entries < MLTU_LOG_ARG_MAX_ENTRIES);
    TEST_ASSERT_FATAL(OS_MBUF_PKTLEN(om) <= MLTU_LOG_ENTRY_MAX_LEN);

    entry = mla->entries + mla->num_entries++;
    entry->hdr = *hdr;
    entry->len = OS_MBUF_PKTLEN(om);

    rc = os_mbuf_copydata(om, 0, OS_MBUF_PKTLEN(om), entry->body);
    TEST_ASSERT_FATAL(rc == 0);

    return 0;
}

static const struct log_handler mltu_handler = {
    .log_type = LOG_TYPE_MEMORY,
    .log_append_body = mltu_log_append_body,
    .log_append_mbuf_body = mltu_log_append_mbuf_body,
};

void
mltu_register_log(struct log *lg, struct mltu_log_arg *arg, const char *name,
                  uint8_t level)
{
    int rc;

    rc = log_register((char *)name, lg, &mltu_handler, arg, level);
    TEST_ASSERT_FATAL(rc == 0);
}

void
mltu_append(uint8_t module, uint8_t level, uint8_t etype, void *data,
            int len, bool mbuf)
{
    struct os_mbuf *om;
    int rc;

    if (!mbuf) {
        rc = modlog_append(module, level, etype, data, len);
    } else {
        om = os_msys_get_pkthdr(0, 0);
        TEST_ASSERT_FATAL(om != NULL);

        rc = os_mbuf_append(om, data, len);
        TEST_ASSERT_FATAL(rc == 0);

        rc = modlog_append_mbuf(module, level, etype, om);
    }
    TEST_ASSERT_FATAL(rc == 0);
}
