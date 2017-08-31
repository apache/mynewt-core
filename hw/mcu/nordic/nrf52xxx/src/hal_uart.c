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
#include "bsp/cmsis_nvic.h"
#include "bsp/bsp.h"

#include "nrf.h"
#include "mcu/nrf52_hal.h"
#include "os/os_trace_api.h"


#include <assert.h>

#define UARTE_INT_ENDTX		UARTE_INTEN_ENDTX_Msk
#define UARTE_INT_ENDRX		UARTE_INTEN_ENDRX_Msk
#define UARTE_CONFIG_PARITY	UARTE_CONFIG_PARITY_Msk
#define UARTE_CONFIG_HWFC	UARTE_CONFIG_HWFC_Msk
#define UARTE_ENABLE		UARTE_ENABLE_ENABLE_Enabled
#define UARTE_DISABLE           UARTE_ENABLE_ENABLE_Disabled

/*
 * Only one UART on NRF 52832.
 */
struct hal_uart {
    uint8_t u_open:1;
    uint8_t u_rx_stall:1;
    uint8_t u_tx_started:1;
    uint8_t u_rx_buf;
    uint8_t u_tx_buf[8];
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
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

    for (i = 0; i < sizeof(u->u_tx_buf); i++) {
        data = u->u_tx_func(u->u_func_arg);
        if (data < 0) {
            break;
        }
        u->u_tx_buf[i] = data;
    }
    return i;
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
        rc = hal_uart_tx_fill_buf(u);
        if (rc > 0) {
            NRF_UARTE0->INTENSET = UARTE_INT_ENDTX;
            NRF_UARTE0->TXD.PTR = (uint32_t)&u->u_tx_buf;
            NRF_UARTE0->TXD.MAXCNT = rc;
            NRF_UARTE0->TASKS_STARTTX = 1;
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

    if (port != 0) {
        return;
    }
    u = &uart;
    if (u->u_rx_stall) {
        __HAL_DISABLE_INTERRUPTS(sr);
        rc = u->u_rx_func(u->u_func_arg, u->u_rx_buf);
        if (rc == 0) {
            u->u_rx_stall = 0;
            NRF_UARTE0->TASKS_STARTRX = 1;
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

    /* If we have started, wait until the current uart dma buffer is done */
    if (u->u_tx_started) {
        while (NRF_UARTE0->EVENTS_ENDTX == 0) {
            /* Wait here until the dma is finished */
        }
    }

    NRF_UARTE0->EVENTS_ENDTX = 0;
    NRF_UARTE0->TXD.PTR = (uint32_t)&data;
    NRF_UARTE0->TXD.MAXCNT = 1;
    NRF_UARTE0->TASKS_STARTTX = 1;

    while (NRF_UARTE0->EVENTS_ENDTX == 0) {
        /* Wait till done */
    }

    /* Stop the uart */
    NRF_UARTE0->TASKS_STOPTX = 1;
}

static void
uart_irq_handler(void)
{
    struct hal_uart *u;
    int rc;

    os_trace_enter_isr();

    u = &uart;
    if (NRF_UARTE0->EVENTS_ENDTX) {
        NRF_UARTE0->EVENTS_ENDTX = 0;
        rc = hal_uart_tx_fill_buf(u);
        if (rc > 0) {
            NRF_UARTE0->TXD.PTR = (uint32_t)&u->u_tx_buf;
            NRF_UARTE0->TXD.MAXCNT = rc;
            NRF_UARTE0->TASKS_STARTTX = 1;
        } else {
            if (u->u_tx_done) {
                u->u_tx_done(u->u_func_arg);
            }
            NRF_UARTE0->INTENCLR = UARTE_INT_ENDTX;
            NRF_UARTE0->TASKS_STOPTX = 1;
            u->u_tx_started = 0;
        }
    }
    if (NRF_UARTE0->EVENTS_ENDRX) {
        NRF_UARTE0->EVENTS_ENDRX = 0;
        rc = u->u_rx_func(u->u_func_arg, u->u_rx_buf);
        if (rc < 0) {
            u->u_rx_stall = 1;
        } else {
            NRF_UARTE0->TASKS_STARTRX = 1;
        }
    }
    os_trace_exit_isr();
}

static uint32_t
hal_uart_baudrate(int baudrate)
{
    switch (baudrate) {
    case 1200:
        return UARTE_BAUDRATE_BAUDRATE_Baud1200;
    case 2400:
        return UARTE_BAUDRATE_BAUDRATE_Baud2400;
    case 4800:
        return UARTE_BAUDRATE_BAUDRATE_Baud4800;
    case 9600:
        return UARTE_BAUDRATE_BAUDRATE_Baud9600;
    case 19200:
        return UARTE_BAUDRATE_BAUDRATE_Baud19200;
    case 38400:
        return UARTE_BAUDRATE_BAUDRATE_Baud38400;
    case 57600:
        return UARTE_BAUDRATE_BAUDRATE_Baud57600;
    case 76800:
        return UARTE_BAUDRATE_BAUDRATE_Baud76800;
    case 115200:
        return UARTE_BAUDRATE_BAUDRATE_Baud115200;
    case 230400:
        return UARTE_BAUDRATE_BAUDRATE_Baud230400;
    case 460800:
        return UARTE_BAUDRATE_BAUDRATE_Baud460800;
    case 921600:
        return UARTE_BAUDRATE_BAUDRATE_Baud921600;
    case 1000000:
        return UARTE_BAUDRATE_BAUDRATE_Baud1M;
    default:
        return 0;
    }
}

int
hal_uart_init(int port, void *arg)
{
    struct nrf52_uart_cfg *cfg;

    if (port != 0) {
        return -1;
    }
    cfg = (struct nrf52_uart_cfg *)arg;

    NRF_UARTE0->PSEL.TXD = cfg->suc_pin_tx;
    NRF_UARTE0->PSEL.RXD = cfg->suc_pin_rx;
    NRF_UARTE0->PSEL.RTS = cfg->suc_pin_rts;
    NRF_UARTE0->PSEL.CTS = cfg->suc_pin_cts;

    NVIC_SetVector(UARTE0_UART0_IRQn, (uint32_t)uart_irq_handler);

    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct hal_uart *u;
    uint32_t cfg_reg = 0;
    uint32_t baud_reg;

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
    if (stopbits != 1) {
        return -1;
    }

    switch (parity) {
    case HAL_UART_PARITY_NONE:
        break;
    case HAL_UART_PARITY_ODD:
        return -1;
    case HAL_UART_PARITY_EVEN:
        cfg_reg |= UARTE_CONFIG_PARITY;
        break;
    }

    switch (flow_ctl) {
    case HAL_UART_FLOW_CTL_NONE:
        break;
    case HAL_UART_FLOW_CTL_RTS_CTS:
        cfg_reg |= UARTE_CONFIG_HWFC;
        if (NRF_UARTE0->PSEL.RTS == 0xffffffff ||
          NRF_UARTE0->PSEL.CTS == 0xffffffff) {
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
    NRF_UARTE0->ENABLE = 0;
    NRF_UARTE0->INTENCLR = 0xffffffff;
    NRF_UARTE0->BAUDRATE = baud_reg;
    NRF_UARTE0->CONFIG = cfg_reg;

    NVIC_EnableIRQ(UARTE0_UART0_IRQn);

    NRF_UARTE0->ENABLE = UARTE_ENABLE;

    NRF_UARTE0->INTENSET = UARTE_INT_ENDRX;
    NRF_UARTE0->RXD.PTR = (uint32_t)&u->u_rx_buf;
    NRF_UARTE0->RXD.MAXCNT = sizeof(u->u_rx_buf);
    NRF_UARTE0->TASKS_STARTRX = 1;

    u->u_rx_stall = 0;
    u->u_tx_started = 0;
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
        NRF_UARTE0->ENABLE = 0;
        NRF_UARTE0->INTENCLR = 0xffffffff;
        return 0;
    }
    return -1;
}
