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

#include <inttypes.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdio.h>

#include "os/os.h"

#include "console.h"

/* Control characters */
#define ESC                0x1b
#define DEL                0x7f

/* ANSI escape sequences */
#define ANSI_ESC           '['
#define ANSI_UP            'A'
#define ANSI_DOWN          'B'
#define ANSI_FORWARD       'C'
#define ANSI_BACKWARD      'D'
#define ANSI_END           'F'
#define ANSI_HOME          'H'
#define ANSI_DEL           '~'

#define ESC_ESC         (1 << 0)
#define ESC_ANSI        (1 << 1)
#define ESC_ANSI_FIRST  (1 << 2)
#define ESC_ANSI_VAL    (1 << 3)
#define ESC_ANSI_VAL_2  (1 << 4)

static int esc_state;
static unsigned int ansi_val, ansi_val_2;

static uint8_t cur, end;
static struct os_eventq *avail_queue;
static struct os_eventq *lines_queue;
static uint8_t (*completion_cb)(char *line, uint8_t len);

typedef int (*stdout_func_t)(int);

static int
console_out_hook_default(int c)
{
    return EOF;
}

static stdout_func_t console_out = console_out_hook_default;
extern stdout_func_t _get_stdout_hook(void);

void
console_printf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

size_t
console_file_write(void *arg, const char *str, size_t cnt)
{
    int i;

    for (i = 0; i < cnt; i++) {
        console_out(str[i]);
    }
    return cnt;
}

void
console_write(const char *str, int cnt)
{
    console_file_write(NULL, str, cnt);
}

static inline void
cursor_forward(unsigned int count)
{
    console_printf("\x1b[%uC", count);
}

static inline void
cursor_backward(unsigned int count)
{
    console_printf("\x1b[%uD", count);
}

static inline void
cursor_save(void)
{
    console_out(ESC);
    console_out('[');
    console_out('s');
}

static inline void
cursor_restore(void)
{
    console_out(ESC);
    console_out('[');
    console_out('u');
}

static void
insert_char(char *pos, char c, uint8_t end)
{
    char tmp;

    /* Echo back to console */
    console_out(c);

    if (end == 0) {
        *pos = c;
        return;
    }

    tmp = *pos;
    *(pos++) = c;

    cursor_save();

    while (end-- > 0) {
        console_out(tmp);
        c = *pos;
        *(pos++) = tmp;
        tmp = c;
    }

    /* Move cursor back to right place */
    cursor_restore();
}

static void
del_char(char *pos, uint8_t end)
{
    console_out('\b');

    if (end == 0) {
        console_out(' ');
        console_out('\b');
        return;
    }

    cursor_save();

    while (end-- > 0) {
        *pos = *(pos + 1);
        console_out(*(pos++));
    }

    console_out(' ');

    /* Move cursor back to right place */
    cursor_restore();
}

static void
handle_ansi(uint8_t byte, char *line)
{
    if (esc_state & ESC_ANSI_FIRST) {
        esc_state &= ~ESC_ANSI_FIRST;
        if (!isdigit(byte)) {
            ansi_val = 1;
            goto ansi_cmd;
        }

        esc_state |= ESC_ANSI_VAL;
        ansi_val = byte - '0';
        ansi_val_2 = 0;
        return;
    }

    if (esc_state & ESC_ANSI_VAL) {
        if (isdigit(byte)) {
            if (esc_state & ESC_ANSI_VAL_2) {
                ansi_val_2 *= 10;
                ansi_val_2 += byte - '0';
            } else {
                ansi_val *= 10;
                ansi_val += byte - '0';
            }
            return;
        }

        /* Multi value sequence, e.g. Esc[Line;ColumnH */
        if (byte == ';' && !(esc_state & ESC_ANSI_VAL_2)) {
            esc_state |= ESC_ANSI_VAL_2;
            return;
        }

        esc_state &= ~ESC_ANSI_VAL;
        esc_state &= ~ESC_ANSI_VAL_2;
    }

ansi_cmd:
    switch (byte) {
    case ANSI_BACKWARD:
        if (ansi_val > cur) {
            break;
        }

        end += ansi_val;
        cur -= ansi_val;
        cursor_backward(ansi_val);
        break;
    case ANSI_FORWARD:
        if (ansi_val > end) {
            break;
        }

        end -= ansi_val;
        cur += ansi_val;
        cursor_forward(ansi_val);
        break;
    case ANSI_HOME:
        if (!cur) {
            break;
        }

        cursor_backward(cur);
        end += cur;
        cur = 0;
        break;
    case ANSI_END:
        if (!end) {
            break;
        }

        cursor_forward(end);
        cur += end;
        end = 0;
        break;
    case ANSI_DEL:
        if (!end) {
            break;
        }

        cursor_forward(1);
        del_char(&line[cur], --end);
        break;
    default:
        break;
    }

    esc_state &= ~ESC_ANSI;
}

int
console_handle_char(uint8_t byte)
{
    static struct os_event *ev;
    static struct console_input *input;

    if (!ev) {
        ev = os_eventq_get_no_wait(avail_queue);
        if (!ev)
            return 0;
        input = ev->ev_arg;
    }

    /* Handle ANSI escape mode */
    if (esc_state & ESC_ANSI) {
        handle_ansi(byte, input->line);
        return 0;
    }

    /* Handle escape mode */
    if (esc_state & ESC_ESC) {
        esc_state &= ~ESC_ESC;
        handle_ansi(byte, input->line);
        switch (byte) {
        case ANSI_ESC:
            esc_state |= ESC_ANSI;
            esc_state |= ESC_ANSI_FIRST;
            break;
        default:
            break;
        }

        return 0;
    }

    /* Handle special control characters */
    if (!isprint(byte)) {
        handle_ansi(byte, input->line);
        switch (byte) {
        case DEL:
            if (cur > 0) {
                del_char(&input->line[--cur], end);
            }
            break;
        case ESC:
            esc_state |= ESC_ESC;
            break;
        case '\r':
            input->line[cur + end] = '\0';
            console_out('\r');
            console_out('\n');
            cur = 0;
            end = 0;
            os_eventq_put(lines_queue, ev);
            input = NULL;
            ev = NULL;
            break;
        case '\t':
            if (completion_cb && !end) {
                cur += completion_cb(input->line, cur);
            }
            break;
        default:
            break;
        }

        return 0;
    }

    /* Ignore characters if there's no more buffer space */
    if (cur + end < sizeof(input->line) - 1) {
        insert_char(&input->line[cur++], byte, end);
    }
    return 0;
}

int
console_init(struct os_eventq *avail, struct os_eventq *lines,
             uint8_t (*completion)(char *str, uint8_t len))
{
    int rc = 0;

    avail_queue = avail;
    lines_queue = lines;
    completion_cb = completion;

#if MYNEWT_VAL(CONSOLE_UART)
    rc = uart_console_init();
#elif MYNEWT_VAL(CONSOLE_RTT)
    rc = rtt_console_init();
#endif
    console_out = _get_stdout_hook();
    return rc;
}
