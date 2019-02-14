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

#include <assert.h>
#include <string.h>
#include "base64/base64.h"
#include "config/config.h"
#include "fault_priv.h"

static char *fault_conf_get(int argc, char **argv, char *buf, int max_len);
static int fault_conf_set(int argc, char **argv, char *val);
static int fault_conf_commit(void);
static int fault_conf_export(void (*func)(char *name, char *val),
        enum conf_export_tgt tgt);

static struct conf_handler fault_conf_handler = {
    .ch_name = "fault",
    .ch_get = fault_conf_get,
    .ch_set = fault_conf_set,
    .ch_commit = fault_conf_commit,
    .ch_export = fault_conf_export
};

/** Converts in-RAM setting to a config-friendly string. */
static char *
fault_conf_get(int argc, char **argv, char *buf, int max_len)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "chronfail")) {
            return conf_str_from_bytes(fault_chronic_counts,
                    sizeof fault_chronic_counts,
                    buf, max_len);
        }
    }
    return NULL;
}

/** Converts config string to binary in-RAM value. */
static int
fault_conf_set(int argc, char **argv, char *val)
{
    int decode_len;

    if (argc == 1) {
        if (strcmp(argv[0], "chronfail") == 0) {
            decode_len = base64_decode_len(val);
            if (decode_len > MYNEWT_VAL(FAULT_MAX_DOMAINS)) {
                return SYS_ENOMEM;
            }

            memset(fault_chronic_counts, 0, sizeof fault_chronic_counts);
            base64_decode(val, fault_chronic_counts);

            return 0;
        }
    }
    return -1;
}

static int
fault_conf_commit(void)
{
    return 0;
}

static int
fault_conf_export(void (*func)(char *name, char *val),
        enum conf_export_tgt tgt)
{
    char buf[BASE64_ENCODE_SIZE(sizeof fault_chronic_counts) + 1];

    conf_str_from_bytes(fault_chronic_counts, sizeof fault_chronic_counts,
        buf, sizeof buf);

    func("fault/chronfail", buf);
    return 0;
}

int
fault_conf_persist_chronic_counts(void)
{
    char buf[BASE64_ENCODE_SIZE(sizeof fault_chronic_counts) + 1];

    conf_str_from_bytes(fault_chronic_counts, sizeof fault_chronic_counts,
        buf, sizeof buf);

    return conf_save_one("fault/chronfail", buf);
}

int
fault_conf_init(void)
{
    return conf_register(&fault_conf_handler);
}
