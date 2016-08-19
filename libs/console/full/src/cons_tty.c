/**
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
#include "os/os.h"
#include "hal/hal_uart.h"
#include "bsp/bsp.h"
#include "console/console.h"

int g_console_is_init;

/** Indicates whether the previous line of output was completed. */
int console_is_midline;

#define CONSOLE_TX_BUF_SZ	32	/* IO buffering, must be power of 2 */
#define CONSOLE_RX_BUF_SZ	128
#define CONSOLE_RX_CHUNK	16

#define CONSOLE_DEL		0x7f	/* del character */
#define CONSOLE_ESC		0x1b	/* esc character */
#define CONSOLE_LEFT		'D'     /* esc-[-D emitted when moving left */
#define CONSOLE_UP		'A'     /* esc-[-A moving up */
#define CONSOLE_RIGHT		'C'     /* esc-[-C moving right */
#define CONSOLE_DOWN		'B'     /* esc-[-B moving down */

#define CONSOLE_HEAD_INC(cr)	(((cr)->cr_head + 1) & ((cr)->cr_size - 1))
#define CONSOLE_TAIL_INC(cr)	(((cr)->cr_tail + 1) & ((cr)->cr_size - 1))

typedef void (*console_write_char)(char);

struct console_ring {
    uint8_t cr_head;
    uint8_t cr_tail;
    uint8_t cr_size;
    uint8_t _pad;
    uint8_t *cr_buf;
};

struct console_tty {
    struct console_ring ct_tx;
    uint8_t ct_tx_buf[CONSOLE_TX_BUF_SZ]; /* must be after console_ring */
    struct console_ring ct_rx;
    uint8_t ct_rx_buf[CONSOLE_RX_BUF_SZ]; /* must be after console_ring */
    console_rx_cb ct_rx_cb;	/* callback that input is ready */
    console_write_char ct_write_char;
    uint8_t ct_echo_off:1;
    uint8_t ct_esc_seq:2;
} console_tty;

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
        hal_uart_start_tx(CONSOLE_UART);
        OS_EXIT_CRITICAL(sr);
	if (os_started()) {
            os_time_delay(1);
	}
        OS_ENTER_CRITICAL(sr);
    }
    console_add_char(&ct->ct_tx, ch);
    OS_EXIT_CRITICAL(sr);
}

static void
console_blocking_tx(char ch)
{
    hal_uart_blocking_tx(CONSOLE_UART, ch);
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
    ct->ct_write_char = console_blocking_tx;

    console_tx_flush(ct, CONSOLE_TX_BUF_SZ);
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
    hal_uart_start_tx(CONSOLE_UART);
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
        hal_uart_start_rx(CONSOLE_UART);
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
    int tx_space;
    int i;
    int tx_buf[3];

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
        if (ct->ct_esc_seq == 2) {
            /*
             * Do nothing.
             */
            ct->ct_esc_seq = 0;
            goto out;
        }
        goto queue_char;
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
        hal_uart_start_tx(CONSOLE_UART);
    }
out:
    return 0;
}

int
console_is_init(void)
{
    return (g_console_is_init);
}

int
console_init(console_rx_cb rx_cb)
{
    struct console_tty *ct = &console_tty;
    int rc;

    rc = hal_uart_init_cbs(CONSOLE_UART, console_tx_char, NULL,
            console_rx_char, ct);
    if (rc) {
        return rc;
    }
    ct->ct_tx.cr_size = CONSOLE_TX_BUF_SZ;
    ct->ct_tx.cr_buf = ct->ct_tx_buf;
    ct->ct_rx.cr_size = CONSOLE_RX_BUF_SZ;
    ct->ct_rx.cr_buf = ct->ct_rx_buf;
    ct->ct_rx_cb = rx_cb;
    ct->ct_write_char = console_queue_char;

    rc = hal_uart_config(CONSOLE_UART, 115200, 8, 1, HAL_UART_PARITY_NONE,
      HAL_UART_FLOW_CTL_NONE);
    if (rc) {
        return rc;
    }

    g_console_is_init = 1;

    return 0;
}
