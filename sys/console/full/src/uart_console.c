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

#include "os/mynewt.h"

#if MYNEWT_VAL(CONSOLE_UART)
#include <ctype.h>
#include <assert.h>

#include "uart/uart.h"
#include "bsp/bsp.h"

#include "console/console.h"
#include "console_priv.h"

static struct uart_dev *uart_dev;
static struct console_ring cr_tx;
static uint8_t cr_tx_buf[MYNEWT_VAL(CONSOLE_UART_TX_BUF_SIZE)];
typedef void (*console_write_char)(struct uart_dev*, uint8_t);
static console_write_char write_char_cb;

#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) > 0
static struct console_ring cr_rx;
static uint8_t cr_rx_buf[MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE)];

struct os_event rx_ev;
#endif

struct console_ring {
    uint8_t head;
    uint8_t tail;
    uint16_t size;
    uint8_t *buf;
};

static inline int
inc_and_wrap(int i, int max)
{
    return (i + 1) & (max - 1);
}

static void
console_ring_add_char(struct console_ring *cr, char ch)
{
    cr->buf[cr->head] = ch;
    cr->head = inc_and_wrap(cr->head, cr->size);
}

static uint8_t
console_ring_pull_char(struct console_ring *cr)
{
    uint8_t ch;

    ch = cr->buf[cr->tail];
    cr->tail = inc_and_wrap(cr->tail, cr->size);
    return ch;
}

static bool
console_ring_is_full(const struct console_ring *cr)
{
    return inc_and_wrap(cr->head, cr->size) == cr->tail;
}

static bool
console_ring_is_empty(const struct console_ring *cr)
{
    return cr->head == cr->tail;
}

static void
console_queue_char(struct uart_dev *uart_dev, uint8_t ch)
{
    int sr;

    if ((uart_dev->ud_dev.od_flags & OS_DEV_F_STATUS_OPEN) == 0) {
        return;
    }

    OS_ENTER_CRITICAL(sr);
    while (console_ring_is_full(&cr_tx)) {
        /* TX needs to drain */
        uart_start_tx(uart_dev);
        OS_EXIT_CRITICAL(sr);
        if (os_started()) {
            os_time_delay(1);
        }
        OS_ENTER_CRITICAL(sr);
    }
    console_ring_add_char(&cr_tx, ch);
    OS_EXIT_CRITICAL(sr);
}

/*
 * Flush cnt characters from console output queue.
 */
static void
console_tx_flush(int cnt)
{
    int i;
    uint8_t byte;

    for (i = 0; i < cnt; i++) {
        if (console_ring_is_empty(&cr_tx)) {
            break;
        }
        byte = console_ring_pull_char(&cr_tx);
        uart_blocking_tx(uart_dev, byte);
    }
}

void
uart_console_blocking_mode(void)
{
    int sr;

    OS_ENTER_CRITICAL(sr);
    if (write_char_cb) {
        write_char_cb = uart_blocking_tx;

        console_tx_flush(MYNEWT_VAL(CONSOLE_UART_TX_BUF_SIZE));
    }
    OS_EXIT_CRITICAL(sr);
}

void
uart_console_non_blocking_mode(void)
{
    int sr;

    OS_ENTER_CRITICAL(sr);
    if (write_char_cb) {
        write_char_cb = console_queue_char;
    }
    OS_EXIT_CRITICAL(sr);
}

int
console_out(int c)
{
    /* Assure that there is a write cb installed; this enables to debug
     * code that is faulting before the console was initialized.
     */
    if (!write_char_cb) {
        return c;
    }

    if ('\n' == c) {
        write_char_cb(uart_dev, '\r');
        console_is_midline = 0;
    } else {
        console_is_midline = 1;
    }
    write_char_cb(uart_dev, c);
    uart_start_tx(uart_dev);

    return c;
}

/*
 * Interrupts disabled when console_tx_char/console_rx_char are called.
 * Characters sent only in blocking mode.
 */
static int
console_tx_char(void *arg)
{
    if (console_ring_is_empty(&cr_tx)) {
        return -1;
    }
    return console_ring_pull_char(&cr_tx);
}

/*
 * Interrupts disabled when console_tx_char/console_rx_char are called.
 */
static int
console_rx_char(void *arg, uint8_t byte)
{
#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) > 0
    if (console_ring_is_full(&cr_rx)) {
        console_ring_pull_char(&cr_rx);
    }

    console_ring_add_char(&cr_rx, byte);

    if (!rx_ev.ev_queued) {
        os_eventq_put(os_eventq_dflt_get(), &rx_ev);
    }

    return 0;
#else
    return console_handle_char(byte);
#endif
}

#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) > 0
static void
console_rx_char_event(struct os_event *ev)
{
    uint8_t b;
    int sr;

    while (!console_ring_is_empty(&cr_rx)) {
        OS_ENTER_CRITICAL(sr);
        b = console_ring_pull_char(&cr_rx);
        OS_EXIT_CRITICAL(sr);
        console_handle_char(b);
    }
}
#endif

int
uart_console_is_init(void)
{
    return uart_dev != NULL;
}

int
uart_console_init(void)
{
    struct uart_conf uc = {
        .uc_speed = MYNEWT_VAL(CONSOLE_UART_BAUD),
        .uc_databits = 8,
        .uc_stopbits = 1,
        .uc_parity = UART_PARITY_NONE,
        .uc_flow_ctl = MYNEWT_VAL(CONSOLE_UART_FLOW_CONTROL),
        .uc_tx_char = console_tx_char,
        .uc_rx_char = console_rx_char,
    };

    cr_tx.size = MYNEWT_VAL(CONSOLE_UART_TX_BUF_SIZE);
    cr_tx.buf = cr_tx_buf;
    write_char_cb = console_queue_char;

#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) > 0
    cr_rx.size = MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE);
    cr_rx.buf = cr_rx_buf;

    rx_ev.ev_cb = console_rx_char_event;
#endif

    if (!uart_dev) {
        uart_dev = (struct uart_dev *)os_dev_open(MYNEWT_VAL(CONSOLE_UART_DEV),
          OS_TIMEOUT_NEVER, &uc);
        if (!uart_dev) {
            return -1;
        }
    }
    return 0;
}
#endif /* MYNEWT_VAL(CONSOLE_UART) */
