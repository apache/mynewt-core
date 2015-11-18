/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <inttypes.h>
#include "os/os.h"
#include "hal/hal_uart.h"
#include "bsp/bsp.h"
#include "console/console.h"

#define CONSOLE_TX_BUF_SZ	32	/* IO buffering, must be power of 2 */
#define CONSOLE_RX_BUF_SZ	128

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

static void
console_pull_char_head(struct console_ring *cr)
{
    if (cr->cr_head != cr->cr_tail) {
        cr->cr_head = (cr->cr_head - 1) & (cr->cr_size - 1);
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
        os_time_delay(1);
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

void
console_blocking_mode(void)
{
    struct console_tty *ct = &console_tty;
    int sr;
    uint8_t byte;

    OS_ENTER_CRITICAL(sr);
    ct->ct_write_char = console_blocking_tx;

    /*
     * Flush console output queue.
     */
    while (ct->ct_tx.cr_head != ct->ct_tx.cr_tail) {
        byte = console_pull_char(&ct->ct_tx);
        console_blocking_tx(byte);
    }
    OS_EXIT_CRITICAL(sr);
}

void
console_write(const char *str, int cnt)
{
    struct console_tty *ct = &console_tty;
    int i;

    i = 0;
    for (i = 0; i < cnt; i++) {
        if (str[i] == '\n') {
            ct->ct_write_char('\r');
        }
        ct->ct_write_char(str[i]);
    }
    hal_uart_start_tx(CONSOLE_UART);
}

int
console_read(char *str, int cnt)
{
    struct console_tty *ct = &console_tty;
    struct console_ring *cr = &ct->ct_rx;
    int sr;
    int i;
    uint8_t ch;

    OS_ENTER_CRITICAL(sr);
    for (i = 0; i < cnt; i++) {
        if (cr->cr_head == cr->cr_tail) {
            i = -1;
            break;
        }
        ch = console_pull_char(cr);
        if (ch == '\n') {
            *str = '\0';
            break;
        }
        *str++ = ch;
    }
    OS_EXIT_CRITICAL(sr);
    if (i >= 0) {
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
console_rx_char(void *arg, uint8_t data)
{
    struct console_tty *ct = (struct console_tty *)arg;
    struct console_ring *tx = &ct->ct_tx;
    struct console_ring *rx = &ct->ct_rx;

    if (CONSOLE_HEAD_INC(&ct->ct_tx) == ct->ct_tx.cr_tail) {
        /*
         * RX queue full. Reader must drain this.
         */
        if (ct->ct_rx_cb) {
            ct->ct_rx_cb(0);
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
        console_add_char(tx, '\n');
        console_add_char(tx, '\r');
        console_add_char(rx, '\n');
        if (ct->ct_rx_cb) {
            ct->ct_rx_cb(1);
        }
        break;
    case '\b':
        /*
         * backspace
         */
        console_add_char(tx, '\b');
        console_add_char(tx, ' ');
        console_add_char(tx, '\b');
        console_pull_char_head(rx);
        break;
    default:
        console_add_char(tx, data);
        console_add_char(rx, data);
        break;
    }
    hal_uart_start_tx(CONSOLE_UART);
    return 0;
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
    return 0;
}
