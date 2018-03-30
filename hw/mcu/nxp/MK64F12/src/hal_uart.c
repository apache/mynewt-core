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
#include <stdlib.h>

#include "os/mynewt.h"
#include "hal/hal_uart.h"
#include "hal/hal_gpio.h"
#include "hal/hal_system.h"
#include "mcu/cmsis_nvic.h"
#include "bsp/bsp.h"

#include "mcu/frdm-k64f_hal.h"
#include "MK64F12.h"
#include "fsl_port.h"
#include "fsl_uart.h"
#include "fsl_debug_console.h"

#include "hal_uart_nxp.h"

/*! @brief Ring buffer size (Unit: Byte). */
#define TX_BUF_SZ  32
#define RX_BUF_SZ  128

struct uart_ring {
    uint16_t ur_head;
    uint16_t ur_tail;
    uint16_t ur_size;
    uint8_t _pad;
    uint8_t *ur_buf;
};

struct hal_uart {
    UART_Type       *u_base;
    clock_name_t     clk_src;
    uint32_t         u_irq;
    PORT_Type       *p_base;
    clock_ip_name_t  p_clock;
    int  u_pin_rx;
    int  u_pin_tx;
    /* TODO: support flow control pins */
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
    uint8_t u_configured:1;
    uint8_t u_open:1;
    uint8_t u_tx_started:1;
    uint8_t u_rx_stall:1;
    struct uart_ring ur_tx;
    uint8_t tx_buffer[TX_BUF_SZ];
    struct uart_ring ur_rx;
    uint8_t rx_buffer[RX_BUF_SZ];
};

/* UART configurations */
static struct hal_uart uarts[FSL_FEATURE_SOC_UART_COUNT];

static uint8_t const s_uartExists[] = NXP_UART_EXISTS;
static uint8_t const s_uartEnabled[] = NXP_UART_ENABLED;
static UART_Type *const s_uartBases[] = UART_BASE_PTRS;
static clock_name_t s_uartClocks[] = NXP_UART_CLOCKS;
static uint8_t const s_uartIRQ[] = UART_RX_TX_IRQS;
static PORT_Type *const s_uartPort[] = NXP_UART_PORTS;
static clock_ip_name_t const s_uartPortClocks[] = NXP_UART_PORT_CLOCKS;
static uint8_t const s_uartPIN_RX[] = NXP_UART_PIN_RX;
static uint8_t const s_uartPIN_TX[] = NXP_UART_PIN_TX;

static void uart_irq0(void);
static void uart_irq1(void);
static void uart_irq2(void);
static void uart_irq3(void);
static void uart_irq4(void);
static void uart_irq5(void);
static void (*s_uartirqs[])(void) = {
    uart_irq0, uart_irq1, uart_irq2, uart_irq3, uart_irq4, uart_irq5
};

/*
 * RING BUFFER FUNCTIONS
 */

static uint8_t ur_is_empty(struct uart_ring *ur)
{
    return (ur->ur_head == ur->ur_tail);
}

static uint8_t ur_is_full(struct uart_ring *ur)
{
    return (((ur->ur_tail + 1) % ur->ur_size) == ur->ur_head);
}

static void ur_bump(struct uart_ring *ur)
{
    if (!ur_is_empty(ur)) {
        ur->ur_head++;
        ur->ur_head %= ur->ur_size;
        return;
    }
}

static uint8_t ur_read(struct uart_ring *ur)
{
    return ur->ur_buf[ur->ur_head];
}

static int ur_queue(struct uart_ring *ur, uint8_t data)
{
    if (!ur_is_full(ur)) {
        ur->ur_buf[ur->ur_tail] = data;
        ur->ur_tail++;
        ur->ur_tail %= ur->ur_size;
        return 0;
    }
    return -1;
}

/*
 * END RING BUFFER FUNCTIONS
 */

int hal_uart_init_cbs(int port, hal_uart_tx_char tx_func,
  hal_uart_tx_done tx_done, hal_uart_rx_char rx_func, void *arg)
{
    struct hal_uart *u;

    if (port >= FSL_FEATURE_SOC_UART_COUNT) {
        return -1;
    }
    u = &uarts[port];
    u->u_rx_func = rx_func;
    u->u_tx_func = tx_func;
    u->u_tx_done = tx_done;
    u->u_func_arg = arg;

    return 0;
}

void hal_uart_blocking_tx(int port, uint8_t byte)
{
    struct hal_uart *u;

    if (port >= FSL_FEATURE_SOC_UART_COUNT) {
        return;
    }
    u = &uarts[port];
    if (!u->u_configured || !u->u_open) {
        return;
    }

    UART_WriteBlocking(u->u_base, &byte, 1);
}

static int
hal_uart_tx_fill_buf(struct hal_uart *u)
{
    int data = 0;
    int i = 0;
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    while (!ur_is_full(&u->ur_tx)) {
        if (u->u_tx_func) {
            data = u->u_tx_func(u->u_func_arg);
        }
        if (data <= 0) {
            break;
        }
        i++;
        ur_queue(&u->ur_tx, data);
    }
    OS_EXIT_CRITICAL(sr);
    return i;
}

void hal_uart_start_tx(int port)
{
    struct hal_uart *u;
    int data = -1;
    int rc;

    if (port >= FSL_FEATURE_SOC_UART_COUNT) {
        return;
    }
    u = &uarts[port];
    if (!u->u_configured || !u->u_open) {
        return;
    }

    /* main loop */
    while (true)
    {
        /* add data to TX ring buffer */
        if (u->u_tx_started == 0) {
            rc = hal_uart_tx_fill_buf(u);
            if (rc > 0) {
                u->u_tx_started = 1;
            }
        }

        /* Send data only when UART TX register is empty and TX ring buffer has data to send out. */
        while (!ur_is_empty(&u->ur_tx) &&
               (kUART_TxDataRegEmptyFlag & UART_GetStatusFlags(u->u_base))) {
            data = ur_read(&u->ur_tx);
            UART_WriteByte(u->u_base, data);
            ur_bump(&u->ur_tx);
        }

        if (ur_is_empty(&u->ur_tx)) {
            if (u->u_tx_done) {
                u->u_tx_done(u->u_func_arg);
            }
            u->u_tx_started = 0;
            break;
        }
    }
}

void
hal_uart_start_rx(int port)
{
    struct hal_uart *u;
    os_sr_t sr;
    int rc = 0;

    if (port >= FSL_FEATURE_SOC_UART_COUNT) {
        return;
    }
    u = &uarts[port];
    if (!u->u_configured || !u->u_open) {
        return;
    }

    u->u_rx_stall = 0;

    /* Send back what's in the RX ring buffer until it's empty or we get an
     * error */
    while ((rc >= 0) && !ur_is_empty(&u->ur_rx)) {
        OS_ENTER_CRITICAL(sr);
        rc = u->u_rx_func(u->u_func_arg, ur_read(&u->ur_rx));
        if (rc >= 0) {
            ur_bump(&u->ur_rx);
        } else {
            u->u_rx_stall = 1;
        }
        OS_EXIT_CRITICAL(sr);
    }
}

static void
uart_irq_handler(int port)
{
    struct hal_uart *u;
    uint32_t status;
    uint8_t data;

    u = &uarts[port];
    if (u->u_configured && u->u_open) {
        status = UART_GetStatusFlags(u->u_base);
        /* Check for RX data */
        if (status & (kUART_RxDataRegFullFlag | kUART_RxOverrunFlag)) {
            data = UART_ReadByte(u->u_base);
            if (u->u_rx_stall || u->u_rx_func(u->u_func_arg, data) < 0) {
                /*
                 * RX queue full.
                 */
                u->u_rx_stall = 1;
                ur_queue(&u->ur_rx, data);
            }
        }
        /* Check for TX complete */
        if (kUART_TxDataRegEmptyFlag & UART_GetStatusFlags(u->u_base)) {
            if (u->u_tx_started) {
                u->u_tx_started = 0;
                if (u->u_tx_done)
                    u->u_tx_done(u->u_func_arg);
            }
        }
    }
}

static void
uart_irq0(void)
{
    uart_irq_handler(0);
}

static void
uart_irq1(void)
{
    uart_irq_handler(1);
}

static void
uart_irq2(void)
{
    uart_irq_handler(2);
}

static void
uart_irq3(void)
{
    uart_irq_handler(3);
}

static void
uart_irq4(void)
{
    uart_irq_handler(4);
}

static void
uart_irq5(void)
{
    uart_irq_handler(5);
}

int
hal_uart_config(int port, int32_t speed, uint8_t databits, uint8_t stopbits,
                enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct hal_uart *u;
    uart_config_t uconfig;

    if (port >= FSL_FEATURE_SOC_UART_COUNT) {
        return -1;
    }
    u = &uarts[port];
    if (!u->u_configured || u->u_open) {
        return -1;
    }

    /* PIN config (all UARTs use kPORT_MuxAlt3) */
    CLOCK_EnableClock(u->p_clock);
    PORT_SetPinMux(u->p_base, u->u_pin_rx, kPORT_MuxAlt3);
    PORT_SetPinMux(u->p_base, u->u_pin_tx, kPORT_MuxAlt3);

    /* UART CONFIG */
    UART_GetDefaultConfig(&uconfig);
    uconfig.baudRate_Bps = speed;

    /* TODO: only handles 8 databits currently */

    switch (stopbits) {
    case 1:
        uconfig.stopBitCount = kUART_OneStopBit;
        break;
    case 2:
        uconfig.stopBitCount = kUART_TwoStopBit;
        break;
    default:
        return -1;
    }

    switch (parity) {
    case HAL_UART_PARITY_NONE:
        uconfig.parityMode = kUART_ParityDisabled;
        break;
    case HAL_UART_PARITY_ODD:
        uconfig.parityMode = kUART_ParityOdd;
        break;
    case HAL_UART_PARITY_EVEN:
        uconfig.parityMode = kUART_ParityEven;
        break;
    }

    /* TODO: HW flow control not supported */
    assert(flow_ctl == HAL_UART_FLOW_CTL_NONE);

    u->u_open = 1;
    u->u_tx_started = 0;

    NVIC_SetVector(u->u_irq, (uint32_t)s_uartirqs[port]);

    /* Initialize UART device */
    UART_Init(u->u_base, &uconfig, CLOCK_GetFreq(u->clk_src));
    UART_EnableTx(u->u_base, true);
    UART_EnableRx(u->u_base, true);
    UART_EnableInterrupts(u->u_base,
                          kUART_RxDataRegFullInterruptEnable |
                          kUART_RxOverrunInterruptEnable);
    EnableIRQ(u->u_irq);

    return 0;
}

int hal_uart_close(int port)
{
    struct hal_uart *u;

    if (port >= FSL_FEATURE_SOC_UART_COUNT) {
        return -1;
    }
    u = &uarts[port];
    if (!u->u_open) {
        return -1;
    }

    u->u_open = 0;
    UART_DisableInterrupts(u->u_base, kUART_RxDataRegFullInterruptEnable | kUART_RxOverrunInterruptEnable | kUART_TxDataRegEmptyInterruptEnable);
    DisableIRQ(u->u_irq);
    UART_EnableTx(u->u_base, false);
    UART_EnableRx(u->u_base, false);

    return 0;
}

int hal_uart_init(int port, void *cfg)
{
    if (s_uartExists[port]) {
        if (s_uartEnabled[port]) {
            uarts[port].u_base        = s_uartBases[port];
            uarts[port].clk_src       = s_uartClocks[port];
            uarts[port].u_irq         = s_uartIRQ[port];
            uarts[port].p_base        = s_uartPort[port];
            uarts[port].p_clock       = s_uartPortClocks[port];
            uarts[port].u_pin_rx      = s_uartPIN_RX[port];
            uarts[port].u_pin_tx      = s_uartPIN_TX[port];
            uarts[port].ur_tx.ur_buf  = uarts[port].tx_buffer;
            uarts[port].ur_tx.ur_size = TX_BUF_SZ;
            uarts[port].ur_tx.ur_head = 0;
            uarts[port].ur_tx.ur_tail = 0;
            uarts[port].ur_rx.ur_buf  = uarts[port].rx_buffer;
            uarts[port].ur_rx.ur_size = RX_BUF_SZ;
            uarts[port].ur_rx.ur_head = 0;
            uarts[port].ur_rx.ur_tail = 0;
            uarts[port].u_configured  = 1;
        }
        else {
            uarts[port].u_configured = 0;
        }
    }

    return 0;
}
