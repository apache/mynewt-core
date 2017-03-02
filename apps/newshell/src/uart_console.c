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

#include <ctype.h>
#include <assert.h>

#include "os/os.h"
#include "uart/uart.h"
#include "bsp/bsp.h"

#include "console.h"

static struct uart_dev *uart_dev;

extern void __stdout_hook_install(int (*hook)(int));

static int
uart_console_out(int c)
{
    if ('\n' == c) {
        uart_blocking_tx(uart_dev, '\r');
    }
    uart_blocking_tx(uart_dev, c);

    return c;
}

/*
 * Interrupts disabled when console_tx_char/console_rx_char are called.
 * Characters sent only in blocking mode.
 */
static int
console_tx_char(void *arg)
{
    return -1;
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
uart_console_init()
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

    __stdout_hook_install(uart_console_out);

    if (!uart_dev) {
        uart_dev = (struct uart_dev *)os_dev_open(CONSOLE_UART,
          OS_TIMEOUT_NEVER, &uc);
        if (!uart_dev) {
            return -1;
        }
    }
    return 0;
}
