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

/* This whole file is conditionally compiled based on whether the
 * log package is configured to use the shell (MYNEWT_VAL(LOG_CLI)).
 */

#if MYNEWT_VAL(LOG_CLI)

#include <stdio.h>
#include <string.h>
#include <parse/parse.h>
#include <ctype.h>

#include "cbmem/cbmem.h"
#include "log/log.h"
#include "shell/shell.h"
#include "console/console.h"
#include "base64/hex.h"
#include "tinycbor/cbor.h"
#include "tinycbor/compilersupport_p.h"
#include "log_cbor_reader/log_cbor_reader.h"

static uint32_t shell_log_count;


struct walk_arg {
    /* Number of entries to skip */
    uint32_t skip;
    /* Number of entries to process */
    uint32_t count_limit;
    /* Entry number */
    uint32_t count;
};

static int
shell_log_count_entry(struct log *log, struct log_offset *log_offset,
                      const struct log_entry_hdr *ueh, const void *dptr, uint16_t len)
{
    struct walk_arg *arg = (struct walk_arg *)log_offset->lo_arg;

    shell_log_count++;
    if (arg) {
        arg->count++;
        if ((arg->count_limit > 0) && (arg->count >= arg->count_limit)) {
            return 1;
        }
    }

    return 0;
}

static int
shell_log_dump_entry(struct log *log, struct log_offset *log_offset,
                     const struct log_entry_hdr *ueh, const void *dptr, uint16_t len)
{
    char data[128 + 1];
    int dlen;
    int rc = 0;
    struct CborParser cbor_parser;
    struct CborValue cbor_value;
    struct log_cbor_reader cbor_reader;
    struct walk_arg *arg = (struct walk_arg *)log_offset->lo_arg;
    char tmp[32 + 1];
    int off;
    int blksz;
    bool read_data = ueh->ue_etype != LOG_ETYPE_CBOR;
    bool read_hash = ueh->ue_flags & LOG_FLAGS_IMG_HASH;

    if (arg) {
        arg->count++;
        /* Continue walk if number of entries to skip not reached yet */
        if (arg->count <= arg->skip) {
            return 0;
        }
    }

    dlen = min(len, 128);

    if (read_data) {
        rc = log_read_body(log, dptr, data, 0, dlen);
        if (rc < 0) {
            return rc;
        }
        data[rc] = 0;
    }

    if (read_hash) {
        console_printf("[ih=0x%x%x%x%x]", ueh->ue_imghash[0], ueh->ue_imghash[1],
                       ueh->ue_imghash[2], ueh->ue_imghash[3]);
    }
    console_printf(" [%llu] ", ueh->ue_ts);
#if MYNEWT_VAL(LOG_SHELL_SHOW_INDEX)
    console_printf(" [ix=%lu] ", ueh->ue_index);
#endif

    switch (ueh->ue_etype) {
    case LOG_ETYPE_STRING:
        console_write(data, strlen(data));
        break;
    case LOG_ETYPE_CBOR:
        log_cbor_reader_init(&cbor_reader, log, dptr, len);
        cbor_parser_init(&cbor_reader.r, 0, &cbor_parser, &cbor_value);
        cbor_value_to_pretty(stdout, &cbor_value);
        break;
    default:
        for (off = 0; off < rc; off += blksz) {
            blksz = dlen - off;
            if (blksz > sizeof(tmp) >> 1) {
                blksz = sizeof(tmp) >> 1;
            }
            hex_format(&data[off], blksz, tmp, sizeof(tmp));
            console_write(tmp, strlen(tmp));
        }
        if (rc < len) {
            console_write("...", 3);
        }
    }

    console_write("\n", 1);
    if (arg) {
        if ((arg->count_limit > 0) && (arg->count - arg->skip >= arg->count_limit)) {
            return 1;
        }
    }
    return 0;
}

int
shell_log_dump_cmd(int argc, char **argv)
{
    struct log *log;
    struct log_offset log_offset = {};
    bool list_only = false;
    char *log_name = NULL;
    uint32_t log_last_index = 0;
    uint32_t log_limit = 0;
    bool stream;
    bool partial_match = false;
    bool clear_log;
    bool dump_logs = true;
    struct walk_arg arg = {};
    int i;
    int rc;

    clear_log = false;
    for (i = 1; i < argc; ++i) {
        if (0 == strcmp(argv[i], "-l")) {
            list_only = true;
            break;
        }
        if (0 == strcmp(argv[i], "-n")) {
            if (i + 1 < argc) {
                arg.count_limit = parse_ll_bounds(argv[i + 1], 1, 1000000, &rc);
                if (rc) {
                    arg.count_limit = 1;
                }
                log_offset.lo_arg = &arg;
            }
            ++i;
            continue;
        }
        if (0 == strcmp(argv[i], "-s")) {
            if (i + 1 < argc) {
                arg.skip = parse_ll_bounds(argv[i + 1], 0, 1000000, &rc);
                if (rc) {
                    arg.skip = 0;
                }
                log_offset.lo_arg = &arg;
            }
            ++i;
            continue;
        }
        if (0 == strcmp(argv[i], "-t")) {
            dump_logs = false;
            continue;
        }

        /* the -c option is to clear a log (or logs). */
        if (!strcmp(argv[i], "-c")) {
            clear_log = true;
        } else if (isdigit((unsigned char)argv[i][0])) {
            log_limit = parse_ll_bounds(argv[i], 1, 1000000, &rc);
            if (clear_log) {
                goto err;
            }
        } else {
            log_name = argv[i];
            if ('*' == log_name[strlen(log_name) - 1]) {
                partial_match = true;
                log_name[strlen(log_name) - 1] = '\0';
            }
        }
    }

    log = NULL;
    while (1) {
        log = log_list_get_next(log);
        if (log == NULL) {
            break;
        }

        stream = log->l_log->log_type == LOG_TYPE_STREAM;

        if (list_only) {
            console_printf("%s%s\n", log->l_name,
                           stream ? " (stream)" : "");
            continue;
        }

        if (stream ||
            (log_name != NULL && ((partial_match && (log->l_name != strstr(log->l_name, log_name))) ||
                                  (!partial_match && 0 != strcmp(log->l_name, log_name))))) {
            continue;
        }

        if (clear_log) {
            console_printf("Clearing log %s\n", log->l_name);
            rc = log_flush(log);
            if (rc != 0) {
                goto err;
            }
        } else {
            if (dump_logs) {
                console_printf("Dumping log %s\n", log->l_name);
            }

            log_offset.lo_ts = 0;
            log_last_index = log_get_last_index(log);
            if (log_limit == 0 || log_last_index < log_limit) {
                log_offset.lo_index = 0;
            } else {
                log_offset.lo_index = log_last_index - log_limit;
            }
            log_offset.lo_data_len = 0;

            if (dump_logs) {
                arg.count = 0;
                rc = log_walk_body(log, shell_log_dump_entry, &log_offset);
            } else {
                /* Measure time for log_walk */
                shell_log_count = 0;
                os_time_t start = os_time_get();
                rc = log_walk_body(log, shell_log_count_entry, &log_offset);
                os_time_t end = os_time_get();
                console_printf("Log %s %d entries walked in %d ms\n", log->l_name,
                               (int)shell_log_count, (int)os_time_ticks_to_ms32(end - start));
            }
            if (rc != 0) {
                goto err;
            }
        }
    }

    return (0);
err:
    return (rc);
}


#if MYNEWT_VAL(LOG_STORAGE_INFO)
int
shell_log_storage_cmd(int argc, char **argv)
{
    struct log *log;
    struct log_storage_info info;

    log = NULL;
    while (1) {
        log = log_list_get_next(log);
        if (log == NULL) {
            break;
        }

        if (log->l_log->log_type == LOG_TYPE_STREAM) {
            continue;
        }

        if (log_storage_info(log, &info)) {
            console_printf("Storage info not supported for %s\n", log->l_name);
        } else {
            uint32_t entry_count = 0;
            log_get_entry_count(log, &entry_count);
            console_printf("%s: %d of %d used; %d entries\n", log->l_name,
                           (unsigned)info.used, (unsigned)info.size, (int)entry_count);
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
            console_printf("%s: %d of %d used by unread entries\n", log->l_name,
                           (unsigned)info.used_unread, (unsigned)info.size);
#endif
        }
    }

    return (0);
}
#endif

static int
log_fill_command(int argc, char **argv)
{
    const char *log_name;
    struct log *log;
    int num = 1;

    if (argc > 2) {
        log_name = argv[2];
        log = log_find(log_name);
    } else {
        log = log_list_get_next(NULL);
    }
    if (log == NULL) {
        console_printf("No log to fill\n");
        return -1;
    }
    if (argc > 1) {
        num = atoi(argv[1]);
        if (num <= 0 || num > 10000) {
            num = 1;
        }
    }

    for (int i = 0; i < num; ++i) {
        log_printf(log, MODLOG_MODULE_DFLT, LOG_LEVEL_INFO, "Log os_time %d", (int)os_time_get());
    }

    return 0;
}

static struct shell_cmd log_fill_cmd = {
    .sc_cmd = "log-fill",
    .sc_cmd_func = log_fill_command
};

void
shell_log_fill_register(void)
{
    shell_cmd_register(&log_fill_cmd);
}
#endif
