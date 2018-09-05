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

#include <assert.h>

#include "os/mynewt.h"
#include "hal/hal_uart.h"
#include "mcu/cmsis_nvic.h"
#include "bsp/bsp.h"

#include "nrf.h"
#include "mcu/nrf52_hal.h"


#define UARTE_INT_ENDTX		UARTE_INTEN_ENDTX_Msk
#define UARTE_INT_ENDRX		UARTE_INTEN_ENDRX_Msk
#define UARTE_CONFIG_PARITY	UARTE_CONFIG_PARITY_Msk
#define UARTE_CONFIG_HWFC	UARTE_CONFIG_HWFC_Msk
#define UARTE_ENABLE		UARTE_ENABLE_ENABLE_Enabled
#define UARTE_DISABLE       UARTE_ENABLE_ENABLE_Disabled

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

#if defined(NRF52840_XXAA)
static struct hal_uart uart0;
static struct hal_uart uart1;
#else
static struct hal_uart uart0;
#endif

int
hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
  hal_uart_rx_char rx_func, void *arg)
{
    struct hal_uart *u;

#if defined(NRF52840_XXAA)
    if (port == 0) {
        u = &uart0;
    } else if (port == 1) {
        u = &uart1;
    } else {
        return -1;
    }
#else
    if (port != 0) {
        return -1;
    }
    u = &uart0;
#endif

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
    NRF_UARTE_Type *nrf_uart;
    struct hal_uart *u;
    int sr;
    int rc;

#if defined(NRF52840_XXAA)
    if (port == 0) {
        nrf_uart = NRF_UARTE0;
        u = &uart0;
    } else if (port == 1) {
        nrf_uart = NRF_UARTE1;
        u = &uart1;
    } else {
        return;
    }
#else
    if (port != 0) {
        return;
    }
    nrf_uart = NRF_UARTE0;
    u = &uart0;
#endif

    __HAL_DISABLE_INTERRUPTS(sr);
    if (u->u_tx_started == 0) {
        rc = hal_uart_tx_fill_buf(u);
        if (rc > 0) {
            nrf_uart->INTENSET = UARTE_INT_ENDTX;
            nrf_uart->TXD.PTR = (uint32_t)&u->u_tx_buf;
            nrf_uart->TXD.MAXCNT = rc;
            nrf_uart->TASKS_STARTTX = 1;
            u->u_tx_started = 1;
        }
    }
    __HAL_ENABLE_INTERRUPTS(sr);
}

void
hal_uart_start_rx(int port)
{
    NRF_UARTE_Type *nrf_uart;
    struct hal_uart *u;
    int sr;
    int rc;

#if defined(NRF52840_XXAA)
    if (port == 0) {
        nrf_uart = NRF_UARTE0;
        u = &uart0;
    } else if (port == 1) {
        nrf_uart = NRF_UARTE1;
        u = &uart1;
    } else {
        return;
    }
#else
    if (port != 0) {
        return;
    }
    nrf_uart = NRF_UARTE0;
    u = &uart0;
#endif

    if (u->u_rx_stall) {
        __HAL_DISABLE_INTERRUPTS(sr);
        rc = u->u_rx_func(u->u_func_arg, u->u_rx_buf);
        if (rc == 0) {
            u->u_rx_stall = 0;
            nrf_uart->TASKS_STARTRX = 1;
        }

        __HAL_ENABLE_INTERRUPTS(sr);
    }
}

void
hal_uart_blocking_tx(int port, uint8_t data)
{
    struct hal_uart *u;
    NRF_UARTE_Type *nrf_uart;

#if defined(NRF52840_XXAA)
    if (port == 0) {
        nrf_uart = NRF_UARTE0;
        u = &uart0;
    } else if (port == 1) {
        nrf_uart = NRF_UARTE1;
        u = &uart1;
    } else {
        return;
    }
#else
    if (port != 0) {
        return;
    }
    nrf_uart = NRF_UARTE0;
    u = &uart0;
#endif

    if (!u->u_open) {
        return;
    }

    /* If we have started, wait until the current uart dma buffer is done */
    if (u->u_tx_started) {
        while (nrf_uart->EVENTS_ENDTX == 0) {
            /* Wait here until the dma is finished */
        }
    }

    nrf_uart->EVENTS_ENDTX = 0;
    nrf_uart->TXD.PTR = (uint32_t)&data;
    nrf_uart->TXD.MAXCNT = 1;
    nrf_uart->TASKS_STARTTX = 1;

    while (nrf_uart->EVENTS_ENDTX == 0) {
        /* Wait till done */
    }

    /* Stop the uart */
    nrf_uart->TASKS_STOPTX = 1;
}

static void
uart_irq_handler(NRF_UARTE_Type *nrf_uart, struct hal_uart *u)
{
    int rc;

    os_trace_isr_enter();

    if (nrf_uart->EVENTS_ENDTX) {
        nrf_uart->EVENTS_ENDTX = 0;
        rc = hal_uart_tx_fill_buf(u);
        if (rc > 0) {
            nrf_uart->TXD.PTR = (uint32_t)&u->u_tx_buf;
            nrf_uart->TXD.MAXCNT = rc;
            nrf_uart->TASKS_STARTTX = 1;
        } else {
            if (u->u_tx_done) {
                u->u_tx_done(u->u_func_arg);
            }
            nrf_uart->INTENCLR = UARTE_INT_ENDTX;
            nrf_uart->TASKS_STOPTX = 1;
            u->u_tx_started = 0;
        }
    }
    if (nrf_uart->EVENTS_ENDRX) {
        nrf_uart->EVENTS_ENDRX = 0;
        rc = u->u_rx_func(u->u_func_arg, u->u_rx_buf);
        if (rc < 0) {
            u->u_rx_stall = 1;
        } else {
            nrf_uart->TASKS_STARTRX = 1;
        }
    }
    os_trace_isr_exit();
}

static void
uart0_irq_handler(void)
{
    uart_irq_handler(NRF_UARTE0, &uart0);
}

#if defined(NRF52840_XXAA)
static void
uart1_irq_handler(void)
{
    uart_irq_handler(NRF_UARTE1, &uart1);
}
#endif

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
    NRF_UARTE_Type *nrf_uart;

#if defined(NRF52840_XXAA)
    if (port == 0) {
        nrf_uart = NRF_UARTE0;
        NVIC_SetVector(UARTE0_UART0_IRQn, (uint32_t)uart0_irq_handler);
    } else if (port == 1) {
        nrf_uart = NRF_UARTE1;
        NVIC_SetVector(UARTE1_IRQn, (uint32_t)uart1_irq_handler);
    } else {
        return -1;
    }
#else
    if (port != 0) {
        return -1;
    }
    nrf_uart = NRF_UARTE0;
    NVIC_SetVector(UARTE0_UART0_IRQn, (uint32_t)uart0_irq_handler);
#endif

    cfg = (struct nrf52_uart_cfg *)arg;

    nrf_uart->PSEL.TXD = cfg->suc_pin_tx;
    nrf_uart->PSEL.RXD = cfg->suc_pin_rx;
    nrf_uart->PSEL.RTS = cfg->suc_pin_rts;
    nrf_uart->PSEL.CTS = cfg->suc_pin_cts;

    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct hal_uart *u;
    uint32_t cfg_reg = 0;
    uint32_t baud_reg;
    NRF_UARTE_Type *nrf_uart;
    IRQn_Type irqnum;

#if defined(NRF52840_XXAA)
    if (port == 0) {
        nrf_uart = NRF_UARTE0;
        irqnum = UARTE0_UART0_IRQn;
        u = &uart0;
    } else if (port == 1) {
        nrf_uart = NRF_UARTE1;
        irqnum = UARTE1_IRQn;
        u = &uart1;
    } else {
        return -1;
    }
#else
    if (port != 0) {
        return -1;
    }
    nrf_uart = NRF_UARTE0;
    irqnum = UARTE0_UART0_IRQn;
    u = &uart0;
#endif

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
        if (nrf_uart->PSEL.RTS == 0xffffffff ||
          nrf_uart->PSEL.CTS == 0xffffffff) {
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
    nrf_uart->ENABLE = 0;
    nrf_uart->INTENCLR = 0xffffffff;
    nrf_uart->BAUDRATE = baud_reg;
    nrf_uart->CONFIG = cfg_reg;

    NVIC_EnableIRQ(irqnum);

    nrf_uart->ENABLE = UARTE_ENABLE;

    nrf_uart->INTENSET = UARTE_INT_ENDRX;
    nrf_uart->RXD.PTR = (uint32_t)&u->u_rx_buf;
    nrf_uart->RXD.MAXCNT = sizeof(u->u_rx_buf);
    nrf_uart->TASKS_STARTRX = 1;

    u->u_rx_stall = 0;
    u->u_tx_started = 0;
    u->u_open = 1;

    return 0;
}

int
hal_uart_close(int port)
{
    struct hal_uart *u;
    NRF_UARTE_Type *nrf_uart;

#if defined(NRF52840_XXAA)
    if (port == 0) {
        nrf_uart = NRF_UARTE0;
        u = &uart0;
    } else if (port == 1) {
        nrf_uart = NRF_UARTE1;
        u = &uart1;
    } else {
        return -1;
    }
#else
    if (port != 0) {
        return -1;
    }
    nrf_uart = NRF_UARTE0;
    u = &uart0;
#endif

    u->u_open = 0;
    nrf_uart->ENABLE = 0;
    nrf_uart->INTENCLR = 0xffffffff;
    return 0;
}
