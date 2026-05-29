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

#include "util/ring_buffer.h"
#include "console/console.h"
#include "console_priv.h"

static struct uart_dev *uart_dev;
static struct ring_buffer cr_tx;
static uint8_t cr_tx_buf[MYNEWT_VAL(CONSOLE_UART_TX_BUF_SIZE)];
typedef void (*console_write_char)(struct uart_dev*, uint8_t);
static console_write_char write_char_cb;

#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) > 0
static struct ring_buffer cr_rx;
static uint8_t cr_rx_buf[MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE)];
static volatile bool uart_console_rx_stalled;

struct os_event rx_ev;
#endif

static void
uart_console_queue_char(struct uart_dev *uart, uint8_t ch)
{
    int sr;

    if (((uart->ud_dev.od_flags & OS_DEV_F_STATUS_OPEN) == 0) ||
        ((uart->ud_dev.od_flags & OS_DEV_F_STATUS_SUSPENDED) != 0)) {
        return;
    }

    OS_ENTER_CRITICAL(sr);
    while (ring_buffer_is_full(&cr_tx)) {
        /* TX needs to drain */
        uart_start_tx(uart_dev);
        OS_EXIT_CRITICAL(sr);
        if (os_started()) {
            os_time_delay(1);
        }
        OS_ENTER_CRITICAL(sr);
    }
    ring_buffer_push(&cr_tx, ch);
    OS_EXIT_CRITICAL(sr);
}

/*
 * Flush cnt characters from console output queue.
 */
static void
uart_console_tx_flush(int cnt)
{
    int i;
    uint8_t byte;

    for (i = 0; i < cnt; i++) {
        if (ring_buffer_is_empty(&cr_tx)) {
            break;
        }
        byte = ring_buffer_pull(&cr_tx);
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

        uart_console_tx_flush(MYNEWT_VAL(CONSOLE_UART_TX_BUF_SIZE));
    }
    OS_EXIT_CRITICAL(sr);
}

void
uart_console_non_blocking_mode(void)
{
    int sr;

    OS_ENTER_CRITICAL(sr);
    if (write_char_cb) {
        write_char_cb = uart_console_queue_char;
    }
    OS_EXIT_CRITICAL(sr);
}

int
console_out_nolock(int c)
{
    /* Assure that there is a write cb installed; this enables to debug
     * code that is faulting before the console was initialized.
     */
    if (!write_char_cb) {
        return c;
    }

    if ('\n' == c) {
        write_char_cb(uart_dev, '\r');
    }
    write_char_cb(uart_dev, c);
    uart_start_tx(uart_dev);

    return c;
}

void
console_rx_restart(void)
{
#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) > 0
    os_eventq_put(os_eventq_dflt_get(), &rx_ev);
#else
    uart_start_rx(uart_dev);
#endif
}

/*
 * Interrupts disabled when console_tx_char/console_rx_char are called.
 * Characters sent only in blocking mode.
 */
static int
uart_console_tx_char(void *arg)
{
    if (ring_buffer_is_empty(&cr_tx)) {
        return -1;
    }
    return ring_buffer_pull(&cr_tx);
}

/*
 * Interrupts disabled when console_tx_char/console_rx_char are called.
 */
static int
uart_console_rx_char(void *arg, uint8_t byte)
{
#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) > 0
    if (ring_buffer_is_full(&cr_rx)) {
        uart_console_rx_stalled = true;
        return -1;
    }

    ring_buffer_push(&cr_rx, byte);

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
uart_console_rx_char_event(struct os_event *ev)
{
    static int b = -1;
    int sr;
    int ret;

    /* We may have unhandled character - try it first */
    if (b >= 0) {
        ret = console_handle_char(b);
        if (ret < 0) {
            return;
        }
    }

    while (!ring_buffer_is_empty(&cr_rx)) {
        OS_ENTER_CRITICAL(sr);
        b = ring_buffer_pull(&cr_rx);
        OS_EXIT_CRITICAL(sr);

        /* If UART RX was stalled due to a full receive buffer, restart RX now
         * that we have removed a byte from the buffer.
         */
        if (uart_console_rx_stalled) {
            uart_console_rx_stalled = false;
            uart_start_rx(uart_dev);
        }

        ret = console_handle_char(b);
        if (ret < 0) {
            return;
        }
    }

    b = -1;
}
#endif

int
uart_console_is_init(void)
{
    return uart_dev != NULL;
}

int
uart_console_deinit(void)
{
    struct os_dev *dev;
    dev = os_dev_lookup(MYNEWT_VAL(CONSOLE_UART_DEV));
    if (dev) {
        /* Force close now */
        os_dev_close(dev);
        uart_dev = NULL;
    } else {
        return SYS_ENODEV;
    }
    return 0;
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
        .uc_tx_char = uart_console_tx_char,
        .uc_rx_char = uart_console_rx_char,
    };

    cr_tx.size = MYNEWT_VAL(CONSOLE_UART_TX_BUF_SIZE);
    cr_tx.buf = cr_tx_buf;
    write_char_cb = uart_console_queue_char;

#if MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE) > 0
    cr_rx.size = MYNEWT_VAL(CONSOLE_UART_RX_BUF_SIZE);
    cr_rx.buf = cr_rx_buf;

    rx_ev.ev_cb = uart_console_rx_char_event;
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
