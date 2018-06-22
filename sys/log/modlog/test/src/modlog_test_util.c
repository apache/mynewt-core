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
mltu_log_append(struct log *log, void *buf, int len)
{
    struct mltu_log_entry *entry;
    struct log_entry_hdr *hdr;
    struct mltu_log_arg *mla;
    int body_len;

    mla = log->l_arg;

    body_len = len - sizeof *hdr;

    TEST_ASSERT_FATAL(mla->num_entries < MLTU_LOG_ARG_MAX_ENTRIES);
    TEST_ASSERT_FATAL(body_len <= MLTU_LOG_ENTRY_MAX_LEN);

    entry = mla->entries + mla->num_entries++;

    hdr = buf;
    entry->hdr = *hdr;
    entry->len = body_len;
    memcpy(entry->body, hdr + 1, body_len);

    return 0;
}

static int
mltu_log_append_mbuf(struct log *log, struct os_mbuf *om)
{
    struct mltu_log_entry *entry;
    struct mltu_log_arg *mla;
    int body_len;
    int rc;

    mla = log->l_arg;

    body_len = OS_MBUF_PKTLEN(om) - sizeof entry->hdr;

    TEST_ASSERT_FATAL(mla->num_entries < MLTU_LOG_ARG_MAX_ENTRIES);
    TEST_ASSERT_FATAL(body_len <= MLTU_LOG_ENTRY_MAX_LEN);

    entry = mla->entries + mla->num_entries++;

    rc = os_mbuf_copydata(om, 0, sizeof entry->hdr, &entry->hdr);
    TEST_ASSERT_FATAL(rc == 0);

    entry->len = body_len;
    rc = os_mbuf_copydata(om, sizeof entry->hdr, body_len, entry->body);
    TEST_ASSERT_FATAL(rc == 0);

    return 0;
}

static const struct log_handler mltu_handler = {
    .log_type = LOG_TYPE_MEMORY,
    .log_append = mltu_log_append,
    .log_append_mbuf = mltu_log_append_mbuf,
};

void
mltu_register_log(struct log *lg, struct mltu_log_arg *arg, const char *name,
                  uint8_t level)
{
    int rc;

    rc = log_register((char *)name, lg, &mltu_handler, arg, level);
    TEST_ASSERT_FATAL(rc == 0);
}

struct os_mbuf *
mltu_alloc_buf(void)
{
    struct os_mbuf *om;
    uint8_t *u8p;

    om = os_msys_get_pkthdr(0, 0);
    TEST_ASSERT_FATAL(om != NULL);

    u8p = os_mbuf_extend(om, sizeof (struct log_entry_hdr));
    TEST_ASSERT_FATAL(u8p != NULL);

    return om;
}

void
mltu_append(uint8_t module, uint8_t level, uint8_t etype, void *data,
            int len, bool mbuf)
{
    uint8_t buf[1024];
    struct os_mbuf *om;
    int rc;

    if (!mbuf) {
        TEST_ASSERT_FATAL(len + sizeof (struct log_entry_hdr) <= sizeof buf);
        memcpy(buf + sizeof (struct log_entry_hdr), data, len);

        rc = modlog_append(module, level, etype, buf, len);
    } else {
        om = mltu_alloc_buf();
        rc = os_mbuf_append(om, data, len);
        TEST_ASSERT_FATAL(rc == 0);

        rc = modlog_append_mbuf(module, level, etype, om);
    }
    TEST_ASSERT_FATAL(rc == 0);
}
