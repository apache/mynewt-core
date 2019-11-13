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

#define HISTORY_CACHE_SIZE MYNEWT_VAL(CONSOLE_HISTORY_LOG_CACHE_SIZE)

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
 * history_cache[(history_ptr - 1) MOD CONSOLE_HISTORY_LOG_CACHE_SIZE] == 0
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
#define PTR_SUB(p, a) (((p) - (a) >= history_cache) ? (p) - (a) : (p) - (a) + HISTORY_CACHE_SIZE)

/*
 * Given p that points to null terminator ending string stored in
 * history change circular buffer, returns pointer to null terminator
 * that is placed before string.
 */
static const char *
move_back(const char *p)
{
    if (p == NULL) {
        return NULL;
    }

    p = PREV_PTR(p);
    /* If *p now is 0, function was called on oldest element in history */
    if (*p == '\0') {
        return NULL;
    }
    /* Move to the beginning and beyond (just one char) so p points to '\0' */
    for (; *p; p = PREV_PTR(p)) ;

    return p;
}

/*
 * Given p that points to null terminator before string stored in
 * history change circular buffer, returns pointer to null terminator
 * that is just after string.
 */
static const char *
move_forward(const char *p)
{
    if (p == NULL) {
        return NULL;
    }

    p = NEXT_PTR(p);
    if (*p == '\0') {
        return NULL;
    }

    for (; *p; p = NEXT_PTR(p)) ;

    /*
     * If after moving forward to valid null terminator, next character is
     * also 0, search found head of circular buffer.  Just return NULL
     * in this case meaning that there is nothing to retrieve.
     */
    if (*(NEXT_PTR(p)) == '\0') {
        return NULL;
    }
    return p;
}

/*
 * Helper to move in both directions.
 */
static const char *
move(const char *p, history_find_type_t search_type)
{
    if (search_type & HFT_NEXT) {
        return move_forward(p);
    } else {
        return move_back(p);
    }
}

static history_handle_t
console_history_add_to_cache(const char *line)
{
    char *cache_end = history_cache + history_ptr;
    /* Let p1 point to last null-terminator */
    char *p1 = PREV_PTR(cache_end);
    int len;
    const char *p2;
    int entry_num = 0;
    int found_at = -1;
    history_handle_t result;

    if (line == NULL) {
        return SYS_EINVAL;
    }

    /* Trim from spaces */
    while (isspace(*line)) {
        line++;
    };

    len = strlen(line);
    if (len == 0) {
        return SYS_EINVAL;
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
    /* Now p1 point to last character of most recent history line */
    p1 = PREV_PTR(p1);

    while (*p1) {
        /* Compare entry in cache with line starting from the end */
        for (p2 = line + len - 1; p2 >= line && *p1 == *p2; --p2, p1 = PREV_PTR(p1)) ;
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
    /* Store result sine p1 will be modified */
    result = (history_handle_t)PREV_PTR(p1);

    /* Copy line to history */
    for (; len > 0; --len) {
        *p1 = *p2++;
        p1 = NEXT_PTR(p1);
    }
    *p1 = '\0';
    p1 = NEXT_PTR(p1);

    /* New head */
    history_ptr = p1 - history_cache;

    /*
     * History pointer should point to '\0', if it is not destroy oldest
     * partial entry.
     */
    while (*p1) {
        *p1 = '\0';
        p1 = NEXT_PTR(p1);
    }

    return result;
}

/*
 * Function will be called from log_walk and will add history lines.
 */
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

history_handle_t
console_history_add(const char *line)
{
    history_handle_t added_line;

    added_line = console_history_add_to_cache(line);

    if (added_line > 0 && history_log) {
        log_printf(history_log, MYNEWT_VAL(CONSOLE_HISTORY_LOG_MODULE),
                   LOG_LEVEL_MAX, line);
    }
    return added_line;
}

int
console_history_get(history_handle_t handle, size_t offset, char *buf,
                    size_t buf_size)
{
    const char *p1 = (const char *)handle;
    size_t copied;

    if (p1 == 0 || p1 < history_cache ||
        p1 >= history_cache + HISTORY_CACHE_SIZE ||
        *p1 != '\0') {
        return SYS_EINVAL;
    }

    p1 = NEXT_PTR(p1);
    for (; offset && *p1; --offset, p1 = NEXT_PTR(p1)) ;
    if (offset > 0) {
        return 0;
    }
    for (copied = 0; buf_size && *p1 != '\0'; --buf_size, ++copied) {
        *buf++ = *p1;
        p1 = NEXT_PTR(p1);
    }
    return copied;
}

history_handle_t
console_history_find(history_handle_t start, history_find_type_t search_type,
                     void *arg)
{
    const char *head = history_cache + history_ptr;
    const char *p1 = (const char *)start;
    const char *p2;
    const char *lp;
    const char *pattern_limit;
    int num;
    history_handle_t result = 0;

    if (p1 == 0) {
        p1 = PREV_PTR(head);
    }
    switch (search_type) {
    case HFT_PREV:
    case HFT_NEXT:
        num = arg ? *(int *)arg : 1;
        for (; num && p1 != NULL; --num) {
            p1 = move(p1, search_type);
        }
        result = (history_handle_t)p1;
        break;
    case HFT_MATCH_PREV:
    case HFT_MATCH_NEXT:
        pattern_limit = (const char *)arg + strlen((const char *)arg);
        for (lp = move(p1, search_type); lp != NULL; lp = move(lp, search_type)) {
            p2 = arg;
            p1 = NEXT_PTR(lp);
            for (; p2 < pattern_limit && *p1 == *p2; ++p2, p1 = NEXT_PTR(p1) ) ;
            if (p2 == pattern_limit) {
                result = (history_handle_t)lp;
                break;
            }
        }
        break;
    default:
        break;
    }
    return result;
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
