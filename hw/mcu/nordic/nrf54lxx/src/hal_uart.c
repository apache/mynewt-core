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
#include <os/mynewt.h>
#include <hal/hal_uart.h>
#include <mcu/cmsis_nvic.h>
#include <bsp/bsp.h>
#include <nrf.h>
#include <mcu/nrf54l_hal.h>

#define UARTE_INT_ENDTX         UARTE_INTEN_DMATXEND_Msk
#define UARTE_INT_ENDRX         UARTE_INTEN_DMARXEND_Msk
#define UARTE_CONFIG_PARITY     UARTE_CONFIG_PARITY_Msk
#define UARTE_CONFIG_PARITY_ODD UARTE_CONFIG_PARITYTYPE_Msk
#define UARTE_CONFIG_HWFC       UARTE_CONFIG_HWFC_Msk
#define UARTE_ENABLE            UARTE_ENABLE_ENABLE_Enabled
#define UARTE_DISABLE           UARTE_ENABLE_ENABLE_Disabled

struct hal_uart {
    uint8_t u_open : 1;
    uint8_t u_rx_stall : 1;
    uint8_t u_tx_started : 1;
    uint8_t u_rx_buf;
    uint8_t u_tx_buf[8];
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;

    NRF_UARTE_Type *nrf_uart;
    uint32_t handler;
    IRQn_Type irqn;
};


#if MYNEWT_VAL(UART_0)
static struct hal_uart uart0;
#endif
#if MYNEWT_VAL(UART_1)
static struct hal_uart uart1;
#endif
#if MYNEWT_VAL(UART_2)
static struct hal_uart uart2;
#endif
#if MYNEWT_VAL(UART_3)
static struct hal_uart uart3;
#endif

static struct hal_uart *
hal_uart_get(int port)
{
    switch (port) {
#if MYNEWT_VAL(UART_0)
    case 0:
        return &uart0;
#endif
#if MYNEWT_VAL(UART_1)
    case 1:
        return &uart1;
#endif
#if MYNEWT_VAL(UART_2)
    case 2:
        return &uart2;
#endif
    default:
        return NULL;
    }
}

int
hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
                  hal_uart_rx_char rx_func, void *arg)
{
    struct hal_uart *u = hal_uart_get(port);

    if (!u || u->u_open) {
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
    struct hal_uart *u = hal_uart_get(port);
    int sr;
    int rc;

    if (!u) {
        return;
    }

    __HAL_DISABLE_INTERRUPTS(sr);
    if (u->u_tx_started == 0) {
        rc = hal_uart_tx_fill_buf(u);
        if (rc > 0) {
            u->nrf_uart->INTENSET = UARTE_INT_ENDTX;
            u->nrf_uart->DMA.TX.PTR = (uint32_t)&u->u_tx_buf;
            u->nrf_uart->DMA.TX.MAXCNT = rc;
            u->nrf_uart->TASKS_DMA.TX.START = 1;
            u->u_tx_started = 1;
        }
    }
    __HAL_ENABLE_INTERRUPTS(sr);
}

void
hal_uart_start_rx(int port)
{
    struct hal_uart *u = hal_uart_get(port);
    int sr;
    int rc;

    if (!u) {
        return;
    }

    if (u->u_rx_stall) {
        __HAL_DISABLE_INTERRUPTS(sr);
        rc = u->u_rx_func(u->u_func_arg, u->u_rx_buf);
        if (rc == 0) {
            u->u_rx_stall = 0;
            u->nrf_uart->TASKS_DMA.RX.START = 1;
        }

        __HAL_ENABLE_INTERRUPTS(sr);
    }
}

void
hal_uart_blocking_tx(int port, uint8_t data)
{
    struct hal_uart *u = hal_uart_get(port);

    if (!u || !u->u_open) {
        return;
    }

    /* If we have started, wait until the current uart dma buffer is done */
    if (u->u_tx_started) {
        while (u->nrf_uart->EVENTS_DMA.TX.END == 0) {
            /* Wait here until the dma is finished */
        }
    }

    u->nrf_uart->EVENTS_DMA.TX.END = 0;
    u->nrf_uart->DMA.TX.PTR = (uint32_t)&data;
    u->nrf_uart->DMA.TX.MAXCNT = 1;
    u->nrf_uart->TASKS_DMA.TX.START = 1;

    while (u->nrf_uart->EVENTS_DMA.TX.END == 0) {
        /* Wait till done */
    }

    /* Stop the uart */
    u->nrf_uart->TASKS_DMA.TX.STOP = 1;
}

#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1) || MYNEWT_VAL(UART_2) || MYNEWT_VAL(UART_3)
static void
uart_irq_handler(struct hal_uart *u)
{
    NRF_UARTE_Type *nrf_uart = u->nrf_uart;
    int rc;

    os_trace_isr_enter();

    if (nrf_uart->EVENTS_DMA.TX.END) {
        nrf_uart->EVENTS_DMA.TX.END = 0;
        rc = hal_uart_tx_fill_buf(u);
        if (rc > 0) {
            nrf_uart->DMA.TX.PTR = (uint32_t)&u->u_tx_buf;
            nrf_uart->DMA.TX.MAXCNT = rc;
            nrf_uart->TASKS_DMA.TX.START = 1;
        } else {
            if (u->u_tx_done) {
                u->u_tx_done(u->u_func_arg);
            }
            nrf_uart->INTENCLR = UARTE_INT_ENDTX;
            nrf_uart->TASKS_DMA.TX.STOP = 1;
            u->u_tx_started = 0;
        }
    }
    if (nrf_uart->EVENTS_DMA.RX.END) {
        nrf_uart->EVENTS_DMA.RX.END = 0;
        rc = u->u_rx_func(u->u_func_arg, u->u_rx_buf);
        if (rc < 0) {
            u->u_rx_stall = 1;
        } else {
            nrf_uart->TASKS_DMA.RX.START = 1;
        }
    }
    os_trace_isr_exit();
}
#endif

#if MYNEWT_VAL(UART_0)
static void
uart0_irq_handler(void)
{
    uart_irq_handler(&uart0);
}
#endif

#if MYNEWT_VAL(UART_1)
static void
uart1_irq_handler(void)
{
    uart_irq_handler(&uart1);
}
#endif

#if MYNEWT_VAL(UART_2)
static void
uart2_irq_handler(void)
{
    uart_irq_handler(&uart2);
}
#endif

#if MYNEWT_VAL(UART_3)
static void
uart3_irq_handler(void)
{
    uart_irq_handler(&uart3);
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
    case 14400:
        return UARTE_BAUDRATE_BAUDRATE_Baud14400;
    case 19200:
        return UARTE_BAUDRATE_BAUDRATE_Baud19200;
    case 28800:
        return UARTE_BAUDRATE_BAUDRATE_Baud28800;
    case 38400:
        return UARTE_BAUDRATE_BAUDRATE_Baud38400;
    case 56000:
        return UARTE_BAUDRATE_BAUDRATE_Baud56000;
    case 57600:
        return UARTE_BAUDRATE_BAUDRATE_Baud57600;
    case 76800:
        return UARTE_BAUDRATE_BAUDRATE_Baud76800;
    case 115200:
        return UARTE_BAUDRATE_BAUDRATE_Baud115200;
    case 230400:
        return UARTE_BAUDRATE_BAUDRATE_Baud230400;
    case 250000:
        return UARTE_BAUDRATE_BAUDRATE_Baud250000;
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
    struct hal_uart *u = hal_uart_get(port);
    struct nrf54l_uart_cfg *cfg = arg;

    if (!u) {
        return -1;
    }

    switch (port) {
#if MYNEWT_VAL(UART_0)
    case 0:
        u->nrf_uart = NRF_UARTE20;
        u->handler = (uint32_t)uart0_irq_handler;
        u->irqn = UARTE20_IRQn;
        break;
#endif
#if MYNEWT_VAL(UART_1)
    case 1:
        u->nrf_uart = NRF_UARTE21;
        u->handler = (uint32_t)uart1_irq_handler;
        u->irqn = UARTE21_IRQn;
        break;
#endif
#if MYNEWT_VAL(UART_2)
    case 2:
        u->nrf_uart = NRF_UARTE22;
        u->handler = (uint32_t)uart2_irq_handler;
        u->irqn = UARTE22_IRQn;
        break;
#endif
    default:
        assert(false);
    }

    u->nrf_uart->PSEL.TXD = cfg->suc_pin_tx;
    u->nrf_uart->PSEL.RXD = cfg->suc_pin_rx;
    u->nrf_uart->PSEL.RTS = cfg->suc_pin_rts;
    u->nrf_uart->PSEL.CTS = cfg->suc_pin_cts;

    NVIC_SetVector(u->irqn, u->handler);

    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
                enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct hal_uart *u = hal_uart_get(port);
    uint32_t cfg_reg = 0;
    uint32_t baud_reg;

    if (!u || u->u_open) {
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
        cfg_reg |= UARTE_CONFIG_PARITY | UARTE_CONFIG_PARITY_ODD;
        break;
    case HAL_UART_PARITY_EVEN:
        cfg_reg |= UARTE_CONFIG_PARITY;
        break;
    }

    switch (flow_ctl) {
    case HAL_UART_FLOW_CTL_NONE:
        break;
    case HAL_UART_FLOW_CTL_RTS_CTS:
        cfg_reg |= UARTE_CONFIG_HWFC;
        if (u->nrf_uart->PSEL.RTS == 0xffffffff ||
            u->nrf_uart->PSEL.CTS == 0xffffffff) {
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
    u->nrf_uart->ENABLE = 0;
    u->nrf_uart->INTENCLR = 0xffffffff;
    u->nrf_uart->BAUDRATE = baud_reg;
    u->nrf_uart->CONFIG = cfg_reg;

    NVIC_EnableIRQ(u->irqn);

    u->nrf_uart->ENABLE = UARTE_ENABLE;

    u->nrf_uart->INTENSET = UARTE_INT_ENDRX;
    u->nrf_uart->DMA.RX.PTR = (uint32_t)&u->u_rx_buf;
    u->nrf_uart->DMA.RX.MAXCNT = sizeof(u->u_rx_buf);
    u->nrf_uart->TASKS_DMA.RX.START = 1;

    u->u_rx_stall = 0;
    u->u_tx_started = 0;
    u->u_open = 1;

    return 0;
}

int
hal_uart_close(int port)
{
    volatile struct hal_uart *u = hal_uart_get(port);

    if (!u) {
        return -1;
    }

    u->u_open = 0;
    while (u->u_tx_started) {
        /* Wait here until the dma is finished */
    }
    u->nrf_uart->ENABLE = 0;
    u->nrf_uart->INTENCLR = 0xffffffff;

    return 0;
}
