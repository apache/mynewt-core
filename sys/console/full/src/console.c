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
#include <bsp/bsp.h>

#include "os/mynewt.h"
#include "os/os_task.h"
#include "console/console.h"
#include "console/ticks.h"
#include "console_priv.h"
#include "console/history.h"

/* Control characters */
#define ESC                0x1b
#define DEL                0x7f
#define BS                 0x08
#define ETX                0x03
#define EOT                0x04
#define FF                 0x0B
#define VT                 0x0C
#define CSI                "\x1b["

/* ANSI escape sequences */
#define ANSI_ESC           '['
#define ANSI_UP            'A'
#define ANSI_DOWN          'B'
#define ANSI_FORWARD       'C'
#define ANSI_BACKWARD      'D'
#define ANSI_END           'F'
#define ANSI_HOME          'H'
#define ANSI_DEL           '~'
#define DSR_CPS            'R'

#define ESC_ESC         (1 << 0)
#define ESC_ANSI        (1 << 1)
#define ESC_ANSI_FIRST  (1 << 2)
#define ESC_ANSI_VAL    (1 << 3)
#define ESC_ANSI_VAL_2  (1 << 4)

#define CONSOLE_NLIP_PKT_START1 (6)
#define CONSOLE_NLIP_PKT_START2 (9)
#define CONSOLE_NLIP_DATA_START1 (4)
#define CONSOLE_NLIP_DATA_START2 (20)

#define NLIP_PKT_START1  (CONSOLE_NLIP_PKT_START1)
#define NLIP_PKT_START2  (CONSOLE_NLIP_PKT_START2)
#define NLIP_DATA_START1 (CONSOLE_NLIP_DATA_START1)
#define NLIP_DATA_START2 (CONSOLE_NLIP_DATA_START2)

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

/* Cursor position in input line */
static uint16_t cur;
/* Number of characters after cursor in input line */
static uint16_t trailing_chars;
static struct os_eventq avail_queue;
static struct os_eventq *lines_queue;
static struct os_event *current_line_ev;
static completion_cb completion;
bool g_console_silence;
bool g_console_silence_non_nlip;
bool g_console_ignore_non_nlip;
static struct os_mutex console_write_lock;

/* Last character to write to console was LF but it was not printed yet */
static bool holding_lf;
static bool prompt_has_focus;
static bool terminal_initialized;
static bool terminal_size_requested;
/* Maximum row as reported by terminal, 0 - terminal size is not known yet */
static int max_row;
/* Buffer for prompt */
static char console_prompt[MYNEWT_VAL(CONSOLE_PROMPT_MAX_LEN)];
/* Length of prompt stored in console_prompt */
static uint16_t prompt_len;
/* Current history line, 0 no history line */
static history_handle_t history_line;

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

/*
 * Helper function to write sequence of characters to terminal without
 * checking for new lines or anything else.
 */
static void
console_write_nolock(const char *str, int cnt)
{
    int i;

    for (i = 0; i < cnt; i++) {
        if (console_out_nolock((int)str[i]) == EOF) {
            break;
        }
    }
}

/*
 * Helper function to write null terminated strings.
 */
static void
console_write_str(const char *str)
{
    console_write_nolock(str, strlen(str));
}

static int
console_filter_out(int c)
{
    if (g_console_silence) {
        return c;
    }

    /* For nlip output or when prompt has console write character */
    if (prompt_has_focus || g_is_output_nlip) {
        return console_out_nolock(c);
    }

    /*
     * For logs output don't write last newt line character at once.
     * Writing new would scroll window and blank line would be
     * visible as last line on terminal.
     * When terminal size is known, last line is used for prompt
     * and command line editing.
     * Following code prevents empty line to separate last log
     * line from prompt editor.
     */
    console_is_midline = c != '\n' && c != '\r';
    if (MYNEWT_VAL(CONSOLE_PROMPT_STICKY) && max_row > 0) {
        if (c == '\n') {
            console_is_midline = false;
            if (holding_lf) {
                console_out_nolock(c);
            } else {
                holding_lf = true;
            }
        } else {
            if (holding_lf) {
                console_out_nolock('\n');
                holding_lf = false;
            }
            console_is_midline = c != '\r';
            c = console_out_nolock(c);
        }
    } else {
        c = console_out_nolock(c);
    }
    return c;
}

/*
 * Helper function to write sequence of characters to console with new
 * line checking.
 */
static void
console_filter_write(const char *str, int cnt)
{
    int i;

    for (i = 0; i < cnt; i++) {
        if (console_filter_out((int)str[i]) == EOF) {
            break;
        }
    }
}

/*
 * Helper function for terminal escape sequences with number parameter.
 * It adds ascii encoded number plus one character.
 */
static char *
add_ascii_num_with_char(char *str, unsigned int num, char c)
{
    char *p = str;
    char *s;
    char tmp;

    /* Put digits in reverse order first. */
    do {
        *p++ = (num % 10) + '0';
        num /= 10;
    } while (num);
    s = str;
    str = p;
    --p;
    /* Revers order of characters, to get correct number representation */
    while (s < p) {
       tmp = *s;
       *s++ = *p;
       *p-- = tmp;
    }

    *str++ = c;
    *str = '\0';

    return str;
}

/*
 * Sends terminal escape sequence to set cursor at given row and column.
 * CSI <row> ; <column> H
 */
static void
console_cursor_set(int row, int column)
{
    char seq[24] = CSI;
    char *p;

    p = add_ascii_num_with_char(seq + 2, row, ';');
    p = add_ascii_num_with_char(p, column, 'H');
    console_write_nolock(seq, p - seq);
}

static inline void
cursor_save(void)
{
    console_out_nolock(ESC);
    console_out_nolock('[');
    console_out_nolock('s');
}

static inline void
cursor_restore(void)
{
    console_out_nolock(ESC);
    console_out_nolock('[');
    console_out_nolock('u');
}

static inline void
cursor_clear_line(void)
{
    console_out_nolock(ESC);
    console_out_nolock('[');
    console_out_nolock('K');
}
/*
 * Function sends sequence to get terminal size.
 * It does it by setting cursor position to some big numbers and
 * then requesting cursor position which should be adjusted by
 * terminal to actual terminal windows size.
 */
static void
request_terminal_size(void)
{
    /* If terminal size was requested, forget any previous value. */
    max_row = 0;
    cursor_save();
    console_write_str(CSI "1;999r" CSI "999;999H" CSI "6n");
    cursor_restore();
    terminal_size_requested = true;
}

static void
console_init_terminal(void)
{
    if (MYNEWT_VAL(CONSOLE_PROMPT_STICKY) && !terminal_initialized) {
        console_write_str(CSI "!p"
                          CSI "1;999r"
                          CSI "999;1H\n\n"
                          CSI "A"
                          CSI "s");
        console_is_midline = 0;
        terminal_initialized = true;
        max_row = 0;
    }
}

/*
 * Switch output to prompt portion of the terminal (last line).
 */
static void
console_switch_to_prompt(void)
{
    struct console_input *input;
    char c;

    console_init_terminal();
    /* If terminal size is not known yet, try asking terminal first */
    if (MYNEWT_VAL(CONSOLE_PROMPT_STICKY) &&
        max_row == 0 && !terminal_size_requested) {
        request_terminal_size();
    }
    /*
     * If terminal size is known and cursor is currently on log portion
     * of the screen, save cursor position and set cursor at place
     * that is in sync with 'cur' variable.
     */
    if (MYNEWT_VAL(CONSOLE_PROMPT_STICKY) &&
        !prompt_has_focus && max_row) {
        cursor_save();
        prompt_has_focus = true;
        console_cursor_set(max_row, prompt_len + cur + 1);
        if (MYNEWT_VAL(CONSOLE_PROMPT_SOFT_CURSOR)) {
            if (trailing_chars && current_line_ev && current_line_ev->ev_arg) {
                input = (struct console_input *)current_line_ev->ev_arg;
                c = input->line[cur];
            } else {
                c = ' ';
            }
            console_write_str(CSI "0m");
            console_out_nolock(c);
            console_out_nolock('\b');
            if (MYNEWT_VAL(CONSOLE_PROMPT_HIDE_CURSOR_IN_LOG_AREA)) {
                console_write_str(CSI "?25h");
            }
        }
    }
}

/*
 * Switch to log portion of the terminal.
 * Cursor will be restored somewhere above bottom line.
 */
static void
console_switch_to_logs(void)
{
    struct console_input *input;
    char c;

    if (g_is_output_nlip) {
        return;
    }

    console_init_terminal();
    if (MYNEWT_VAL(CONSOLE_PROMPT_STICKY) && prompt_has_focus) {
        if (MYNEWT_VAL(CONSOLE_PROMPT_SOFT_CURSOR)) {
            console_write_str(CSI);
            console_write_str(MYNEWT_VAL(CONSOLE_PROMPT_SOFT_CURSOR_ATTR));
            if (trailing_chars && current_line_ev && current_line_ev->ev_arg) {
                input = (struct console_input *)current_line_ev->ev_arg;
                c = input->line[cur];
            } else {
                c = ' ';
            }
            console_out_nolock(c);
            if (MYNEWT_VAL(CONSOLE_PROMPT_HIDE_CURSOR_IN_LOG_AREA)) {
                console_write_str(CSI "?25l");
            }
            console_write_str(CSI "0m\b");
        }
        cursor_restore();
        prompt_has_focus = false;
    }
}

/*
 * Set terminal scrolling region.
 * CSI <top> ; <bottom> r
 */
static void
console_set_scrolling_region(int top, int bottom)
{
    char seq[24] = CSI;
    char *p;

    p = add_ascii_num_with_char(seq + 2, top, ';');
    p = add_ascii_num_with_char(p, bottom, 'r');
    console_write_nolock(seq, p - seq);
}

void
console_prompt_set(const char *prompt, const char *line)
{
    bool locked;

    prompt_len = strlen(prompt);

    /* If this assert fails increase value of CONSOLE_PROMPT_MAX_LEN */
    assert(MYNEWT_VAL(CONSOLE_PROMPT_MAX_LEN) > prompt_len);

    strcpy(console_prompt, prompt);

    /* Place cursor at the end, no trailing chars */
    if (line) {
        cur = strlen(line);
    } else {
        cur = 0;
    }
    trailing_chars = 0;

    locked = console_lock(1000) == OS_OK;

    console_switch_to_prompt();
    if (MYNEWT_VAL(CONSOLE_PROMPT_STICKY) && prompt_has_focus) {
        console_write_str(CSI "999;1H");
        console_write_nolock(prompt, prompt_len);
        console_write_nolock(line, cur);
        console_write_str(CSI "K");
    } else {
        console_write(prompt, prompt_len);
        console_write(line, cur);
    }

    if (locked) {
        (void)console_unlock();
    }
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

    console_switch_to_logs();

    rc = console_filter_out(c);

    (void)console_unlock();

    return rc;
}

void
console_write(const char *str, int cnt)
{
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

    console_switch_to_logs();
    console_filter_write(str, cnt);

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
    char seq[14] = CSI;
    char *p;

    if (count) {
        p = add_ascii_num_with_char(seq + 2, count, 'C');
        console_write_nolock(seq, p - seq);
    }
}

static inline void
cursor_backward(unsigned int count)
{
    char seq[14] = CSI;
    char *p;

    if (count) {
        p = add_ascii_num_with_char(seq + 2, count, 'D');
        console_write_nolock(seq, p - seq);
    }
}

static bool trailing_selection;

/*
 * The following two weak functions are here in case linker does not allow
 * to eliminate dead code (console_history_search()).
 */
history_handle_t __attribute__((weak))
console_history_find(history_handle_t start, history_find_type_t search_type,
                     void *arg)
{
    return (history_handle_t )0;
}

int __attribute__((weak))
console_history_get(history_handle_t handle, size_t offset, char *buf,
                    size_t buf_size)
{
    return 0;
}

static void
console_history_search(char *line, history_find_type_t direction)
{
    line[cur] = '\0';
    history_line = console_history_find(history_line, direction, line);
    /* No more lines backward, do nothing */
    cursor_clear_line();
    if (!history_line) {
        trailing_chars = 0;
    } else {
        trailing_chars = console_history_get(history_line, cur,line + cur,
                                             MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN) - cur);
        console_write_str(CSI "7m");
        console_write_nolock(line + cur, trailing_chars);
        console_write_str(CSI "0m");
        cursor_backward(trailing_chars);
        trailing_selection = true;
    }
}

static void
insert_char(char *pos, char c)
{
    char tmp;
    int end;

    if ((!MYNEWT_VAL(CONSOLE_HISTORY_AUTO_SEARCH) || !trailing_selection) &&
        cur + trailing_chars >= MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN) - 1) {
        return;
    }

    if (echo) {
        if (!MYNEWT_VAL_CHOICE(CONSOLE_HISTORY, none) &&
            MYNEWT_VAL(CONSOLE_HISTORY_AUTO_SEARCH) && trailing_selection) {
            cursor_clear_line();
            trailing_chars = 0;
            trailing_selection = false;
        }
        /* Echo back to console */
        console_out_nolock(c);
    }

    ++cur;

    if (trailing_chars == 0) {
        *pos = c;
        if (!MYNEWT_VAL_CHOICE(CONSOLE_HISTORY, none) &&
            MYNEWT_VAL(CONSOLE_HISTORY_AUTO_SEARCH) && cur > 1) {
            history_line = 0;
            console_history_search(pos - cur + 1, HFT_MATCH_PREV);
            return;
        }
    }

    tmp = *pos;
    *(pos++) = c;
    end = trailing_chars;

    while (end-- > 0) {
        if (echo) {
            console_out_nolock(tmp);
        }
        c = *pos;
        *(pos++) = tmp;
        tmp = c;
    }

    /* Move cursor back to right place */
    if (echo) {
        cursor_backward(trailing_chars);
    }
}

static void
clear_selection(const char *line)
{
    if (MYNEWT_VAL(CONSOLE_HISTORY_AUTO_SEARCH) && trailing_selection) {
        trailing_selection = false;
        console_write_str(CSI "0m");
        console_write_nolock(line + cur, trailing_chars);
        cursor_backward(trailing_chars);
    }
}

static void
delete_selection(char *pos)
{
    if (MYNEWT_VAL(CONSOLE_HISTORY_AUTO_SEARCH) && trailing_selection) {
        *pos = 0;
        trailing_selection = false;
        trailing_chars = 0;
        cursor_clear_line();
    }
}

/* Delete character at cursor position */
static void
del_char(char *pos)
{
    int left;

    delete_selection(pos);

    left = trailing_chars;

    while (left-- > 1) {
        *pos = *(pos + 1);
        console_out_nolock(*(pos++));
    }

    /* Move cursor back to right place */
    if (trailing_chars) {
        console_out_nolock(' ');
        cursor_backward(trailing_chars);
        trailing_chars--;
    }
}

static void
console_clear_line(void)
{
    if (cur) {
        cursor_backward(cur);
    }
    cur = 0;
    trailing_chars = 0;

    cursor_clear_line();
}

#if !MYNEWT_VAL_CHOICE(CONSOLE_HISTORY, none)

static void
console_history_move(char *line, history_find_type_t direction)
{
    history_handle_t new_line;

    new_line = console_history_find(history_line, direction, NULL);

    /* No more lines backward, do nothing */
    if (!new_line && direction == HFT_PREV) {
        return;
    }

    history_line = new_line;
    console_clear_line();
    if (new_line) {
        cur = console_history_get(new_line, 0, line,
                                  MYNEWT_VAL_CONSOLE_MAX_INPUT_LEN);
    } else {
        line[0] = '\0';
    }
    trailing_chars = 0;
    console_write_nolock(line, cur);
}

static void
console_handle_move(char *line, int direction)
{
    if (MYNEWT_VAL(CONSOLE_HISTORY_AUTO_SEARCH) && trailing_selection) {
        console_history_search(line, direction > 0 ? HFT_MATCH_PREV :
                                     HFT_MATCH_NEXT);
    } else {
        console_history_move(line, direction > 0 ? HFT_PREV : HFT_NEXT);
    }
}
#endif

static void
handle_home(char *line)
{
    clear_selection(line);

    if (cur) {
        cursor_backward(cur);
        trailing_chars += cur;
        cur = 0;
    }
}

static void
handle_delete(char *line)
{
    if (trailing_chars) {
        del_char(&line[cur]);
    }
}

static void
handle_end(char *line)
{
    clear_selection(line);

    if (trailing_chars) {
        cursor_forward(trailing_chars);
        cur += trailing_chars;
        trailing_chars = 0;
    }
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
#if !MYNEWT_VAL_CHOICE(CONSOLE_HISTORY, none)
    case ANSI_UP:
    case ANSI_DOWN:
        console_handle_move(line, byte == ANSI_UP ? 1 : -1);
        break;
#endif
    case ANSI_BACKWARD:
        if (ansi_val > cur) {
            break;
        }

        clear_selection(line);
        trailing_chars += ansi_val;
        cur -= ansi_val;
        cursor_backward(ansi_val);
        break;
    case ANSI_FORWARD:
        if (ansi_val > trailing_chars) {
            break;
        }

        clear_selection(line);
        trailing_chars -= ansi_val;
        cur += ansi_val;
        cursor_forward(ansi_val);
        break;
    case ANSI_HOME:
        handle_home(line);
        break;
    case ANSI_END:
        handle_end(line);
        break;
    case '~':
        switch (ansi_val) {
        case 1:
            handle_home(line);
            break;
        case 3:
            handle_delete(line);
            break;
        case 4:
            handle_end(line);
            break;
        }
        break;
    case DSR_CPS:
        if (MYNEWT_VAL(CONSOLE_PROMPT_STICKY) && terminal_size_requested) {
            terminal_size_requested = false;
            max_row = ansi_val;
            console_cursor_set(max_row - 1, 1);
            cursor_save();
            console_set_scrolling_region(1, max_row - 1);
            if (prompt_len) {
                console_cursor_set(max_row, 1);
                console_write_nolock(console_prompt, prompt_len);
                console_write_nolock(line, cur + trailing_chars);
                console_write_str(CSI "K");
                cursor_backward(trailing_chars);
            }
            cursor_restore();
        }
        break;
    default:
        break;
    }

    esc_state &= ~ESC_ANSI;
}

static void
console_handle_line(void)
{
    cur = 0;
    trailing_chars = 0;
    os_eventq_put(lines_queue, current_line_ev);

#if MYNEWT_VAL(CONSOLE_COMPAT)
    if (console_compat_rx_cb) {
        console_compat_rx_cb();
    }
#endif

    current_line_ev = NULL;
}

static int
handle_nlip(uint8_t byte)
{
    int handled;
    struct console_input *input;

    input = current_line_ev->ev_arg;
    handled = 1;

    switch (nlip_state) {
    case NLIP_PKT_START2:
    case NLIP_DATA_START2:
        insert_char(&input->line[cur], byte);
        if (byte == '\n') {
            input->line[cur] = '\0';
            console_echo(1);
            nlip_state = 0;

            console_handle_line();
        }
        break;
    case NLIP_PKT_START1:
        if (byte == CONSOLE_NLIP_PKT_START2) {
            nlip_state = NLIP_PKT_START2;
            /* Disable echo to not flood the UART */
            console_echo(0);
            insert_char(&input->line[cur], CONSOLE_NLIP_PKT_START1);
            insert_char(&input->line[cur], CONSOLE_NLIP_PKT_START2);
        } else {
            nlip_state = 0;
            handled = g_console_ignore_non_nlip;
        }
        break;
    case NLIP_DATA_START1:
        if (byte == CONSOLE_NLIP_DATA_START2) {
            nlip_state = NLIP_DATA_START2;
            /* Disable echo to not flood the UART */
            console_echo(0);
            insert_char(&input->line[cur], CONSOLE_NLIP_DATA_START1);
            insert_char(&input->line[cur], CONSOLE_NLIP_DATA_START2);
        } else {
            nlip_state = 0;
            handled = g_console_ignore_non_nlip;
        }
        break;
    default:
        if (byte == CONSOLE_NLIP_DATA_START1) {
            nlip_state = NLIP_DATA_START1;
        } else if (byte == CONSOLE_NLIP_PKT_START1) {
            nlip_state = NLIP_PKT_START1;
        } else {
            /* For old code compatibility end of lines characters pass through */
            handled = g_console_ignore_non_nlip && byte != '\r' && byte != '\n';
        }
        break;
    }

    return handled;
}

static int
console_append_char(char *line, uint8_t byte)
{
    if (cur + trailing_chars >= MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN) - 1) {
        return 0;
    }

    line[cur + trailing_chars] = byte;

    if (byte == '\0') {
        return 1;
    }

    if (echo) {
        /* Echo back to console */
        console_switch_to_prompt();
        console_out_nolock(byte);
        console_switch_to_logs();
    }
    ++cur;
    return 1;
}

static void
handle_backspace(char *line)
{
    if (cur > 0) {
        cursor_backward(1);
        cur--;
        trailing_chars++;
        del_char(&line[cur]);
    }
}

int
console_handle_char(uint8_t byte)
{
#if !MYNEWT_VAL(CONSOLE_INPUT)
    return 0;
#endif
    struct console_input *input;
    static char prev_endl = '\0';

    if (!lines_queue) {
        return 0;
    }

    if (!current_line_ev) {
        current_line_ev = os_eventq_get_no_wait(&avail_queue);
        if (!current_line_ev) {
            rx_stalled = true;
            return -1;
        }
    }
    input = current_line_ev->ev_arg;

    if (handle_nlip(byte)) {
        return 0;
    }

    if (console_lock(1000)) {
        return -1;
    }

    console_switch_to_prompt();

    /* Handle ANSI escape mode */
    if (esc_state & ESC_ANSI) {
        handle_ansi(byte, input->line);
        goto unlock;
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

        goto unlock;
    }

    /* Handle special control characters */
    if (!isprint(byte)) {
        handle_ansi(byte, input->line);
        switch (byte) {
        case DEL:
        case BS:
            handle_backspace(input->line);
            break;
        case ESC:
            esc_state |= ESC_ESC;
            break;
        default:
            insert_char(&input->line[cur], byte);
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
            input->line[cur + trailing_chars] = '\0';
            if (MYNEWT_VAL(CONSOLE_PROMPT_STICKY) && prompt_has_focus) {
                console_switch_to_logs();
                /*
                 * Cursor is always in the middle of the line since new lines
                 * are held, so new line is needed here.
                 */
                console_out_nolock('\n');
                /* Duplicate command line to log space */
                console_write_str(console_prompt);
                console_write_nolock(input->line, cur + trailing_chars);
                /*
                 * Add additional new line if log line was not finished,
                 * this will avoid printing outstanding line at the end
                 * of duplicated command line.
                 */
                if (console_is_midline) {
                    console_out_nolock('\n');
                }
                console_switch_to_prompt();
                console_clear_line();
            } else {
                console_filter_out('\r');
                console_filter_out('\n');
            }
            if (!MYNEWT_VAL_CHOICE(CONSOLE_HISTORY, none)) {
                console_history_add(input->line);
                history_line = 0;
                if (MYNEWT_VAL(CONSOLE_HISTORY_AUTO_SEARCH)) {
                    trailing_selection = 0;
                }
            }
            console_handle_line();
            break;
        case '\t':
            if (completion && (!trailing_chars ||
                (MYNEWT_VAL(CONSOLE_HISTORY_AUTO_SEARCH) && trailing_selection))) {
                if (MYNEWT_VAL(CONSOLE_HISTORY_AUTO_SEARCH) && trailing_selection) {
                    delete_selection(input->line + cur);
                }
#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) == 0
                console_blocking_mode();
#endif
                console_switch_to_logs();
                completion(input->line, console_append_char);
                console_switch_to_prompt();
#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) == 0
                console_non_blocking_mode();
#endif
            }
            break;
        /* CTRL-C */
        case ETX:
            console_clear_line();
            break;
        /* CTRL-L */
        case VT:
            if (MYNEWT_VAL(CONSOLE_PROMPT_STICKY)) {
                request_terminal_size();
            } else {
                console_out_nolock(VT);
            }
            break;
        }
    } else {
        insert_char(&input->line[cur], byte);
    }
unlock:
    (void)console_unlock();

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

#if MYNEWT_VAL(CONSOLE_UART)
    rc = uart_console_init();
#endif
#if MYNEWT_VAL(CONSOLE_RTT)
    rc = rtt_console_init();
#endif
    SYSINIT_PANIC_ASSERT(rc == 0);
}
