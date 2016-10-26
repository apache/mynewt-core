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
#include <assert.h>
#include "syscfg/syscfg.h"
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "uart/uart.h"
#include "bsp/bsp.h"

#include "console/console.h"
#include "console/prompt.h"

/** Indicates whether the previous line of output was completed. */
int console_is_midline;

#define CONSOLE_RX_CHUNK        16

#if MYNEWT_VAL(CONSOLE_HIST_ENABLE)
#define CONSOLE_HIST_SZ         32
#endif

#define CONSOLE_DEL             0x7f    /* del character */
#define CONSOLE_ESC             0x1b    /* esc character */
#define CONSOLE_LEFT            'D'     /* esc-[-D emitted when moving left */
#define CONSOLE_UP              'A'     /* esc-[-A moving up */
#define CONSOLE_RIGHT           'C'     /* esc-[-C moving right */
#define CONSOLE_DOWN            'B'     /* esc-[-B moving down */

#define CONSOLE_HEAD_INC(cr)    (((cr)->cr_head + 1) & ((cr)->cr_size - 1))
#define CONSOLE_TAIL_INC(cr)    (((cr)->cr_tail + 1) & ((cr)->cr_size - 1))

typedef void (*console_write_char)(char);
void console_print_prompt(void);

struct console_ring {
    uint8_t cr_head;
    uint8_t cr_tail;
    uint16_t cr_size;
    uint8_t *cr_buf;
};

struct console_tty {
    struct uart_dev *ct_dev;

    struct console_ring ct_tx;
    /* must be after console_ring */
    uint8_t ct_tx_buf[MYNEWT_VAL(CONSOLE_TX_BUF_SIZE)];

    struct console_ring ct_rx;
    /* must be after console_ring */
    uint8_t ct_rx_buf[MYNEWT_VAL(CONSOLE_RX_BUF_SIZE)];

    console_rx_cb ct_rx_cb; /* callback that input is ready */
    console_write_char ct_write_char;
    uint8_t ct_echo_off:1;
    uint8_t ct_esc_seq:2;
} console_tty;

#if MYNEWT_VAL(CONSOLE_HIST_ENABLE)
struct console_hist {
    uint8_t ch_head;
    uint8_t ch_tail;
    uint8_t ch_size;
    uint8_t ch_curr;
    uint8_t ch_buf[CONSOLE_HIST_SZ][MYNEWT_VAL(CONSOLE_RX_BUF_SIZE)];
} console_hist;
#endif

static void
console_add_char(struct console_ring *cr, char ch)
{
    cr->cr_buf[cr->cr_head] = ch;
    cr->cr_head = CONSOLE_HEAD_INC(cr);
}

static uint8_t
console_pull_char(struct console_ring *cr)
{
    uint8_t ch;

    ch = cr->cr_buf[cr->cr_tail];
    cr->cr_tail = CONSOLE_TAIL_INC(cr);
    return ch;
}

static int
console_pull_char_head(struct console_ring *cr)
{
    if (cr->cr_head != cr->cr_tail) {
        cr->cr_head = (cr->cr_head - 1) & (cr->cr_size - 1);
        return 0;
    } else {
        return -1;
    }
}

static void
console_queue_char(char ch)
{
    struct console_tty *ct = &console_tty;
    int sr;

    OS_ENTER_CRITICAL(sr);
    while (CONSOLE_HEAD_INC(&ct->ct_tx) == ct->ct_tx.cr_tail) {
        /* TX needs to drain */
        uart_start_tx(ct->ct_dev);
        OS_EXIT_CRITICAL(sr);
        if (os_started()) {
            os_time_delay(1);
        }
        OS_ENTER_CRITICAL(sr);
    }
    console_add_char(&ct->ct_tx, ch);
    OS_EXIT_CRITICAL(sr);
}

#if MYNEWT_VAL(CONSOLE_HIST_ENABLE)
static void
console_hist_init(void)
{
    struct console_hist *ch = &console_hist;

    ch->ch_head = 0;
    ch->ch_tail = 0;
    ch->ch_curr = 0;
    ch->ch_size = CONSOLE_HIST_SZ;
}

static void
console_hist_add(struct console_ring *rx)
{
    struct console_hist *ch = &console_hist;
    uint8_t *str = ch->ch_buf[ch->ch_head];
    uint8_t tail;
    uint8_t empty = 1;

    tail = rx->cr_tail;
    while (tail != rx->cr_head) {
        *str = rx->cr_buf[tail];
        if (*str != ' ' && *str != '\t' && *str != '\n') {
            empty = 0;
        }
        if (*str == '\n') {
            *str = '\0';
            /* don't save empty history */
            if (empty) {
                return;
            }
            break;
        }
        str++;
        tail = (tail + 1) % MYNEWT_VAL(CONSOLE_RX_BUF_SIZE);
    }

    ch->ch_head = (ch->ch_head + 1) & (ch->ch_size - 1);
    ch->ch_curr = ch->ch_head;

    /* buffer full, start overwriting old history */
    if (ch->ch_head == ch->ch_tail) {
        ch->ch_tail = (ch->ch_tail + 1) & (ch->ch_size - 1);
    }
}

static int
console_hist_move(struct console_ring *rx, uint8_t *tx_buf, uint8_t direction)
{
    struct console_hist *ch = &console_hist;
    uint8_t *str = NULL;
    int space = 0;
    int i;
    uint8_t limit = direction == CONSOLE_UP ? ch->ch_tail : ch->ch_head;

    /* no more history to return in this direction */
    if (ch->ch_curr == limit) {
        return 0;
    }

    if (direction == CONSOLE_UP) {
        ch->ch_curr = (ch->ch_curr - 1) & (ch->ch_size - 1);
    } else {
        ch->ch_curr = (ch->ch_curr + 1) & (ch->ch_size - 1);
    }

    /* consume all chars */
    while (console_pull_char_head(rx) == 0) {
        /* do nothing */
    }

    str = ch->ch_buf[ch->ch_curr];
    for (i = 0; i < MYNEWT_VAL(CONSOLE_RX_BUF_SIZE); ++i) {
        if (str[i] == '\0') {
            break;
        }
        tx_buf[i] = str[i];
        console_add_char(rx, str[i]);
        space++;
    }

    return space;
}
#endif

static void
console_blocking_tx(char ch)
{
    struct console_tty *ct = &console_tty;

    uart_blocking_tx(ct->ct_dev, ch);
}

/*
 * Flush cnt characters from console output queue.
 */
static void
console_tx_flush(struct console_tty *ct, int cnt)
{
    int i;
    uint8_t byte;

    for (i = 0; i < cnt; i++) {
        if (ct->ct_tx.cr_head == ct->ct_tx.cr_tail) {
            /*
             * Queue is empty.
             */
            break;
        }
        byte = console_pull_char(&ct->ct_tx);
        console_blocking_tx(byte);
    }
}

void
console_blocking_mode(void)
{
    struct console_tty *ct = &console_tty;
    int sr;

    OS_ENTER_CRITICAL(sr);
    if (ct->ct_write_char) {
        ct->ct_write_char = console_blocking_tx;

        console_tx_flush(ct, MYNEWT_VAL(CONSOLE_TX_BUF_SIZE));
    }
    OS_EXIT_CRITICAL(sr);
}

void
console_echo(int on)
{
    struct console_tty *ct = &console_tty;

    ct->ct_echo_off = !on;
}

size_t
console_file_write(void *arg, const char *str, size_t cnt)
{
    struct console_tty *ct = &console_tty;
    int i;

    if (!ct->ct_write_char) {
        return cnt;
    }
    for (i = 0; i < cnt; i++) {
        if (str[i] == '\n') {
            ct->ct_write_char('\r');
        }
        ct->ct_write_char(str[i]);
    }
    if (cnt > 0) {
        console_is_midline = str[cnt - 1] != '\n';
    }
    uart_start_tx(ct->ct_dev);
    return cnt;
}

void
console_write(const char *str, int cnt)
{
    console_file_write(NULL, str, cnt);
}

int
console_read(char *str, int cnt, int *newline)
{
    struct console_tty *ct = &console_tty;
    struct console_ring *cr = &ct->ct_rx;
    int sr;
    int i;
    uint8_t ch;

    *newline = 0;
    OS_ENTER_CRITICAL(sr);
    for (i = 0; i < cnt; i++) {
        if (cr->cr_head == cr->cr_tail) {
            break;
        }

        if ((i & (CONSOLE_RX_CHUNK - 1)) == (CONSOLE_RX_CHUNK - 1)) {
            /*
             * Make a break from blocking interrupts during the copy.
             */
            OS_EXIT_CRITICAL(sr);
            OS_ENTER_CRITICAL(sr);
        }

        ch = console_pull_char(cr);
        if (ch == '\n') {
            *str = '\0';
            *newline = 1;
            break;
        }
        *str++ = ch;
    }
    OS_EXIT_CRITICAL(sr);
    if (i > 0 || *newline) {
        uart_start_rx(ct->ct_dev);
    }
    return i;
}

/*
 * Interrupts disabled when console_tx_char/console_rx_char are called.
 */
static int
console_tx_char(void *arg)
{
    struct console_tty *ct = (struct console_tty *)arg;
    struct console_ring *cr = &ct->ct_tx;

    if (cr->cr_head == cr->cr_tail) {
        /*
         * No more data.
         */
        return -1;
    }
    return console_pull_char(cr);
}

static int
console_buf_space(struct console_ring *cr)
{
    int space;

    space = (cr->cr_tail - cr->cr_head) & (cr->cr_size - 1);
    return space - 1;
}

static int
console_rx_char(void *arg, uint8_t data)
{
    struct console_tty *ct = (struct console_tty *)arg;
    struct console_ring *tx = &ct->ct_tx;
    struct console_ring *rx = &ct->ct_rx;
    int tx_space = 0;
    int i;
#if MYNEWT_VAL(CONSOLE_HIST_ENABLE)
    uint8_t tx_buf[MYNEWT_VAL(CONSOLE_RX_BUF_SIZE)];
#else
    uint8_t tx_buf[3];
#endif

    if (CONSOLE_HEAD_INC(&ct->ct_rx) == ct->ct_rx.cr_tail) {
        /*
         * RX queue full. Reader must drain this.
         */
        if (ct->ct_rx_cb) {
            ct->ct_rx_cb();
        }
        return -1;
    }

    /* echo */
    switch (data) {
    case '\r':
    case '\n':
        /*
         * linefeed
         */
        tx_buf[0] = '\n';
        tx_buf[1] = '\r';
        tx_space = 2;
        console_add_char(rx, '\n');
#if MYNEWT_VAL(CONSOLE_HIST_ENABLE)
        console_hist_add(rx);
#endif
        if (ct->ct_rx_cb) {
            ct->ct_rx_cb();
        }
        break;
    case CONSOLE_ESC:
        ct->ct_esc_seq = 1;
        goto out;
    case '[':
        if (ct->ct_esc_seq == 1) {
            ct->ct_esc_seq = 2;
            goto out;
        } else {
            goto queue_char;
        }
        break;
    case CONSOLE_LEFT:
        if (ct->ct_esc_seq == 2) {
            goto backspace;
        } else {
            goto queue_char;
        }
        break;
    case CONSOLE_UP:
    case CONSOLE_DOWN:
        if (ct->ct_esc_seq != 2) {
            goto queue_char;
        }
#if MYNEWT_VAL(CONSOLE_HIST_ENABLE)
        tx_space = console_hist_move(rx, tx_buf, data);
        tx_buf[tx_space] = 0;
        ct->ct_esc_seq = 0;
        /*
         * when moving up, stop on oldest history entry
         * when moving down, let it delete input before leaving...
         */
        if (data == CONSOLE_UP && tx_space == 0) {
            goto out;
        }
        if (!ct->ct_echo_off) {
            /* HACK: clean line by backspacing up to maximum possible space */
            for (i = 0; i < MYNEWT_VAL(CONSOLE_TX_BUF_SIZE); i++) {
                if (console_buf_space(tx) < 3) {
                    console_tx_flush(ct, 3);
                }
                console_add_char(tx, '\b');
                console_add_char(tx, ' ');
                console_add_char(tx, '\b');
                uart_start_tx(ct->ct_dev);
            }
            if (tx_space == 0) {
                goto out;
            }
        } else {
            goto queue_char;
        }
        break;
#else
        ct->ct_esc_seq = 0;
        goto out;
#endif
    case CONSOLE_RIGHT:
        if (ct->ct_esc_seq == 2) {
            data = ' '; /* add space */
        }
        goto queue_char;
    case '\b':
    case CONSOLE_DEL:
backspace:
        /*
         * backspace
         */
        ct->ct_esc_seq = 0;
        if (console_pull_char_head(rx) == 0) {
            /*
             * Only wipe out char if we can pull stuff off from head.
             */
            tx_buf[0] = '\b';
            tx_buf[1] = ' ';
            tx_buf[2] = '\b';
            tx_space = 3;
        } else {
            goto out;
        }
        break;
    default:
queue_char:
        tx_buf[0] = data;
        tx_space = 1;
        ct->ct_esc_seq = 0;
        console_add_char(rx, data);
        break;
    }
    if (!ct->ct_echo_off) {
        if (console_buf_space(tx) < tx_space) {
            console_tx_flush(ct, tx_space);
        }
        for (i = 0; i < tx_space; i++) {
            console_add_char(tx, tx_buf[i]);
        }
        uart_start_tx(ct->ct_dev);
    }
out:
    return 0;
}

int
console_is_init(void)
{
    struct console_tty *ct = &console_tty;

    return (ct->ct_dev != NULL);
}

static int is_power_of_two (unsigned int x)
{
  return ((x != 0) && !(x & (x - 1)));
}

int
console_init(console_rx_cb rx_cb)
{
    struct console_tty *ct = &console_tty;
    struct uart_conf uc = {
        .uc_speed = MYNEWT_VAL(CONSOLE_BAUD),
        .uc_databits = 8,
        .uc_stopbits = 1,
        .uc_parity = UART_PARITY_NONE,
        .uc_flow_ctl = MYNEWT_VAL(CONSOLE_FLOW_CONTROL),
        .uc_tx_char = console_tx_char,
        .uc_rx_char = console_rx_char,
        .uc_cb_arg = ct
    };

    ct->ct_rx_cb = rx_cb;
    if (!ct->ct_dev) {
        ct->ct_tx.cr_size = MYNEWT_VAL(CONSOLE_TX_BUF_SIZE);
        ct->ct_tx.cr_buf = ct->ct_tx_buf;
        ct->ct_rx.cr_size = MYNEWT_VAL(CONSOLE_RX_BUF_SIZE);
        ct->ct_rx.cr_buf = ct->ct_rx_buf;
        ct->ct_write_char = console_queue_char;

        ct->ct_dev = (struct uart_dev *)os_dev_open(CONSOLE_UART,
          OS_TIMEOUT_NEVER, &uc);
        if (!ct->ct_dev) {
            return -1;
        }
        ct->ct_echo_off = ! MYNEWT_VAL(CONSOLE_ECHO);
    }

    /* must be a power of 2 */
    assert(is_power_of_two(MYNEWT_VAL(CONSOLE_RX_BUF_SIZE)));

#if MYNEWT_VAL(CONSOLE_HIST_ENABLE)
    console_hist_init();
#endif

    return 0;
}

void
console_pkg_init(void)
{
    int rc;

    rc = console_init(NULL);
    SYSINIT_PANIC_ASSERT(rc == 0);
}
