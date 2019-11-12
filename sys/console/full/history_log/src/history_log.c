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

#include <stdint.h>
#include <ctype.h>
#include <os/mynewt.h>
#if MYNEWT_VAL(LOG_FCB)
#include <fcb/fcb.h>
#endif
#if MYNEWT_VAL(LOG_FCB2)
#include <fcb/fcb2.h>
#endif
#include <log/log.h>
#include <console/history.h>

#define HISTORY_CACHE_SIZE MYNEWT_VAL(CONSOLE_HISTORY_CACHE_SIZE)

#if MYNEWT_VAL(LOG_FCB2) && defined(FLASH_AREA_CONSOLE_HISTORY) && !defined(MYNEWT_VAL_CONSOLE_HISTORY_LOG_NAME)
static struct fcb2 history_fcb;
static struct log history_fcb_log;
#endif
static struct log *history_log;

/*
 * Circular buffer for commands.
 * Lines are separated by '\0'.
 * history_ptr points to place where new command will be stored. It always
 * points to '\0'.
 * history_cache[history_ptr] == 0
 * history_cache[(history_ptr - 1) MOD CONSOLE_HISTORY_CACHE_SIZE] == 0
 *
 * COM1.COM2 ARG.COM3 -t...COMn.COMn-1 ARG.
 *                       ^--- history_ptr
 */
static char history_cache[HISTORY_CACHE_SIZE];

static size_t history_ptr;
/* Helper macros to move pointer in circular buffer */
#define PREV_PTR(p) ((p > history_cache) ? p - 1 : (history_cache + HISTORY_CACHE_SIZE - 1))
#define NEXT_PTR(p) ((p < history_cache + HISTORY_CACHE_SIZE - 1) ? p + 1 : history_cache)
#define PTR_ADD(p, a) (((p) + (a) < history_cache + HISTORY_CACHE_SIZE) ? (p) + (a) : (p) + (a) - HISTORY_CACHE_SIZE)

static int
console_history_add_to_cache(const char *line)
{
    char *cache_end = history_cache + history_ptr;
    /* Let p1 point to last null-terminator */
    char *p1 = PREV_PTR(cache_end);
    int len;
    const char *p2;
    int entry_num = 0;
    int found_at = -1;

    /* Trim from spaces */
    while (isspace(*line)) {
        line++;
    };

    len = strlen(line);
    if (len == 0) {
        return SYS_ENOENT;
    }

    /*
     * Trim trailing spaces. It does not touch input buffer, it just
     * corrects len variable.
     */
    while (isspace(line[len - 1])) {
        len--;
    }

    assert(*cache_end == 0);
    /* p1 should point to string terminator */
    assert(*p1 == 0);
    /* Now p1 point to last character of last command */
    p1 = PREV_PTR(p1);

    while (*p1) {
        /* Compare entry in cache with line starting from the end */
        for (p2 = line + len - 1; p2 >= line && *p1 == *p2; --p2, p1 = PREV_PTR(p1)) {
        }
        if (p2 < line && *p1 == '\0') {
            /* Line was in history cache already */
            if (entry_num == 0) {
                /* Last entry matched no need to do anything */
                return SYS_EALREADY;
            }
            found_at = entry_num;
            break;
        }
        /* Line did no match entry_num line from cache, go to the start of
         * entry */
        while (*p1) {
            p1 = PREV_PTR(p1);
        }
        /* Skip null terminator of previous entry */
        p1 = PREV_PTR(p1);
        entry_num++;
    }

    if (found_at < 0) {
        /* p1 will be used to store new line in cache */
        p1 = cache_end;
    } else {
        /*
         * Line was in the cache, rotate old data.
         * This will overwrite old copy of command.
         * Line will be added a new.
         */
        p1 = NEXT_PTR(p1);
        p2 = PTR_ADD(p1, len + 1);
        while (p2 != cache_end) {
            *p1 = *p2;
            p1 = NEXT_PTR(p1);
            p2 = NEXT_PTR(p2);
        }
    }
    /* Copy current line to the end of cache (including null terminator) */
    p2 = line;
    for (; len > 0; --len) {
        *p1 = *p2++;
        p1 = NEXT_PTR(p1);
    }
    *p1 = '\0';
    p1 = NEXT_PTR(p1);
    history_ptr = p1 - history_cache;

    /*
     * History pointer should point to '\0', if it is not destroy oldest
     * partial entry.
     */
    while (*p1) {
        *p1 = '\0';
        p1 = NEXT_PTR(p1);
    }

    return SYS_EOK;
}

static int
history_cache_from_log(struct log *log, struct log_offset *log_offset,
                       const struct log_entry_hdr *hdr,
                       const void *dptr, uint16_t len)
{
    char line[MYNEWT_VAL_CONSOLE_MAX_INPUT_LEN];

    if (len >= MYNEWT_VAL_CONSOLE_MAX_INPUT_LEN) {
        len = MYNEWT_VAL_CONSOLE_MAX_INPUT_LEN - 1;
    }
    if (hdr->ue_module == MYNEWT_VAL(CONSOLE_HISTORY_LOG_MODULE)) {
        log_read_body(log, dptr, line, 0, len);
        line[len] = '\0';
        (void)console_history_add_to_cache(line);
    }

    return 0;
}

void
console_history_add(const char *line)
{
    if (console_history_add_to_cache(line) == SYS_EOK && history_log) {
        log_printf(history_log, MYNEWT_VAL(CONSOLE_HISTORY_LOG_MODULE),
                   LOG_LEVEL_MAX, line);
    }
}

int
console_history_get(int num, char *buf, size_t buf_size)
{
    char *cache_end = history_cache + history_ptr;
    char *p1 = PREV_PTR(cache_end);
    size_t len = 0;
    size_t i;

    if (num < 0) {
        return SYS_EINVAL;
    }

    if (num == 0) {
        *buf = '\0';
        return 0;
    }
    assert(*cache_end == 0 && *p1 == 0);

    i = num;
    while (i) {
        len = 0;
        p1 = PREV_PTR(p1);
        while (*p1) {
            len++;
            p1 = PREV_PTR(p1);
        }
        if (len) {
            i--;
        } else {
            return SYS_EINVAL;
        }
    }
    if (len >= buf_size) {
        len = buf_size - 1;
    }
    for (i = 0; i <= len; ++i) {
        p1 = NEXT_PTR((p1));
        *buf++ = *p1;
    }

    return (int)num;
}

int
console_history_pkg_init(void)
{
    struct log_offset off = { 0 };

#if (MYNEWT_VAL(LOG_FCB) || MYNEWT_VAL(LOG_FCB2)) && defined(MYNEWT_VAL_CONSOLE_HISTORY_LOG_NAME)
    history_log = log_find(MYNEWT_VAL(CONSOLE_HISTORY_LOG_NAME));
#elif MYNEWT_VAL(LOG_FCB2) && defined(FLASH_AREA_CONSOLE_HISTORY)
    /* If there is dedicated flash area for shell history and FCB2 is enabled */
    fcb2_init_flash_area(&history_fcb, FLASH_AREA_CONSOLE_HISTORY, 0x12C9985, 1);
    if (log_register("con_hist", &history_fcb_log, &log_fcb_handler,
        &history_fcb, 0) == 0) {
        history_log = &history_fcb_log;
    }
#endif
    if (history_log) {
        log_module_register(MYNEWT_VAL(CONSOLE_HISTORY_LOG_MODULE), "CON-HIST");
        log_walk_body(history_log, history_cache_from_log, &off);
    }

    return 0;
}
