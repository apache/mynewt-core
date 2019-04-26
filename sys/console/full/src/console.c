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
#include "os/os_task.h"
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
uint8_t g_is_output_nlip;

#if MYNEWT_VAL(CONSOLE_COMPAT)
#define CONSOLE_COMPAT_MAX_CMD_QUEUED 1
static struct console_input buf[CONSOLE_COMPAT_MAX_CMD_QUEUED];
static struct os_event shell_console_ev[CONSOLE_COMPAT_MAX_CMD_QUEUED];
static console_rx_cb console_compat_rx_cb; /* callback that input is ready */
static struct os_eventq compat_avail_queue;
static struct os_eventq compat_lines_queue;
#endif

static int esc_state;
static int nlip_state;
static int echo = MYNEWT_VAL(CONSOLE_ECHO);
static unsigned int ansi_val, ansi_val_2;
static bool rx_stalled;

static uint16_t cur, end;
static struct os_eventq avail_queue;
static struct os_eventq *lines_queue;
static completion_cb completion;
bool g_console_silence;
bool g_console_silence_non_nlip;
bool g_console_ignore_non_nlip;
static struct os_mutex console_write_lock;

/*
 * Default implementation in case all consoles are disabled - we just ignore any
 * output to console.
 */
int __attribute__((weak))
console_out_nolock(int c)
{
    return c;
}

void
console_echo(int on)
{
    echo = on;
}

int
console_lock(int timeout)
{
    int rc = OS_OK;

    /* Locking from isr while some task own mutex fails with OS_EBUSY */
    if (os_arch_in_isr()) {
        if (os_mutex_get_level(&console_write_lock)) {
            rc = OS_EBUSY;
        }
        goto end;
    }

    rc = os_mutex_pend(&console_write_lock, timeout);
    if (rc == OS_NOT_STARTED) {
        /* No need to block before system start make it OK */
        rc = OS_OK;
    }

end:
    return rc;
}

int
console_unlock(void)
{
    int rc = OS_OK;

    if (os_arch_in_isr()) {
        goto end;
    }

    rc = os_mutex_release(&console_write_lock);
    assert(rc == OS_OK || rc == OS_NOT_STARTED);
end:
    return rc;
}

int
console_out(int c)
{
    int rc;
    const os_time_t timeout =
        os_time_ms_to_ticks32(MYNEWT_VAL(CONSOLE_DEFAULT_LOCK_TIMEOUT));

    if (console_lock(timeout) != OS_OK) {
        return c;
    }
    rc = console_out_nolock(c);

    (void)console_unlock();

    return rc;
}

void
console_write(const char *str, int cnt)
{
    int i;
    const os_time_t timeout =
            os_time_ms_to_ticks32(MYNEWT_VAL(CONSOLE_DEFAULT_LOCK_TIMEOUT));

    if (console_lock(timeout) != OS_OK) {
        return;
    }

    if (cnt >= 2 && str[0] == CONSOLE_NLIP_DATA_START1 &&
        str[1] == CONSOLE_NLIP_DATA_START2) {
        g_is_output_nlip = 1;
    }

    /* From the shell the first byte is always \n followed by the
     * actual pkt start bytes, hence checking byte 1 and 2
     */
    if (cnt >= 3 && str[1] == CONSOLE_NLIP_PKT_START1 &&
        str[2] == CONSOLE_NLIP_PKT_START2) {
        g_is_output_nlip = 1;
    }

    /* If the byte string is non nlip and we are silencing non nlip bytes,
     * do not let it go out on the console
     */ 
    if (!g_is_output_nlip && g_console_silence_non_nlip) {
        goto done;
    }

    for (i = 0; i < cnt; i++) {
        if (console_out_nolock((int)str[i]) == EOF) {
            break;
        }
    }

done:
    if (cnt > 0 && str[cnt - 1] == '\n') {
        g_is_output_nlip = 0;
    }
    (void)console_unlock();
}

#if MYNEWT_VAL(CONSOLE_COMPAT)
int
console_read(char *str, int cnt, int *newline)
{
    struct os_event *ev;
    struct console_input *cmd;
    size_t len;

    *newline = 0;
    ev = os_eventq_get_no_wait(lines_queue);
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

    if (cur + end >= MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN) - 1) {
        return;
    }

    if (echo) {
        /* Echo back to console */
        console_out(c);
    }

    ++cur;

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
    if (cur) {
        cursor_backward(cur);
    }
    cur = 0;
    end = 0;

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
        insert_char(&line[cur], *str, end);
        ++str;
    }
}
#endif

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
#if MYNEWT_VAL(CONSOLE_HISTORY_SIZE) > 0
    case ANSI_UP:
    case ANSI_DOWN:
        console_blocking_mode();
        console_hist_move(line, byte);
        console_non_blocking_mode();
        break;
#endif
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

static int
handle_nlip(uint8_t byte)
{
    if (((nlip_state & NLIP_PKT_START1) &&
         (nlip_state & NLIP_PKT_START2)) ||
        ((nlip_state & NLIP_DATA_START1) &&
         (nlip_state & NLIP_DATA_START2)))
    {
        return 1;
    }

    if ((nlip_state & NLIP_PKT_START1) &&
        (byte == CONSOLE_NLIP_PKT_START2)) {
        nlip_state |= NLIP_PKT_START2;
        return 1;
    } else if ((nlip_state & NLIP_DATA_START1) &&
               (byte == CONSOLE_NLIP_DATA_START2)) {
        nlip_state |= NLIP_DATA_START2;
        return 1;
    } else {
        nlip_state = 0;
        return 0;
    }
}

static int
console_append_char(char *line, uint8_t byte)
{
    if (cur + end >= MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN) - 1) {
        return 0;
    }

    line[cur + end] = byte;

    if (byte == '\0') {
        return 1;
    }

    if (echo) {
        /* Echo back to console */
        console_out(byte);
    }
    ++cur;
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

    if (!lines_queue) {
        return 0;
    }

    if (!ev) {
        ev = os_eventq_get_no_wait(&avail_queue);
        if (!ev) {
            rx_stalled = true;
            return -1;
        }
        input = ev->ev_arg;
    }

    if (handle_nlip(byte))  {
        if (byte == '\n') {
            insert_char(&input->line[cur], byte, end);
            input->line[cur] = '\0';
            cur = 0;
            end = 0;
            os_eventq_put(lines_queue, ev);
            nlip_state = 0;

#if MYNEWT_VAL(CONSOLE_COMPAT)
            if (console_compat_rx_cb) {
                console_compat_rx_cb();
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
            insert_char(&input->line[cur], CONSOLE_NLIP_PKT_START1, end);
        } else if (byte == CONSOLE_NLIP_DATA_START2) {
            /* Disable echo to not flood the UART */
            console_echo(0);
            insert_char(&input->line[cur], CONSOLE_NLIP_DATA_START1, end);
        }

        insert_char(&input->line[cur], byte, end);

        return 0;
    }

    /* Handle ANSI escape mode */
    if (esc_state & ESC_ANSI) {
        if (g_console_ignore_non_nlip) {
            return 0;
        }
        handle_ansi(byte, input->line);
        return 0;
    }

    /* Handle escape mode */
    if (esc_state & ESC_ESC) {
        if (g_console_ignore_non_nlip) {
            return 0;
        }
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
        case CONSOLE_NLIP_PKT_START1:
            nlip_state |= NLIP_PKT_START1;
            break;
        case CONSOLE_NLIP_DATA_START1:
            nlip_state |= NLIP_DATA_START1;
            break;
        case DEL:
        case BS:
            if (g_console_ignore_non_nlip) {
                break;
            }
            if (cur > 0) {
                del_char(&input->line[--cur], end);
            }
            break;
        case ESC:
            if (g_console_ignore_non_nlip) {
                break;
            }
            esc_state |= ESC_ESC;
            break;
        default:
            insert_char(&input->line[cur], byte, end);
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
            input->line[cur + end] = '\0';
            console_out('\r');
            console_out('\n');
            cur = 0;
            end = 0;
            os_eventq_put(lines_queue, ev);
#if MYNEWT_VAL(CONSOLE_HISTORY_SIZE) > 0
            console_hist_add(input->line);
#endif

#if MYNEWT_VAL(CONSOLE_COMPAT)
            if (console_compat_rx_cb) {
                console_compat_rx_cb();
            }
#endif

            input = NULL;
            ev = NULL;
            break;
        case '\t':
            if (g_console_ignore_non_nlip) {
                break;
            }
            if (completion && !end) {
#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) == 0
                console_blocking_mode();
#endif
                completion(input->line, console_append_char);
#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) == 0
                console_non_blocking_mode();
#endif
            }
            break;
        }

        return 0;
    }

    if (!g_console_ignore_non_nlip) {
        insert_char(&input->line[cur], byte, end);
    }
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
    lines_queue = evq;
}

void
console_line_event_put(struct os_event *ev)
{
    os_eventq_put(&avail_queue, ev);

    if (rx_stalled) {
        rx_stalled = false;
        console_rx_restart();
    }
}

void
console_set_completion_cb(completion_cb cb)
{
    completion = cb;
}

void
console_deinit(void)
{
#if MYNEWT_VAL(CONSOLE_UART)
    uart_console_deinit();
#endif
}

void
console_reinit(void)
{
#if MYNEWT_VAL(CONSOLE_UART)
     uart_console_init();
#endif
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
        shell_console_ev[i].ev_arg = &buf[i];
        console_line_event_put(&shell_console_ev[i]);
    }
    console_compat_rx_cb = rx_cb;
    return 0;
}
#endif

void
console_pkg_init(void)
{
    int rc = 0;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    os_eventq_init(&avail_queue);
    os_mutex_init(&console_write_lock);

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
