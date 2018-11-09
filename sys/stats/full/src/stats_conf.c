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

#include "os/mynewt.h"

#if MYNEWT_VAL(STATS_PERSIST)

#include <assert.h>
#include <stdio.h>

#include "base64/base64.h"
#include "config/config.h"
#include "stats/stats.h"
#include "stats_priv.h"

static char *stats_conf_get(int argc, char **argv, char *buf, int max_len);
static int stats_conf_set(int argc, char **argv, char *val);
static int stats_conf_commit(void);
static int stats_conf_export(void (*func)(char *name, char *val),
                             enum conf_export_tgt tgt);

static struct conf_handler stats_conf_handler = {
    .ch_name = "stat",
    .ch_get = stats_conf_get,
    .ch_set = stats_conf_set,
    .ch_commit = stats_conf_commit,
    .ch_export = stats_conf_export
};

static int
stats_conf_snprintf_name(const struct stats_hdr *hdr, size_t max_len,
                         char *buf)
{
    return snprintf(buf, max_len, "stat/%s", hdr->s_name);
}

static void
stats_conf_name(const struct stats_hdr *hdr, char *buf)
{
    stats_conf_snprintf_name(hdr, MYNEWT_VAL(STATS_PERSIST_MAX_NAME_SIZE),
                             buf);
}

static void
stats_conf_serialize(const struct stats_hdr *hdr, void *buf)
{
    size_t rawlen;
    void *data;

    rawlen = stats_size(hdr);
    data = stats_data(hdr);

    conf_str_from_bytes(data, rawlen, buf, MYNEWT_VAL(STATS_PERSIST_BUF_SIZE));
}

/** Converts in-RAM setting to a config-friendly string. */
static char *
stats_conf_get(int argc, char **argv, char *buf, int max_len)
{
    const struct stats_hdr *hdr;

    if (argc == 1) {
        hdr = stats_group_find(argv[0]);
        if (hdr != NULL) {
            stats_conf_serialize(hdr, buf);
        }
    }
    return NULL;
}

/** Converts config string to binary in-RAM value. */
static int
stats_conf_set(int argc, char **argv, char *val)
{
    struct stats_hdr *hdr;
    size_t size;
    void *data;
    int decode_len;

    if (argc == 1) {
        hdr = stats_group_find(argv[0]);
        if (hdr != NULL) {
            size = stats_size(hdr);
            data = stats_data(hdr);

            decode_len = base64_decode_len(val);
            if (decode_len > size) {
                DEBUG_PANIC();
                return OS_ENOMEM;
            }

            memset(data, 0, size);
            base64_decode(val, data);

            return 0;
        }
    }
    return OS_ENOENT;
}

static int
stats_conf_commit(void)
{
    return 0;
}

/**
 * This structure just holds a pointer to walk callback.  It is undefined
 * behavior to cast a function pointer to `void *`, so we wrap it with
 * something that can be safely converted.
 */
struct stats_conf_export_walk_arg {
    void (*func)(char *name, char *val);
};

static int
stats_conf_export_walk(struct stats_hdr *hdr, void *arg)
{
    char name[MYNEWT_VAL(STATS_PERSIST_MAX_NAME_SIZE)];
    char data[MYNEWT_VAL(STATS_PERSIST_BUF_SIZE)];
    struct stats_conf_export_walk_arg *walk_arg;

    walk_arg = arg;

    if (!(hdr->s_flags & STATS_HDR_F_PERSIST)) {
        return 0;
    }

    stats_conf_name(hdr, name);
    stats_conf_serialize(hdr, data);

    walk_arg->func(name, data);

    return 0;
}

static int
stats_conf_export(void (*func)(char *name, char *val),
        enum conf_export_tgt tgt)
{
    struct stats_conf_export_walk_arg arg = { func };
    int rc;

    rc = stats_group_walk(stats_conf_export_walk, &arg);
    return rc;
}

int
stats_conf_save_group(const struct stats_hdr *hdr)
{
    char name[MYNEWT_VAL(STATS_PERSIST_MAX_NAME_SIZE)];
    char data[MYNEWT_VAL(STATS_PERSIST_BUF_SIZE)];

    stats_conf_name(hdr, name);
    stats_conf_serialize(hdr, data);

    return conf_save_one(name, data);
}

void
stats_conf_assert_valid(const struct stats_hdr *hdr)
{
    size_t rawlen;
    size_t enclen;

    rawlen = stats_size(hdr);
    enclen = BASE64_ENCODE_SIZE(rawlen);
    assert(enclen <= MYNEWT_VAL(STATS_PERSIST_BUF_SIZE));

    rawlen = stats_conf_snprintf_name(hdr, 0, NULL);
    assert(rawlen < MYNEWT_VAL(STATS_PERSIST_MAX_NAME_SIZE));
}

void
stats_conf_init(void)
{
    int rc;

    rc = conf_register(&stats_conf_handler);
    SYSINIT_PANIC_ASSERT(rc == 0);
}

#endif
