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

#include "hal/hal_uart.h"
#include "bsp/bsp.h"
#include "mcu/fe310_hal.h"
#include "env/freedom-e300-hifive1/platform.h"
#include <mcu/plic.h>

#include <assert.h>

/*
 * Only one UART on FE310.
 */
struct hal_uart {
    uint8_t u_open:1;
    uint8_t u_rx_stall:1;
    uint8_t u_tx_started:1;
    uint8_t u_rx_buf;
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
    uint32_t u_baudrate;
};
static struct hal_uart uart;

int
hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
  hal_uart_rx_char rx_func, void *arg)
{
    struct hal_uart *u;

    if (port != 0) {
        return -1;
    }
    u = &uart;
    if (u->u_open) {
        return -1;
    }
    u->u_rx_func = rx_func;
    u->u_tx_func = tx_func;
    u->u_tx_done = tx_done;
    u->u_func_arg = arg;
    return 0;
}

static int
fe310_hal_uart_tx_fill_fifo(struct hal_uart *u)
{
    int data = 0;

    while ((int32_t)(UART0_REG(UART_REG_TXFIFO)) >= 0) {
        data = u->u_tx_func(u->u_func_arg);
        if (data < 0) {
            if (u->u_tx_done) {
                u->u_tx_done(u->u_func_arg);
            }
            /* No more interrupts for TX */
            UART0_REG(UART_REG_IE) &= ~UART_IP_TXWM;
            u->u_tx_started = 0;
            break;
        } else {
            UART0_REG(UART_REG_TXFIFO) = data;
        }
    }
    return data;
}

void
hal_uart_start_tx(int port)
{
    struct hal_uart *u;
    int sr;
    int rc;

    if (port != 0) {
        return;
    }
    u = &uart;
    __HAL_DISABLE_INTERRUPTS(sr);
    if (u->u_tx_started == 0) {
        UART0_REG(UART_REG_TXCTRL) |= UART_TXEN;
        rc = fe310_hal_uart_tx_fill_fifo(u);
        if (rc >= 0) {
            u->u_tx_started = 1;
            UART0_REG(UART_REG_IE) |= UART_IP_TXWM;
        }
    }
    __HAL_ENABLE_INTERRUPTS(sr);
}

void
hal_uart_start_rx(int port)
{
    struct hal_uart *u;
    int sr;
    int rc;

    if (port != 0) {
        return;
    }
    u = &uart;
    if (u->u_rx_stall) {
        __HAL_DISABLE_INTERRUPTS(sr);
        rc = u->u_rx_func(u->u_func_arg, u->u_rx_buf);
        if (rc == 0) {
            u->u_rx_stall = 0;
            UART0_REG(UART_REG_IE) |= UART_IP_RXWM;
        }

        __HAL_ENABLE_INTERRUPTS(sr);
    }
}

void
hal_uart_blocking_tx(int port, uint8_t data)
{
    struct hal_uart *u;

    if (port != 0) {
        return;
    }

    u = &uart;
    if (!u->u_open) {
        return;
    }

    UART0_REG(UART_REG_TXCTRL) |= UART_TXEN;
    while (UART0_REG(UART_REG_TXFIFO) < 0) {
    }
    UART0_REG(UART_REG_TXFIFO) = data;
}

static void
fe310_uart_irq_handler(int num)
{
    struct hal_uart *u;
    int rc;
    int data;

    u = &uart;
    /* RX Path */
    while (1) {
        data = UART0_REG(UART_REG_RXFIFO);
        if (data < 0) {
            break;
        }
        rc = u->u_rx_func(u->u_func_arg, (uint8_t)data);
        if (rc < 0) {
            /* Don't rise RX interrupts till next hal_uart_start_rx */
            UART0_REG(UART_REG_IE) &= ~UART_IP_RXWM;
            u->u_rx_buf = (uint8_t)data;
            u->u_rx_stall = 1;
        }
    }

    /* TX Path */
    if (u->u_tx_started) {
        fe310_hal_uart_tx_fill_fifo(u);
    }
}

int
hal_uart_init(int port, void *arg)
{
    uint32_t mask;
    struct fe310_uart_cfg *cfg = (struct fe310_uart_cfg *)arg;

    if (port != 0) {
        return -1;
    }

    /* Configure RX/TX pins */
    mask = (1 << cfg->suc_pin_tx) | (1 << cfg->suc_pin_rx);
    GPIO_REG(GPIO_IOF_EN) |= mask;
    GPIO_REG(GPIO_IOF_SEL) &= ~mask;

    /* Install interrupt handler */
    plic_set_handler(INT_UART0_BASE, fe310_uart_irq_handler, 3);

    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct hal_uart *u;

    if (port != 0) {
        return -1;
    }

    u = &uart;
    if (u->u_open) {
        return -1;
    }

    /*
     * pin config
     * UART config
     * nvic config
     * enable uart
     */
    if (databits != 8) {
        return -1;
    }
    if (stopbits > 2) {
        return -1;
    }

    switch (parity) {
    case HAL_UART_PARITY_NONE:
        break;
    case HAL_UART_PARITY_ODD:
    case HAL_UART_PARITY_EVEN:
        return -1;
    }

    switch (flow_ctl) {
    case HAL_UART_FLOW_CTL_NONE:
        break;
    case HAL_UART_FLOW_CTL_RTS_CTS:
        return -1;
    }
    UART0_REG(UART_REG_DIV) = (get_cpu_freq() + (baudrate / 2)) / baudrate - 1;
    /* Set threashold and stop bits, not enable yet */
    UART0_REG(UART_REG_TXCTRL) = UART_TXWM(4) | ((stopbits - 1) << 1);
    /* RX anebled with interrupt when any bytes is there */
    UART0_REG(UART_REG_RXCTRL) = UART_RXWM(0) | UART_RXEN;
    UART0_REG(UART_REG_IE) = UART_IP_RXWM;

    plic_enable_interrupt(INT_UART0_BASE);

    u->u_rx_stall = 0;
    u->u_tx_started = 0;
    u->u_open = 1;
    u->u_baudrate = baudrate;

    return 0;
}

int
hal_uart_close(int port)
{
    struct hal_uart *u;

    u = &uart;

    if (port == 0) {
        u->u_open = 0;
        UART0_REG(UART_REG_TXCTRL) &= ~UART_TXEN;
        UART0_REG(UART_REG_RXCTRL) = 0;
        plic_disable_interrupt(INT_UART0_BASE);
        return 0;
    }
    return -1;
}

void
hal_uart_sys_clock_changed(void)
{
    struct hal_uart *u;

    u = &uart;

    if (u->u_open) {
        UART0_REG(UART_REG_DIV) = (get_cpu_freq() + (u->u_baudrate / 2)) / u->u_baudrate - 1;
    }
}
