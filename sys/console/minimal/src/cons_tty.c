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

/*
 * Minimal line-based console implementation. Does not include
 * echo, command line history, prompt or console_printf().
 * Only offers console_write() and console_read().
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

#define CONSOLE_RX_CHUNK        16

#define CONSOLE_HEAD_INC(cr)    (((cr)->cr_head + 1) & ((cr)->cr_size - 1))
#define CONSOLE_TAIL_INC(cr)    (((cr)->cr_tail + 1) & ((cr)->cr_size - 1))

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
console_queue_char(char ch)
{
    struct console_tty *ct = &console_tty;
    int sr;

    OS_ENTER_CRITICAL(sr);
    while (CONSOLE_HEAD_INC(&ct->ct_tx) == ct->ct_tx.cr_tail) {
        /* TX needs to drain */
        uart_start_tx(ct->ct_dev);
        OS_EXIT_CRITICAL(sr);
        OS_ENTER_CRITICAL(sr);
    }
    console_add_char(&ct->ct_tx, ch);
    OS_EXIT_CRITICAL(sr);
}

void
console_write(const char *str, int cnt)
{
    struct console_tty *ct = &console_tty;
    int i;

    for (i = 0; i < cnt; i++) {
        if (str[i] == '\n') {
            console_queue_char('\r');
        }
        console_queue_char(str[i]);
    }
    uart_start_tx(ct->ct_dev);
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
console_rx_char(void *arg, uint8_t data)
{
    struct console_tty *ct = (struct console_tty *)arg;
    struct console_ring *rx = &ct->ct_rx;

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
        console_add_char(rx, '\n');
        if (ct->ct_rx_cb) {
            ct->ct_rx_cb();
        }
        break;
    default:
        console_add_char(rx, data);
        break;
    }
    return 0;
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

        ct->ct_dev = (struct uart_dev *)os_dev_open(CONSOLE_UART,
          OS_TIMEOUT_NEVER, &uc);
        if (!ct->ct_dev) {
            return -1;
        }
    }

    /* must be a power of 2 */
    assert(is_power_of_two(MYNEWT_VAL(CONSOLE_RX_BUF_SIZE)));

    return 0;
}

void
console_pkg_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = console_init(NULL);
    SYSINIT_PANIC_ASSERT(rc == 0);
}
