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
#include <inttypes.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "os/mynewt.h"
#include "console/console.h"
#include "console/ticks.h"
#include "console_priv.h"

/* Control characters */
#define ESC                0x1b
#define DEL                0x7f
#define BS                 0x08

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

#define CONSOLE_NLIP_PKT_START1 (6)
#define CONSOLE_NLIP_PKT_START2 (9)
#define CONSOLE_NLIP_DATA_START1 (4)
#define CONSOLE_NLIP_DATA_START2 (20)

#define NLIP_PKT_START1  (1 << 0)
#define NLIP_PKT_START2  (1 << 1)
#define NLIP_DATA_START1 (1 << 2)
#define NLIP_DATA_START2 (1 << 3)

/* Indicates whether the previous line of output was completed. */
int console_is_midline;

#if MYNEWT_VAL(CONSOLE_COMPAT)
#define CONSOLE_COMPAT_MAX_CMD_QUEUED 1
static struct console_input cons_buf[CONSOLE_COMPAT_MAX_CMD_QUEUED];
static struct os_event cons_shell_ev[CONSOLE_COMPAT_MAX_CMD_QUEUED];
static console_rx_cb cons_compat_rx_cb; /* callback that input is ready */
static struct os_eventq compat_avail_queue;
static struct os_eventq compat_lines_queue;
#endif

static int cons_esc_state;
static int cons_nlip_state;
static int cons_echo = MYNEWT_VAL(CONSOLE_ECHO);
static unsigned int cons_ansi_val, cons_ansi_val_2;
static bool cons_rx_stalled;

static uint16_t cons_cur, cons_end;
static struct os_eventq cons_avail_queue;
static struct os_eventq *cons_lines_queue;
static console_completion_cb cons_completion;

/*
 * Default implementation in case all consoles are disabled - we just ignore any
 * output to console.
 */
int __attribute__((weak))
console_out(int c)
{
    return c;
}

void
console_echo(int on)
{
    cons_echo = on;
}

void
console_write(const char *str, int cnt)
{
    int i;

    for (i = 0; i < cnt; i++) {
        if (console_out((int)str[i]) == EOF) {
            break;
        }
    }
}

#if MYNEWT_VAL(CONSOLE_COMPAT)
int
console_read(char *str, int cnt, int *newline)
{
    struct os_event *ev;
    struct console_input *cmd;
    size_t len;

    *newline = 0;
    ev = os_eventq_get_no_wait(cons_lines_queue);
    if (!ev) {
        return 0;
    }
    cmd = ev->ev_arg;
    len = strlen(cmd->line);

    if ((cnt - 1) < len) {
        len = cnt - 1;
    }

    if (len > 0) {
        memcpy(str, cmd->line, len);
        str[len] = '\0';
    } else {
        str[0] = cmd->line[0];
    }

    console_line_event_put(ev);

    *newline = 1;
    return len;
}
#endif

void
console_blocking_mode(void)
{
#if MYNEWT_VAL(CONSOLE_UART)
    uart_console_blocking_mode();
#endif
}

void
console_non_blocking_mode(void)
{
#if MYNEWT_VAL(CONSOLE_UART)
    uart_console_non_blocking_mode();
#endif
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

#if MYNEWT_VAL(CONSOLE_HISTORY_SIZE) > 0
static inline void
cursor_clear_line(void)
{
    console_out(ESC);
    console_out('[');
    console_out('K');
}
#endif

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
insert_char(char *pos, char c, uint16_t end)
{
    char tmp;

    if (cons_cur + end >= MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN) - 1) {
        return;
    }

    if (cons_echo) {
        /* Echo back to console */
        console_out(c);
    }

    ++cons_cur;

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
del_char(char *pos, uint16_t end)
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

#if MYNEWT_VAL(CONSOLE_HISTORY_SIZE) > 0
static char console_hist_lines[ MYNEWT_VAL(CONSOLE_HISTORY_SIZE) ][ MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN) ];

static struct console_hist {
    uint8_t head;
    uint8_t tail;
    uint8_t size;
    uint8_t curr;
    char *lines[ MYNEWT_VAL(CONSOLE_HISTORY_SIZE) + 1 ];
} console_hist;

static void
console_hist_init(void)
{
    struct console_hist *sh = &console_hist;
    int i;

    memset(console_hist_lines, 0, sizeof(console_hist_lines));
    memset(&console_hist, 0, sizeof(console_hist));

    sh->size = MYNEWT_VAL(CONSOLE_HISTORY_SIZE) + 1;

    for (i = 0; i < sh->size - 1; i++) {
        sh->lines[i] = console_hist_lines[i];
    }
}

static size_t
trim_whitespace(const char *str, size_t len, char *out)
{
    const char *end;
    size_t out_size;

    if (len == 0) {
        return 0;
    }

    /* Trim leading space */
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == 0) { /* All spaces? */
        *out = 0;
        return 0;
    }

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    end++;

    /* Set output size to minimum of trimmed string length and buffer size minus 1 */
    out_size = min(end - str, len - 1);

    /* Copy trimmed string and add null terminator */
    memcpy(out, str, out_size);
    out[out_size] = 0;

    return out_size;
}

static uint8_t
ring_buf_next(uint8_t i, uint8_t size)
{
    return (uint8_t) ((i + 1) % size);
}

static uint8_t
ring_buf_prev(uint8_t i, uint8_t size)
{
    return i == 0 ? i = size - 1 : --i;
}

static bool
console_hist_is_full(void)
{
    struct console_hist *sh = &console_hist;

    return ring_buf_next(sh->head, sh->size) == sh->tail;
}

static bool
console_hist_move_to_head(char *line)
{
    struct console_hist *sh = &console_hist;
    char *match = NULL;
    uint8_t prev, curr;

    curr = sh->tail;
    while (curr != sh->head) {
        if (strcmp(sh->lines[curr], line) == 0) {
            match = sh->lines[curr];
            break;
        }
        curr = ring_buf_next(curr, sh->size);
    }

    if (!match) {
        return false;
    }

    prev = curr;
    curr = ring_buf_next(curr, sh->size);
    while (curr != sh->head) {
        sh->lines[prev] = sh->lines[curr];
        prev = curr;
        curr = ring_buf_next(curr, sh->size);
    }

    sh->lines[prev] = match;

    return true;
}

static void
console_hist_add(char *line)
{
    struct console_hist *sh = &console_hist;
    char buf[MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN)];
    size_t len;

    /* Reset current pointer */
    sh->curr = sh->head;

    len = trim_whitespace(line, sizeof(buf), buf);
    if (!len) {
        return;
    }

    if (console_hist_move_to_head(buf)) {
        return;
    }

    if (console_hist_is_full()) {
        /*
         * We have N buffers, but there are N+1 items in queue so one element is
         * always empty. If queue is full we need to move buffer from oldest
         * entry to current head and trim queue tail.
         */
        assert(sh->lines[sh->head] == NULL);
        sh->lines[sh->head] = sh->lines[sh->tail];
        sh->lines[sh->tail] = NULL;
        sh->tail = ring_buf_next(sh->tail, sh->size);
    }

    strcpy(sh->lines[sh->head], buf);
    sh->head = ring_buf_next(sh->head, sh->size);

    /* Reset current pointer */
    sh->curr = sh->head;
}

static void
console_clear_line(void)
{
    if (cons_cur) {
        cursor_backward(cons_cur);
    }
    cons_cur = 0;
    cons_end = 0;

    cursor_clear_line();
}

static void
console_hist_move(char *line, uint8_t direction)
{
    struct console_hist *sh = &console_hist;
    char *str = NULL;
    uint8_t limit = direction ==  ANSI_UP ? sh->tail : sh->head;

    /* no more history to return in this direction */
    if (sh->curr == limit) {
        return;
    }

    if (direction == ANSI_UP) {
        sh->curr = ring_buf_prev(sh->curr, sh->size);
    } else {
        sh->curr = ring_buf_next(sh->curr, sh->size);
    }

    console_clear_line();
    str = sh->lines[sh->curr];
    while (str && *str != '\0') {
        insert_char(&line[cons_cur], *str, cons_end);
        ++str;
    }
}
#endif

static void
handle_ansi(uint8_t byte, char *line)
{
    if (cons_esc_state & ESC_ANSI_FIRST) {
        cons_esc_state &= ~ESC_ANSI_FIRST;
        if (!isdigit(byte)) {
            cons_ansi_val = 1;
            goto ansi_cmd;
        }

        cons_esc_state |= ESC_ANSI_VAL;
        cons_ansi_val = byte - '0';
        cons_ansi_val_2 = 0;
        return;
    }

    if (cons_esc_state & ESC_ANSI_VAL) {
        if (isdigit(byte)) {
            if (cons_esc_state & ESC_ANSI_VAL_2) {
                cons_ansi_val_2 *= 10;
                cons_ansi_val_2 += byte - '0';
            } else {
                cons_ansi_val *= 10;
                cons_ansi_val += byte - '0';
            }
            return;
        }

        /* Multi value sequence, e.g. Esc[Line;ColumnH */
        if (byte == ';' && !(cons_esc_state & ESC_ANSI_VAL_2)) {
            cons_esc_state |= ESC_ANSI_VAL_2;
            return;
        }

        cons_esc_state &= ~ESC_ANSI_VAL;
        cons_esc_state &= ~ESC_ANSI_VAL_2;
    }

ansi_cmd:
    switch (byte) {
#if MYNEWT_VAL(CONSOLE_HISTORY_SIZE) > 0
    case ANSI_UP:
    case ANSI_DOWN:
        console_blocking_mode();
        console_hist_move(line, byte);
        console_non_blocking_mode();
        break;
#endif
    case ANSI_BACKWARD:
        if (cons_ansi_val > cons_cur) {
            break;
        }

        cons_end += cons_ansi_val;
        cons_cur -= cons_ansi_val;
        cursor_backward(cons_ansi_val);
        break;
    case ANSI_FORWARD:
        if (cons_ansi_val > cons_end) {
            break;
        }

        cons_end -= cons_ansi_val;
        cons_cur += cons_ansi_val;
        cursor_forward(cons_ansi_val);
        break;
    case ANSI_HOME:
        if (!cons_cur) {
            break;
        }

        cursor_backward(cons_cur);
        cons_end += cons_cur;
        cons_cur = 0;
        break;
    case ANSI_END:
        if (!cons_end) {
            break;
        }

        cursor_forward(cons_end);
        cons_cur += cons_end;
        cons_end = 0;
        break;
    case ANSI_DEL:
        if (!cons_end) {
            break;
        }

        cursor_forward(1);
        del_char(&line[cons_cur], --cons_end);
        break;
    default:
        break;
    }

    cons_esc_state &= ~ESC_ANSI;
}

static int
handle_nlip(uint8_t byte)
{
    if (((cons_nlip_state & NLIP_PKT_START1) &&
         (cons_nlip_state & NLIP_PKT_START2)) ||
        ((cons_nlip_state & NLIP_DATA_START1) &&
         (cons_nlip_state & NLIP_DATA_START2)))
    {
        return 1;
    }

    if ((cons_nlip_state & NLIP_PKT_START1) &&
        (byte == CONSOLE_NLIP_PKT_START2)) {
        cons_nlip_state |= NLIP_PKT_START2;
        return 1;
    } else if ((cons_nlip_state & NLIP_DATA_START1) &&
               (byte == CONSOLE_NLIP_DATA_START2)) {
        cons_nlip_state |= NLIP_DATA_START2;
        return 1;
    } else {
        cons_nlip_state = 0;
        return 0;
    }
}

static int
console_append_char(char *line, uint8_t byte)
{
    if (cons_cur + cons_end >= MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN) - 1) {
        return 0;
    }

    line[cons_cur + cons_end] = byte;

    if (byte == '\0') {
        return 1;
    }

    if (cons_echo) {
        /* Echo back to console */
        console_out(byte);
    }
    ++cons_cur;
    return 1;
}

int
console_handle_char(uint8_t byte)
{
#if !MYNEWT_VAL(CONSOLE_INPUT)
    return 0;
#endif

    static struct os_event *ev;
    static struct console_input *input;
    static char prev_endl = '\0';

    if (!cons_lines_queue) {
        return 0;
    }

    if (!ev) {
        ev = os_eventq_get_no_wait(&cons_avail_queue);
        if (!ev) {
            cons_rx_stalled = true;
            return -1;
        }
        input = ev->ev_arg;
    }

    if (handle_nlip(byte))  {
        if (byte == '\n') {
            insert_char(&input->line[cons_cur], byte, cons_end);
            input->line[cons_cur] = '\0';
            cons_cur = 0;
            cons_end = 0;
            os_eventq_put(cons_lines_queue, ev);
            cons_nlip_state = 0;

#if MYNEWT_VAL(CONSOLE_COMPAT)
            if (cons_compat_rx_cb) {
                cons_compat_rx_cb();
            }
#endif

            input = NULL;
            ev = NULL;
            console_echo(1);
            return 0;
        /* Ignore characters if there's no more buffer space */
        } else if (byte == CONSOLE_NLIP_PKT_START2) {
            /* Disable echo to not flood the UART */
            console_echo(0);
            insert_char(&input->line[cons_cur], CONSOLE_NLIP_PKT_START1, cons_end);
        } else if (byte == CONSOLE_NLIP_DATA_START2) {
            /* Disable echo to not flood the UART */
            console_echo(0);
            insert_char(&input->line[cons_cur], CONSOLE_NLIP_DATA_START1, cons_end);
        }

        insert_char(&input->line[cons_cur], byte, cons_end);

        return 0;
    }

    /* Handle ANSI escape mode */
    if (cons_esc_state & ESC_ANSI) {
        handle_ansi(byte, input->line);
        return 0;
    }

    /* Handle escape mode */
    if (cons_esc_state & ESC_ESC) {
        cons_esc_state &= ~ESC_ESC;
        handle_ansi(byte, input->line);
        switch (byte) {
        case ANSI_ESC:
            cons_esc_state |= ESC_ANSI;
            cons_esc_state |= ESC_ANSI_FIRST;
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
        case CONSOLE_NLIP_PKT_START1:
            cons_nlip_state |= NLIP_PKT_START1;
            break;
        case CONSOLE_NLIP_DATA_START1:
            cons_nlip_state |= NLIP_DATA_START1;
            break;
        case DEL:
        case BS:
            if (cons_cur > 0) {
                del_char(&input->line[--cons_cur], cons_end);
            }
            break;
        case ESC:
            cons_esc_state |= ESC_ESC;
            break;
        default:
            insert_char(&input->line[cons_cur], byte, cons_end);
            /* Falls through. */
        case '\r':
            /* Falls through. */
        case '\n':
            if (byte == '\n' && prev_endl == '\r') {
                /* handle double end line chars */
                prev_endl = byte;
                break;
            }
            prev_endl = byte;
            input->line[cons_cur + cons_end] = '\0';
            console_out('\r');
            console_out('\n');
            cons_cur = 0;
            cons_end = 0;
            os_eventq_put(cons_lines_queue, ev);
#if MYNEWT_VAL(CONSOLE_HISTORY_SIZE) > 0
            console_hist_add(input->line);
#endif

#if MYNEWT_VAL(CONSOLE_COMPAT)
            if (cons_compat_rx_cb) {
                cons_compat_rx_cb();
            }
#endif

            input = NULL;
            ev = NULL;
            break;
        case '\t':
            if (cons_completion && !cons_end) {
#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) == 0
                console_blocking_mode();
#endif
                cons_completion(input->line, console_append_char);
#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) == 0
                console_non_blocking_mode();
#endif
            }
            break;
        }

        return 0;
    }

    insert_char(&input->line[cons_cur], byte, cons_end);
    return 0;
}

int
console_is_init(void)
{
#if MYNEWT_VAL(CONSOLE_UART)
    return uart_console_is_init();
#endif
#if MYNEWT_VAL(CONSOLE_RTT)
    return rtt_console_is_init();
#endif
#if MYNEWT_VAL(CONSOLE_BLE_MONITOR)
    return ble_monitor_console_is_init();
#endif
    return 0;
}

void
console_line_queue_set(struct os_eventq *evq)
{
    cons_lines_queue = evq;
}

void
console_line_event_put(struct os_event *ev)
{
    os_eventq_put(&cons_avail_queue, ev);

    if (cons_rx_stalled) {
        cons_rx_stalled = false;
        console_rx_restart();
    }
}

void
console_set_completion_cb(console_completion_cb cb)
{
    cons_completion = cb;
}

#if MYNEWT_VAL(CONSOLE_COMPAT)
int
console_init(console_rx_cb rx_cb)
{
    int i;

    os_eventq_init(&compat_lines_queue);
    os_eventq_init(&compat_avail_queue);
    console_line_queue_set(&compat_lines_queue);

    for (i = 0; i < CONSOLE_COMPAT_MAX_CMD_QUEUED; i++) {
        cons_shell_ev[i].ev_arg = &cons_buf[i];
        console_line_event_put(&cons_shell_ev[i]);
    }
    cons_compat_rx_cb = rx_cb;
    return 0;
}
#endif

void
console_pkg_init(void)
{
    int rc = 0;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    os_eventq_init(&cons_avail_queue);

#if MYNEWT_VAL(CONSOLE_HISTORY_SIZE) > 0
    console_hist_init();
#endif

#if MYNEWT_VAL(CONSOLE_UART)
    rc = uart_console_init();
#endif
#if MYNEWT_VAL(CONSOLE_RTT)
    rc = rtt_console_init();
#endif
    SYSINIT_PANIC_ASSERT(rc == 0);
}
