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

#ifndef bssnz_t
/* Just in case bsp.h does not define it, in this case console history will
 * not be preserved across software resets
 */
#define bssnz_t
#endif

bssnz_t static char console_hist_lines[ MYNEWT_VAL(RAM_HISTORY_SIZE) ][ MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN) ];

bssnz_t static struct console_hist {
    uint32_t magic;
    uint8_t head;
    uint8_t count;
    char *lines[ MYNEWT_VAL(RAM_HISTORY_SIZE) ];
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
    return (uint8_t) ((i + 1) % MYNEWT_VAL(RAM_HISTORY_SIZE));
}

static uint8_t
ring_buf_prev(uint8_t i)
{
    return i == 0 ? i = MYNEWT_VAL(RAM_HISTORY_SIZE) - 1 : --i;
}

static bool
console_hist_is_full(void)
{
    return console_hist.count == MYNEWT_VAL(RAM_HISTORY_SIZE);
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

void
console_history_add(const char *line)
{
    struct console_hist *sh = &console_hist;
    char buf[MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN)];
    size_t len;

    len = trim_whitespace(line, buf, sizeof(buf));
    if (len == 0) {
        return;
    }

    if (console_hist_move_to_head(buf)) {
        return;
    }

    strcpy(sh->lines[sh->head], buf);
    sh->head = ring_buf_next(sh->head);
    if (!console_hist_is_full()) {
        sh->count++;
    }
}

int
console_history_get(int num, char *buf, size_t buf_size)
{
    const char *line;
    size_t line_len;

    if (num > console_hist.count || num < -console_hist.count) {
        return SYS_EINVAL;
    }

    if (num == 0) {
        line_len = 0;
    } else {
        if (num < 0) {
            num = console_hist.count + num + 1;
        }
        if (num <= console_hist.head) {
            line = console_hist.lines[console_hist.head - num];
        } else {
            line = console_hist.lines[console_hist.head +
                                      MYNEWT_VAL(RAM_HISTORY_SIZE) - num];
        }

        line_len = strlen(line);
        if (line_len >= buf_size) {
            line_len = buf_size - 1;
        }
        memcpy(buf, line, line_len);
    }
    buf[line_len] = '\0';

    return num;
}

int
console_history_pkg_init(void)
{
    struct console_hist *sh = &console_hist;
    int i;
    int size;

    if (sh->magic != 0xBABEFACE) {
        memset(console_hist_lines, 0, sizeof(console_hist_lines));
        memset(&console_hist, 0, sizeof(console_hist));

        size = MYNEWT_VAL(RAM_HISTORY_SIZE) + 1;

        for (i = 0; i < size - 1; i++) {
            sh->lines[i] = console_hist_lines[i];
        }
        sh->magic = 0xBABEFACE;
    }

    return 0;
}
