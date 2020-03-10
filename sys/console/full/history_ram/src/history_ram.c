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
#include <console/history.h>

#ifndef bssnz_t
/* Just in case bsp.h does not define it, in this case console history will
 * not be preserved across software resets
 */
#define bssnz_t
#endif

#define HISTORY_SIZE MYNEWT_VAL(CONSOLE_HISTORY_RAM_HISTORY_SIZE)

bssnz_t static char console_hist_lines[HISTORY_SIZE][ MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN) ];

bssnz_t static struct console_hist {
    uint32_t magic;
    uint8_t head;
    uint8_t count;
    char *lines[HISTORY_SIZE + 1];
} console_hist;

static size_t
trim_whitespace(const char *str, char *out, size_t out_size)
{
    const char *end;
    size_t len;

    if (out_size == 0) {
        return 0;
    }

    /* Skip leading space */
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == 0) { /* All spaces? */
        *out = 0;
        return 0;
    }

    /* Skip trailing space */
    end = str + strlen(str) - 1;
    while (isspace((unsigned char)*end)) {
        end--;
    }

    end++;

    /* Set output size to minimum of trimmed string length and buffer size minus 1 */
    len = min(end - str, out_size - 1);

    /* Copy trimmed string and add null terminator */
    memcpy(out, str, len);
    out[len] = 0;

    return len;
}

static uint8_t
ring_buf_next(uint8_t i)
{
    return (uint8_t) ((i + 1) % HISTORY_SIZE);
}

static uint8_t
ring_buf_prev(uint8_t i)
{
    return i == 0 ? HISTORY_SIZE - 1 : i - 1;
}

static bool
console_hist_is_full(void)
{
    return console_hist.count == HISTORY_SIZE;
}

static bool
console_hist_move_to_head(char *line)
{
    struct console_hist *sh = &console_hist;
    char *match = NULL;
    uint8_t prev;
    uint8_t curr;
    uint8_t left;

    left = sh->count;
    curr = ring_buf_prev(sh->head);
    while (left) {
        if (strcmp(sh->lines[curr], line) == 0) {
            match = sh->lines[curr];
            break;
        }
        curr = ring_buf_next(curr);
        left--;
    }

    if (!match) {
        return false;
    }

    prev = curr;
    curr = ring_buf_next(curr);
    while (curr != sh->head) {
        sh->lines[prev] = sh->lines[curr];
        prev = curr;
        curr = ring_buf_next(curr);
    }

    sh->lines[prev] = match;

    return true;
}

history_handle_t
console_history_add(const char *line)
{
    struct console_hist *sh = &console_hist;
    char buf[MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN)];
    size_t len;

    len = trim_whitespace(line, buf, sizeof(buf));
    if (len == 0) {
        return 0;
    }

    if (console_hist_move_to_head(buf)) {
        return 1;
    }

    strcpy(sh->lines[sh->head], buf);
    sh->head = ring_buf_next(sh->head);
    if (!console_hist_is_full()) {
        sh->count++;
    }
    return 1;
}

/**
 * Function returns line from history
 *
 * @param line_num 1 - based line counter, 1 - last line in history
 *
 * @return line with given number or NULL if line_num is out of range
 */
static const char *
console_history_line(uint8_t line_num)
{
    int head = console_hist.head;

    if (line_num > console_hist.count) {
        return NULL;
    } else {
        return console_hist.lines[(head + console_hist.count - line_num) % console_hist.count];
    }
}

history_handle_t
console_history_find(history_handle_t start, history_find_type_t search_type,
                     void *arg)
{
    const char *history_line;
    const char *pattern;
    size_t pattern_size;
    int num = 0;
    int direction;

    switch (search_type) {
    case HFT_PREV:
        num = start + (arg ? *(int *)arg : 1);
        if (num > console_hist.count) {
            num = 0;
        }
        break;
    case HFT_NEXT:
        num = start - (arg ? *(int *)arg : 1);
        if (num <= 0) {
            num = 0;
        }
        break;
    case HFT_MATCH_NEXT:
    case HFT_MATCH_PREV:
        direction = (search_type == HFT_MATCH_NEXT) ? -1 : 1;
        pattern = (const char *)arg;
        pattern_size = strlen(pattern);
        num = start;
        while (1) {
            num += direction;
            history_line = console_history_line(num);
            if (history_line == NULL) {
                num = 0;
            } else if (strncmp(history_line, pattern, pattern_size) != 0) {
                continue;
            }
            break;
        }
        break;
    default:
        break;
    }
    return num;
}

int
console_history_get(history_handle_t handle, size_t offset, char *buf,
                    size_t buf_size)
{
    const char *line;
    size_t line_len;
    int num = (int)handle;

    if (num > console_hist.count || num < 1) {
        return SYS_EINVAL;
    }

    if (num <= console_hist.head) {
        line = console_hist.lines[console_hist.head - num];
    } else {
        line = console_hist.lines[console_hist.head + HISTORY_SIZE - num];
    }

    line_len = strlen(line);
    if (line_len <= offset) {
        return 0;
    }
    line += offset;
    line_len -= offset;

    if (line_len > buf_size) {
        line_len = buf_size;
    }
    memcpy(buf, line, line_len);

    return line_len;
}

int
console_history_ram_pkg_init(void)
{
    struct console_hist *sh = &console_hist;
    int i;

    if (sh->magic != 0xBABEFACE) {
        memset(console_hist_lines, 0, sizeof(console_hist_lines));
        memset(&console_hist, 0, sizeof(console_hist));

        for (i = 0; i < HISTORY_SIZE; i++) {
            sh->lines[i] = console_hist_lines[i];
        }
        sh->magic = 0xBABEFACE;
    }

    return 0;
}
