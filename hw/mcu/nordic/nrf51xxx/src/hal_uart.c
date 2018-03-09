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
#include "mcu/cmsis_nvic.h"
#include "bsp/bsp.h"

#include "nrf.h"
#include "mcu/nrf51_hal.h"

#include <assert.h>

#define UART_INT_TXDRDY		UART_INTENSET_TXDRDY_Msk
#define UART_INT_RXDRDY		UART_INTENSET_RXDRDY_Msk
#define UART_CONFIG_PARITY	UART_CONFIG_PARITY_Msk
#define UART_CONFIG_HWFC	UART_CONFIG_HWFC_Msk
#define UART_ENABLE		    UART_ENABLE_ENABLE_Enabled

/* Only one UART on NRF 51xxx. */
struct hal_uart {
    uint8_t u_open:1;
    uint8_t u_rx_stall:1;
    uint8_t u_tx_started:1;
    uint8_t u_tx_buf;
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
    const struct nrf51_uart_cfg *u_cfg;
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
hal_uart_tx_fill_buf(struct hal_uart *u)
{
    int data;
    int i;

    i = 0;
    data = u->u_tx_func(u->u_func_arg);
    if (data >= 0) {
        u->u_tx_buf = data;
        i = 1;
    }
    return i;
}

void
hal_uart_start_tx(int port)
{
    struct hal_uart *u;
    int sr;
    int rc;

    u = &uart;
    __HAL_DISABLE_INTERRUPTS(sr);
    if (u->u_tx_started == 0) {
        rc = hal_uart_tx_fill_buf(u);
        if (rc > 0) {
            NRF_UART0->INTENSET = UART_INT_TXDRDY;
            NRF_UART0->TXD = u->u_tx_buf;
            NRF_UART0->TASKS_STARTTX = 1;
            u->u_tx_started = 1;
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
    uint32_t ch;

    u = &uart;
    if (u->u_rx_stall) {
        __HAL_DISABLE_INTERRUPTS(sr);
        while (NRF_UART0->EVENTS_RXDRDY) {
            NRF_UART0->EVENTS_RXDRDY = 0;
            ch = NRF_UART0->RXD;
            rc = u->u_rx_func(u->u_func_arg, ch);
            if (rc == 0) {
                u->u_rx_stall = 0;
                NRF_UART0->TASKS_STARTRX = 1;
            }
        }
        __HAL_ENABLE_INTERRUPTS(sr);
    }
}

void
hal_uart_blocking_tx(int port, uint8_t data)
{
    struct hal_uart *u;

    u = &uart;
    if (!u->u_open) {
        return;
    }

    /* If we have started, wait until the current uart dma buffer is done */
    if (u->u_tx_started) {
        while (NRF_UART0->EVENTS_TXDRDY == 0) {
            /* Wait here until the dma is finished */
        }
    }

    NRF_UART0->EVENTS_TXDRDY = 0;
    NRF_UART0->TXD = (uint32_t)data;
    NRF_UART0->TASKS_STARTTX = 1;

    while (NRF_UART0->EVENTS_TXDRDY == 0) {
        /* Wait till done */
    }

    /* Stop the uart */
    NRF_UART0->TASKS_STOPTX = 1;
}

static void
uart_irq_handler(void)
{
    struct hal_uart *u;
    int rc;
    uint32_t ch;

    u = &uart;
    if (NRF_UART0->EVENTS_TXDRDY) {
        NRF_UART0->EVENTS_TXDRDY = 0;
        rc = hal_uart_tx_fill_buf(u);
        if (rc > 0) {
            NRF_UART0->TXD = u->u_tx_buf;
            NRF_UART0->TASKS_STARTTX = 1;
        } else {
            if (u->u_tx_done) {
                u->u_tx_done(u->u_func_arg);
            }
            NRF_UART0->INTENCLR = UART_INT_TXDRDY;
            NRF_UART0->TASKS_STOPTX = 1;
            u->u_tx_started = 0;
        }
    }
    while (NRF_UART0->EVENTS_RXDRDY) {
        NRF_UART0->EVENTS_RXDRDY = 0;
        ch = NRF_UART0->RXD;
        rc = u->u_rx_func(u->u_func_arg, ch);
        if (rc < 0) {
            u->u_rx_stall = 1;
        } else {
            NRF_UART0->TASKS_STARTRX = 1;
        }
    }
}

static uint32_t
hal_uart_baudrate(int baudrate)
{
    switch (baudrate) {
    case 9600:
        return UART_BAUDRATE_BAUDRATE_Baud9600;
    case 19200:
        return UART_BAUDRATE_BAUDRATE_Baud19200;
    case 38400:
        return UART_BAUDRATE_BAUDRATE_Baud38400;
    case 57600:
        return UART_BAUDRATE_BAUDRATE_Baud57600;
    case 115200:
        return UART_BAUDRATE_BAUDRATE_Baud115200;
    case 230400:
        return UART_BAUDRATE_BAUDRATE_Baud230400;
    case 460800:
        return UART_BAUDRATE_BAUDRATE_Baud460800;
    case 921600:
        return UART_BAUDRATE_BAUDRATE_Baud921600;
    case 1000000:
        return UART_BAUDRATE_BAUDRATE_Baud1M;
    default:
        return 0;
    }
}

int
hal_uart_init(int port, void *arg)
{
    struct hal_uart *u;

    if (port != 0) {
        return -1;
    }

    u = &uart;
    if (u->u_open) {
        return -1;
    }
    u->u_cfg = arg;

    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct hal_uart *u;
    const struct nrf51_uart_cfg *cfg;
    uint32_t cfg_reg = 0;
    uint32_t baud_reg;

    if (port != 0) {
        return -1;
    }

    u = &uart;
    if (u->u_open) {
        return -1;
    }
    cfg = u->u_cfg;
    assert(cfg);

    /*
     * pin config
     * UART config
     * nvic config
     * enable uart
     */
    if (databits != 8) {
        return -1;
    }
    if (stopbits != 1) {
        return -1;
    }

    switch (parity) {
    case HAL_UART_PARITY_NONE:
        break;
    case HAL_UART_PARITY_ODD:
        return -1;
    case HAL_UART_PARITY_EVEN:
        cfg_reg |= UART_CONFIG_PARITY;
        break;
    }

    switch (flow_ctl) {
    case HAL_UART_FLOW_CTL_NONE:
        break;
    case HAL_UART_FLOW_CTL_RTS_CTS:
        cfg_reg |= UART_CONFIG_HWFC;
        if (cfg->suc_pin_rts < 0 || cfg->suc_pin_cts < 0) {
            /*
             * Can't turn on HW flow control if pins to do that are not
             * defined.
             */
            assert(0);
            return -1;
        }
        break;
    }
    baud_reg = hal_uart_baudrate(baudrate);
    if (baud_reg == 0) {
        return -1;
    }
    NRF_UART0->ENABLE = 0;
    NRF_UART0->INTENCLR = 0xffffffff;
    NRF_UART0->PSELTXD = cfg->suc_pin_tx;
    NRF_UART0->PSELRXD = cfg->suc_pin_rx;
    if (flow_ctl == HAL_UART_FLOW_CTL_RTS_CTS) {
        NRF_UART0->PSELRTS = cfg->suc_pin_rts;
        NRF_UART0->PSELCTS = cfg->suc_pin_cts;
    } else {
        NRF_UART0->PSELRTS = 0xffffffff;
        NRF_UART0->PSELCTS = 0xffffffff;
    }
    NRF_UART0->BAUDRATE = baud_reg;
    NRF_UART0->CONFIG = cfg_reg;

    NVIC_SetVector(UART0_IRQn, (uint32_t)uart_irq_handler);
    NVIC_EnableIRQ(UART0_IRQn);

    NRF_UART0->ENABLE = UART_ENABLE;
    NRF_UART0->INTENSET = UART_INT_RXDRDY;
    NRF_UART0->TASKS_STARTRX = 1;

    u->u_open = 1;

    return 0;
}

int
hal_uart_close(int port)
{
    struct hal_uart *u;

    u = &uart;

    if (port == 0) {
        u->u_open = 0;
        NRF_UART0->ENABLE = 0;
        NRF_UART0->INTENCLR = 0xffffffff;
        return 0;
    }
    return -1;
}
