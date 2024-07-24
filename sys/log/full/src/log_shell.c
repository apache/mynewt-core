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
    char tmp[32 + 1];
    int off;
    int blksz;
    bool read_data = ueh->ue_etype != LOG_ETYPE_CBOR;
    bool read_hash = ueh->ue_flags & LOG_FLAGS_IMG_HASH;
#if MYNEWT_VAL(LOG_FLAGS_TLV_SUPPORT) && MYNEWT_VAL(LOG_TLV_NUM_ENTRIES)
    bool read_num_entries = ueh->ue_flags & LOG_FLAGS_TLV_SUPPORT;
#else
    bool read_num_entries = false;
#endif
    uint32_t entries = 0;

    dlen = min(len, 128);

    if (read_data) {
#if MYNEWT_VAL(LOG_FLAGS_TLV_SUPPORT)
        dlen -= log_len_in_medium(log, sizeof(struct log_tlv));

        rc = log_read_trailer(log, dptr, LOG_TLV_NUM_ENTRIES, &entries);
        if (!rc) {
            dlen -= log_len_in_medium(log, LOG_NUM_ENTRIES_SIZE);
        } else {
            console_printf("failed to read trailer\n");
        }
#endif
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

    if (read_num_entries) {
#if MYNEWT_VAL(LOG_FLAGS_TLV_SUPPORT)
        dlen -= log_len_in_medium(log, sizeof(struct log_tlv));
        rc = log_read_trailer(log, dptr, LOG_TLV_NUM_ENTRIES, &entries);
        if (!rc) {
            dlen -= log_len_in_medium(log, LOG_NUM_ENTRIES_SIZE);
        }
#endif
        console_printf("[ne=%u]", (unsigned int)entries);
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
    return 0;
}

int
shell_log_dump_cmd(int argc, char **argv)
{
    struct log *log;
    struct log_offset log_offset;
    bool list_only = false;
    char *log_name = NULL;
    uint32_t log_last_index = 0;
    uint32_t log_limit = 0;
    bool stream;
    bool partial_match = false;
    bool clear_log;
    bool num_entries = false;
    uint32_t entries;
    int i;
    int rc;

    clear_log = false;
    for (i = 1; i < argc; ++i) {
        if (0 == strcmp(argv[i], "-l")) {
            list_only = true;
            break;
        }

        /* the -c option is to clear a log (or logs). */
        if (!strcmp(argv[i], "-c")) {
            clear_log = true;
        } else if (argc == 3 && !strcmp(argv[i], "-ne")) {
            num_entries = true;
            log_name = argv[i+1];
            break;
        } else if (argc == 5 && !strcmp(argv[i], "-ne") &&
                   !strcmp(argv[i+2], "-i")) {
            num_entries = true;
            log_name = argv[i+1];
            log_limit = parse_ll_bounds(argv[i+3], 1, 1000000, &rc);
            break;
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
        } else if (num_entries) {
            rc = log_get_entries(log, log_limit, &entries);
            if (!rc) {
                console_printf("entries: %u\n", (unsigned int)entries);
            } else {
                console_printf("Invalid or empty log, rc=%d!\n", rc);
            }
        } else {
            console_printf("Dumping log %s\n", log->l_name);

            log_offset.lo_arg = NULL;
            log_offset.lo_ts = 0;
            log_last_index = log_get_last_index(log);
            if (log_limit == 0 || log_last_index < log_limit) {
                log_offset.lo_index = 0;
            } else {
                log_offset.lo_index = log_last_index - log_limit;
            }
            log_offset.lo_data_len = 0;

            rc = log_walk_body(log, shell_log_dump_entry, &log_offset);
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
            console_printf("%s: %d of %d used\n", log->l_name,
                           (unsigned)info.used, (unsigned)info.size);
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
            console_printf("%s: %d of %d used by unread entries\n", log->l_name,
                           (unsigned)info.used_unread, (unsigned)info.size);
#endif
        }
    }

    return (0);
}
#endif

#endif
