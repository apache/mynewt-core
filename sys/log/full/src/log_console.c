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

#if MYNEWT_VAL(LOG_CONSOLE)

#include <cbmem/cbmem.h>
#include <console/console.h>
#include "log/log.h"
#include "tinycbor/cbor.h"
#include "tinycbor/cbor_buf_reader.h"
#include "datetime/datetime.h"

static int
log_console_dump_cbor_entry(const void *dptr, uint16_t len)
{
    struct CborParser cbor_parser;
    struct CborValue cbor_value;
    struct cbor_buf_reader cbor_buf_reader;

    cbor_buf_reader_init(&cbor_buf_reader, dptr, len);
    cbor_parser_init(&cbor_buf_reader.r, 0, &cbor_parser, &cbor_value);
    cbor_value_to_pretty(stdout, &cbor_value);

    console_write("\n", 1);
    return 0;
}

static struct log log_console;

struct log *
log_console_get(void)
{
    return &log_console;
}

#if MYNEWT_VAL(LOG_CONSOLE_PRETTY)
#define CSI                     "\x1b["
#define COLOR_BLACK             CSI "30m"
#define COLOR_RED               CSI "31m"
#define COLOR_GREEN             CSI "32m"
#define COLOR_YELLOW            CSI "33m"
#define COLOR_BLUE              CSI "34m"
#define COLOR_MAGENTA           CSI "35m"
#define COLOR_CYAN              CSI "36m"
#define COLOR_WHITE             CSI "37m"
#define COLOR_BLACK_BG          CSI "40m"
#define COLOR_RED_BG            CSI "41m"
#define COLOR_GREEN_BG          CSI "42m"
#define COLOR_YELLOW_BG         CSI "43m"
#define COLOR_WHITE_BG          CSI "47m"

#if MYNEWT_VAL(LOG_CONSOLE_PRETTY_WITH_COLORS)
#define COLOR_DBG               COLOR_CYAN
#define COLOR_INF               ""
#define COLOR_WRN               COLOR_YELLOW
#define COLOR_ERR               COLOR_RED
#define COLOR_CRI               COLOR_RED_BG
#define COLOR_RESET             CSI "0m"

const char *const module_colors[] = {
    COLOR_GREEN COLOR_BLACK_BG,
    COLOR_RED COLOR_WHITE_BG,
    COLOR_BLUE,
    COLOR_YELLOW COLOR_BLACK_BG,
    COLOR_MAGENTA COLOR_WHITE_BG,
    COLOR_CYAN COLOR_BLACK_BG,
    COLOR_WHITE COLOR_RED_BG,
    COLOR_BLACK COLOR_GREEN_BG,
    COLOR_BLACK COLOR_YELLOW_BG,
};

static uint8_t log_module_color_index[10];

static void
log_module_color(uint8_t module, char *color_on, char *color_off)
{
    int i;

    *color_on = 0;
    *color_off = 0;

    if (module) {
        for (i = 0; i < ARRAY_SIZE(log_module_color_index) &&
             log_module_color_index[i] != 0 &&
             log_module_color_index[i] != module; ++i) {
        }
        if (i < ARRAY_SIZE(log_module_color_index)) {
            if (log_module_color_index[i] == 0) {
                log_module_color_index[i] = module;
            }
            strcpy(color_on, module_colors[i]);
            strcpy(color_off, COLOR_RESET);
        }
    }
}

#else
#define COLOR_DBG               ""
#define COLOR_INF               ""
#define COLOR_WRN               ""
#define COLOR_ERR               ""
#define COLOR_CRI               ""
#define COLOR_RESET             ""
#define log_module_color(hdr, on, off)
#endif

static const char * const log_level_color[] = {
    COLOR_DBG,
    COLOR_INF,
    COLOR_WRN,
    COLOR_ERR,
    COLOR_CRI,
};

static const char * const log_level_str[] = {
    "DBG",
    "INF",
    "WRN",
    "ERR",
    "CRI",
};

void
log_console_print_hdr(const struct log_entry_hdr *hdr)
{
    char module_num[10];
    char image_hash_str[17];
    char level_str_buf[23];
    const char *level_str = "";
    const char *module_name = NULL;
    char color[11];
    char color_off[6];

    /* Find module defined in syscfg.logcfg sections */
    module_name = log_module_get_name(hdr->ue_module);

    if (MYNEWT_VAL(LOG_CONSOLE_PRETTY_WITH_COLORS)) {
        log_module_color(hdr->ue_module, color, color_off);
    } else {
        color[0] = 0;
        color_off[0] = 0;
    }

    if (module_name == NULL) {
        module_name = module_num;
        sprintf(module_num, "mod=%u", hdr->ue_module);
    }
    if (hdr->ue_flags & LOG_FLAGS_IMG_HASH) {
        sprintf(image_hash_str, "[ih=0x%02x%02x%02x%02x]", hdr->ue_imghash[0], hdr->ue_imghash[1],
                hdr->ue_imghash[2], hdr->ue_imghash[3]);
    } else {
        image_hash_str[0] = 0;
    }
    if (hdr->ue_level <= LOG_LEVEL_CRITICAL) {
        if (MYNEWT_VAL(LOG_CONSOLE_PRETTY_WITH_COLORS)) {
            strcpy(level_str_buf, log_level_color[hdr->ue_level]);
            strcat(level_str_buf, log_level_str[hdr->ue_level]);
            strcat(level_str_buf, COLOR_RESET);
            level_str = level_str_buf;
        } else {
            level_str = log_level_str[hdr->ue_level];
        }
    } else {
        sprintf(level_str_buf, "%-3u", hdr->ue_level);
        level_str = level_str_buf;
    }

    if (MYNEWT_VAL(LOG_CONSOLE_PRETTY_WITH_TIMESTAMP) &&
        MYNEWT_VAL(LOG_CONSOLE_PRETTY_TIMESTAMP_CLOCK)) {
        struct os_timeval tv = {
            .tv_usec = (unsigned int)hdr->ue_ts % 1000000,
            .tv_sec = (unsigned int)(hdr->ue_ts / 1000000),
        };
        struct clocktime ct;
        timeval_to_clocktime(&tv, NULL, &ct);
        console_printf("[%d-%02d-%02dT%02d:%02d:%02d.%06u][%s%7s%s][%s]%s ",
                       ct.year, ct.mon, ct.day,
                       ct.hour, ct.min, ct.sec, ct.usec,
                       color, module_name, color_off, level_str, image_hash_str);
    } else if (MYNEWT_VAL(LOG_CONSOLE_PRETTY_WITH_TIMESTAMP)) {
        unsigned int us = (unsigned int)hdr->ue_ts % 1000000;
        unsigned int s = (unsigned int)(hdr->ue_ts / 1000000);
        console_printf("[%u.%06u][%s%7s%s][%s]%s ", s, us, color, module_name, color_off, level_str, image_hash_str);
    } else {
        console_printf("[%s%7s%s][%s]%s ", color, module_name, color_off, level_str, image_hash_str);
    }
}

#else
void
log_console_print_hdr(const struct log_entry_hdr *hdr)
{
    console_printf("[ts=" "%" PRIu64 "us, mod=%u level=%u ",
                   hdr->ue_ts, hdr->ue_module, hdr->ue_level);

    if (hdr->ue_flags & LOG_FLAGS_IMG_HASH) {
        console_printf("ih=0x%02x%02x%02x%02x", hdr->ue_imghash[0], hdr->ue_imghash[1],
                       hdr->ue_imghash[2], hdr->ue_imghash[3]);
    }
    console_printf("]");
}
#endif

static int
log_console_append_body(struct log *log, const struct log_entry_hdr *hdr,
                        const void *body, int body_len)
{
    if (!console_is_init()) {
        return (0);
    }

    if (!console_is_midline) {
        log_console_print_hdr(hdr);
    }

    if (hdr->ue_etype != LOG_ETYPE_CBOR) {
        console_write(body, body_len);
    } else {
        log_console_dump_cbor_entry(body, body_len);
    }
    return (0);
}

static int
log_console_append(struct log *log, void *buf, int len)
{
    int hdr_len;

    hdr_len = log_hdr_len(buf);

    return log_console_append_body(log, buf, (uint8_t *)buf + hdr_len,
                                   len - hdr_len);
}

static int
log_console_read(struct log *log, const void *dptr, void *buf, uint16_t offset,
        uint16_t len)
{
    /* You don't read console, console read you */
    return (OS_EINVAL);
}

static int
log_console_walk(struct log *log, log_walk_func_t walk_func,
        struct log_offset *log_offset)
{
    /* You don't walk console, console walk you. */
    return (OS_EINVAL);
}

static int
log_console_flush(struct log *log)
{
    /* You don't flush console, console flush you. */
    return (OS_EINVAL);
}

const struct log_handler log_console_handler = {
    .log_type = LOG_TYPE_STREAM,
    .log_read = log_console_read,
    .log_append = log_console_append,
    .log_append_body = log_console_append_body,
    .log_walk = log_console_walk,
    .log_flush = log_console_flush,
};

void
log_console_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = log_register("console", &log_console, &log_console_handler, NULL,
                      MYNEWT_VAL(LOG_LEVEL));
    SYSINIT_PANIC_ASSERT(rc == 0);
}

#endif
