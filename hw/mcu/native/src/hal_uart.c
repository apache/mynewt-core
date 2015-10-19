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

#include "os/os.h"
#include "hal/hal_uart.h"
#include "bsp/bsp.h"

#include <util.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#define UART_MAX_BYTES_PER_POLL	64
#define UART_POLLER_STACK_SZ	1024
#define UART_POLLER_PRIO	0

struct uart {
    int u_open;
    int u_fd;
    int u_tx_run;
    int u_rx_char;
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
};

/*
 * XXXX should try to use O_ASYNC/SIGIO for byte arrival notification,
 * so we wouldn't need an OS for pseudo ttys.
 */
static struct uart uarts[UART_CNT];
static int uart_poller_running;
static struct os_task uart_poller_task;
static os_stack_t uart_poller_stack[UART_POLLER_STACK_SZ];

static void
uart_poller(void *arg)
{
    int i;
    int rc;
    int bytes;
    int sr;
    char ch;
    struct uart *uart;

    while (1) {
        for (i = 0; i < UART_CNT; i++) {
            if (!uarts[i].u_open) {
                continue;
            }
            uart = &uarts[i];

            for (bytes = 0; bytes < UART_MAX_BYTES_PER_POLL; bytes++) {
                if (uart->u_tx_run) {
                    OS_ENTER_CRITICAL(sr);
                    rc = uart->u_tx_func(uart->u_func_arg);
                    if (rc < 0) {
                        /*
                         * No more data to send.
                         */
                        uart->u_tx_run = 0;
                        if (uart->u_tx_done) {
                            uart->u_tx_done(uart->u_func_arg);
                        }
                        OS_EXIT_CRITICAL(sr);
                        break;
                    }
                    OS_EXIT_CRITICAL(sr);
                    ch = rc;
                    rc = write(uart->u_fd, &ch, 1);
                    if (rc <= 0) {
                        /* XXX EOF/error, what now? */
                        assert(0);
                        break;
                    }
                }
            }
            for (bytes = 0; bytes < UART_MAX_BYTES_PER_POLL; bytes++) {
                if (uart->u_rx_char < 0) {
                    rc = read(uart->u_fd, &ch, 1);
                    if (rc == 0) {
                        /* XXX EOF, what now? */
                        assert(0);
                    } else if (rc < 0) {
                        break;
                    } else {
                        uart->u_rx_char = ch;
                    }
                }
                if (uart->u_rx_char >= 0) {
                    OS_ENTER_CRITICAL(sr);
                    rc = uart->u_rx_func(uart->u_func_arg, uart->u_rx_char);
                    /* Delivered */
                    if (rc >= 0) {
                        uart->u_rx_char = -1;
                    }
                    OS_EXIT_CRITICAL(sr);
                }
            }
        }
        os_time_delay(10);
    }
}

static int
uart_pty(void)
{
    int fd;
    int loop_slave;
    int flags;
    struct termios tios;
    char pty_name[32];

    if (openpty(&fd, &loop_slave, pty_name, NULL, NULL) < 0) {
        perror("openpty() failed");
        return -1;
    }

    if (tcgetattr(loop_slave, &tios)) {
        perror("tcgetattr() failed");
        goto err;
    }

    tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
    tios.c_cflag |= CS8 | CREAD;
    tios.c_iflag = IGNPAR | IXON;
    tios.c_oflag = 0;
    tios.c_lflag = 0;
    if (tcsetattr(loop_slave, TCSAFLUSH, &tios) < 0) {
        perror("tcsetattr() failed");
        goto err;
    }

    flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        perror("fcntl(F_GETFL) fail");
        goto err;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl(F_SETFL) fail");
        goto err;
    }

    printf("console at %s\n", pty_name);
    return fd;
err:
    close(fd);
    close(loop_slave);
    return -1;
}

void
hal_uart_start_tx(int port)
{
    int sr;

    if (port >= UART_CNT || uarts[port].u_open == 0) {
        return;
    }
    OS_ENTER_CRITICAL(sr);
    uarts[port].u_tx_run = 1;
    OS_EXIT_CRITICAL(sr);
}

void
hal_uart_start_rx(int port)
{
    /* nothing to do here */
}

int
hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
  hal_uart_rx_char rx_func, void *arg)
{
    struct uart *uart;
    int rc;

    if (port >= UART_CNT) {
        return -1;
    }

    uart = &uarts[port];
    if (uart->u_open) {
        return -1;
    }
    uart->u_tx_func = tx_func;
    uart->u_tx_done = tx_done;
    uart->u_rx_func = rx_func;
    uart->u_func_arg = arg;
    uart->u_rx_char = -1;

    if (!uart_poller_running) {
        uart_poller_running = 1;
        rc = os_task_init(&uart_poller_task, "uart_poller", uart_poller, NULL,
          UART_POLLER_PRIO, OS_WAIT_FOREVER, uart_poller_stack,
          UART_POLLER_STACK_SZ);
        assert(rc == 0);
    }
    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct uart *uart;

    if (port >= UART_CNT) {
        return -1;
    }

    uart = &uarts[port];
    if (uart->u_open) {
        return -1;
    }
    uart->u_fd = uart_pty();
    if (uart->u_fd < 0) {
        return -1;
    }
    uart->u_open = 1;
    return 0;
}
