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

#include "syscfg/syscfg.h"

#if MYNEWT_VAL(CONSOLE_UART)
#include <ctype.h>
#include <assert.h>

#include "os/os.h"
#include "uart/uart.h"
#include "bsp/bsp.h"

#include "console/console.h"
#include "console_priv.h"

#define CONSOLE_HEAD_INC(cr)    (((cr)->cr_head + 1) & ((cr)->cr_size - 1))
#define CONSOLE_TAIL_INC(cr)    (((cr)->cr_tail + 1) & ((cr)->cr_size - 1))

static struct uart_dev *uart_dev;
static struct console_ring cr_tx;
/* must be after console_ring */
static uint8_t cr_tx_buf[MYNEWT_VAL(CONSOLE_UART_TX_BUF_SIZE)];
typedef void (*console_write_char)(struct uart_dev*, uint8_t);
static console_write_char write_char_cb;

struct console_ring {
    uint8_t cr_head;
    uint8_t cr_tail;
    uint16_t cr_size;
    uint8_t *cr_buf;
};

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
console_queue_char(struct uart_dev *uart_dev, uint8_t ch)
{
    int sr;

    OS_ENTER_CRITICAL(sr);
    while (CONSOLE_HEAD_INC(&cr_tx) == cr_tx.cr_tail) {
        /* TX needs to drain */
        uart_start_tx(uart_dev);
        OS_EXIT_CRITICAL(sr);
        if (os_started()) {
            os_time_delay(1);
        }
        OS_ENTER_CRITICAL(sr);
    }
    console_add_char(&cr_tx, ch);
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
        if (cr_tx.cr_head == cr_tx.cr_tail) {
            /*
             * Queue is empty.
             */
            break;
        }
        byte = console_pull_char(&cr_tx);
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
    if (cr_tx.cr_head == cr_tx.cr_tail) {
        /*
         * No more data.
         */
        return -1;
    }
    return console_pull_char(&cr_tx);
}

/*
 * Interrupts disabled when console_tx_char/console_rx_char are called.
 */
static int
console_rx_char(void *arg, uint8_t byte)
{
    return console_handle_char(byte);
}

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

    cr_tx.cr_size = MYNEWT_VAL(CONSOLE_UART_TX_BUF_SIZE);
    cr_tx.cr_buf = cr_tx_buf;
    write_char_cb = console_queue_char;

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
