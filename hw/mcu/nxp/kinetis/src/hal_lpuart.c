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
#include "syscfg/syscfg.h"
#include <fsl_device_registers.h>

#include "fsl_clock.h"
#include "fsl_port.h"
#include "fsl_lpuart.h"

#include "hal_lpuart_nxp.h"

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
    LPUART_Type *u_base;
    clock_name_t clk_src;
    uint32_t u_irq;
    PORT_Type *p_base;
    clock_ip_name_t p_clock;
    int u_pin_rx;
    int u_pin_tx;
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
    uint8_t u_configured : 1;
    uint8_t u_open : 1;
    uint8_t u_tx_started : 1;
    uint8_t u_rx_stall : 1;
    struct uart_ring ur_tx;
    uint8_t tx_buffer[TX_BUF_SZ];
    struct uart_ring ur_rx;
    uint8_t rx_buffer[RX_BUF_SZ];
};

/* UART configurations */
#define UART_CNT (((uint8_t)(MYNEWT_VAL(UART_0) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_1) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_2) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_3) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_4) != 0)))

static struct hal_uart uarts[UART_CNT];
static struct hal_uart *
uart_by_port(int port)
{
    int index;

    (void)index;
    (void)uarts;

    index = 0;
#if MYNEWT_VAL(UART_0)
    if (port == 0) {
        return &uarts[index];
    }
    index++;
#endif
#if MYNEWT_VAL(UART_1)
    if (port == 1) {
        return &uarts[index];
    }
    index++;
#endif
#if MYNEWT_VAL(UART_2)
#   if FSL_FEATURE_SOC_LPUART_COUNT < 3
#       error "UART_2 is not supported on this MCU"
#   endif
    if (port == 2) {
        return &uarts[index];
    }
    index++;
#endif
#if MYNEWT_VAL(UART_3)
#   if FSL_FEATURE_SOC_LPUART_COUNT < 4
#       error "UART_3 is not supported on this MCU"
#   endif
    if (port == 3) {
        return &uarts[index];
    }
    index++;
#endif
#if MYNEWT_VAL(UART_4)
#   if FSL_FEATURE_SOC_LPUART_COUNT < 5
#       error "UART_4 is not supported on this MCU"
#   endif
    if (port == 4) {
        return &uarts[index];
    }
#endif
    return NULL;
};

static LPUART_Type *const s_uartBases[] = LPUART_BASE_PTRS;
static uint8_t const s_uartIRQ[] = LPUART_RX_TX_IRQS;
static PORT_Type *const s_uartPort[] = NXP_UART_PORTS;
static clock_ip_name_t const s_uartPortClocks[] = NXP_UART_PORT_CLOCKS;
static uint8_t const s_uartPIN_RX[] = NXP_UART_PIN_RX;
static uint8_t const s_uartPIN_TX[] = NXP_UART_PIN_TX;

#if MYNEWT_VAL(UART_0)
static void uart_irq0(void);
#endif
#if MYNEWT_VAL(UART_1)
static void uart_irq1(void);
#endif
#if MYNEWT_VAL(UART_2)
static void uart_irq2(void);
#endif
#if MYNEWT_VAL(UART_3)
static void uart_irq3(void);
#endif
#if MYNEWT_VAL(UART_4)
static void uart_irq4(void);
#endif

static void (*const s_uartirqs[])(void) = {
#if MYNEWT_VAL(UART_0)
    uart_irq0,
#else
    NULL,
#endif
#if MYNEWT_VAL(UART_1)
    uart_irq1,
#else
    NULL,
#endif
#if MYNEWT_VAL(UART_2)
    uart_irq2,
#else
    NULL,
#endif
#if MYNEWT_VAL(UART_3)
    uart_irq3,
#else
    NULL,
#endif
#if MYNEWT_VAL(UART_4)
    uart_irq4,
#else
    NULL,
#endif
};

/*
 * RING BUFFER FUNCTIONS
 */

static uint8_t
ur_is_empty(struct uart_ring *ur)
{
    return (ur->ur_head == ur->ur_tail);
}

static uint8_t
ur_is_full(struct uart_ring *ur)
{
    return (((ur->ur_tail + 1) % ur->ur_size) == ur->ur_head);
}

static void
ur_bump(struct uart_ring *ur)
{
    if (!ur_is_empty(ur)) {
        ur->ur_head++;
        ur->ur_head %= ur->ur_size;
        return;
    }
}

static uint8_t
ur_read(struct uart_ring *ur)
{
    return ur->ur_buf[ur->ur_head];
}

static int
ur_queue(struct uart_ring *ur, uint8_t data)
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

int
hal_uart_init_cbs(int port,
                  hal_uart_tx_char tx_func,
                  hal_uart_tx_done tx_done,
                  hal_uart_rx_char rx_func,
                  void *arg)
{
    struct hal_uart *u;

    u = uart_by_port(port);
    if (!u) {
        return -1;
    }
    u->u_rx_func = rx_func;
    u->u_tx_func = tx_func;
    u->u_tx_done = tx_done;
    u->u_func_arg = arg;

    return 0;
}

void
hal_uart_blocking_tx(int port, uint8_t byte)
{
    struct hal_uart *u;

    u = uart_by_port(port);
    if (!u || !u->u_configured || !u->u_open) {
        return;
    }

    LPUART_WriteBlocking(u->u_base, &byte, 1);
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

void
hal_uart_start_tx(int port)
{
    struct hal_uart *u;
    int data = -1;
    int rc;

    u = uart_by_port(port);
    if (!u || !u->u_configured || !u->u_open) {
        return;
    }

    /* main loop */
    while (true) {
        /* add data to TX ring buffer */
        if (u->u_tx_started == 0) {
            rc = hal_uart_tx_fill_buf(u);
            if (rc > 0) {
                u->u_tx_started = 1;
            }
        }

        /* Send data only when UART TX register is empty and TX ring buffer has data to send out. */
        while (!ur_is_empty(&u->ur_tx) &&
               (kLPUART_TxDataRegEmptyFlag & LPUART_GetStatusFlags(u->u_base))) {
            data = ur_read(&u->ur_tx);
            LPUART_WriteByte(u->u_base, data);
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

    u = uart_by_port(port);
    if (!u || !u->u_configured || !u->u_open) {
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

    u = uart_by_port(port);
    if (u && u->u_configured && u->u_open) {
        status = LPUART_GetStatusFlags(u->u_base);
        /* Check for RX data */
        if (status & (kLPUART_RxDataRegFullFlag | kLPUART_RxOverrunFlag)) {
            data = LPUART_ReadByte(u->u_base);
            if (u->u_rx_stall || u->u_rx_func(u->u_func_arg, data) < 0) {
                /*
                 * RX queue full.
                 */
                u->u_rx_stall = 1;
                ur_queue(&u->ur_rx, data);
            }
        }
        /* Check for TX complete */
        if (kLPUART_TxDataRegEmptyFlag & LPUART_GetStatusFlags(u->u_base)) {
            if (u->u_tx_started) {
                u->u_tx_started = 0;
                if (u->u_tx_done) {
                    u->u_tx_done(u->u_func_arg);
                }
            }
        }
    }
}

#if MYNEWT_VAL(UART_0)
static void
uart_irq0(void)
{
    uart_irq_handler(0);
}
#endif

#if MYNEWT_VAL(UART_1)
static void
uart_irq1(void)
{
    uart_irq_handler(1);
}
#endif

#if MYNEWT_VAL(UART_2)
static void
uart_irq2(void)
{
    uart_irq_handler(2);
}
#endif

#if MYNEWT_VAL(UART_3)
static void
uart_irq3(void)
{
    uart_irq_handler(3);
}
#endif

#if MYNEWT_VAL(UART_4)
static void
uart_irq4(void)
{
    uart_irq_handler(4);
}
#endif

int
hal_uart_config(int port,
                int32_t speed,
                uint8_t databits,
                uint8_t stopbits,
                enum hal_uart_parity parity,
                enum hal_uart_flow_ctl flow_ctl)
{
    struct hal_uart *u;
    lpuart_config_t uconfig;

    u = uart_by_port(port);
    if (!u || !u->u_configured || u->u_open) {
        return -1;
    }

    /* PIN config (all UARTs use kPORT_MuxAlt3) */
    CLOCK_EnableClock(u->p_clock);
    PORT_SetPinMux(u->p_base, u->u_pin_rx, kPORT_MuxAlt3);
    PORT_SetPinMux(u->p_base, u->u_pin_tx, kPORT_MuxAlt3);

    /* UART CONFIG */
    CLOCK_SetLpuartClock(2U);

    LPUART_GetDefaultConfig(&uconfig);
    uconfig.baudRate_Bps = speed;

    switch (databits) {
    case 8:
        uconfig.dataBitsCount = kLPUART_EightDataBits;
        break;
    case 7:
#if FSL_FEATURE_LPUART_HAS_7BIT_DATA_SUPPORT
        uconfig.dataBitsCount = kLPUART_SevenDataBits;
        break;
#endif /* Fallthrought */
    default:
        return -1;
    }

    switch (stopbits) {
    case 1:
        uconfig.stopBitCount = kLPUART_OneStopBit;
        break;
    case 2:
        uconfig.stopBitCount = kLPUART_TwoStopBit;
        break;
    default:
        return -1;
    }

    switch (parity) {
    case HAL_UART_PARITY_NONE:
        uconfig.parityMode = kLPUART_ParityDisabled;
        break;
    case HAL_UART_PARITY_ODD:
        uconfig.parityMode = kLPUART_ParityOdd;
        break;
    case HAL_UART_PARITY_EVEN:
        uconfig.parityMode = kLPUART_ParityEven;
        break;
    }

    if (flow_ctl != HAL_UART_FLOW_CTL_NONE) {
        return -1;
    }

    u->u_open = 1;
    u->u_tx_started = 0;
    uconfig.enableTx = true;
    uconfig.enableRx = true;

    NVIC_SetVector(u->u_irq, (uint32_t)s_uartirqs[port]);

    /* Initialize UART device */
    LPUART_Init(u->u_base, &uconfig, CLOCK_GetFreq(u->clk_src));
    LPUART_EnableInterrupts(u->u_base,
                            kLPUART_RxDataRegFullInterruptEnable |
                            kLPUART_RxOverrunInterruptEnable);
    EnableIRQ(u->u_irq);

    return 0;
}

int
hal_uart_close(int port)
{
    struct hal_uart *u;

    u = uart_by_port(port);
    if (!u || !u->u_open) {
        return -1;
    }

    u->u_open = 0;
    LPUART_DisableInterrupts(u->u_base,
                             kLPUART_RxDataRegFullInterruptEnable |
                             kLPUART_RxOverrunInterruptEnable |
                             kLPUART_TxDataRegEmptyInterruptEnable);
    DisableIRQ(u->u_irq);
    LPUART_EnableTx(u->u_base, false);
    LPUART_EnableRx(u->u_base, false);

    return 0;
}

int
hal_uart_init(int port, void *cfg)
{
    struct hal_uart *u;

    u = uart_by_port(port);
    if (u) {
        u->u_base = s_uartBases[port];
        u->clk_src = kCLOCK_Osc0ErClk;
        u->u_irq = s_uartIRQ[port];
        u->p_base = s_uartPort[port];
        u->p_clock = s_uartPortClocks[port];
        u->u_pin_rx = s_uartPIN_RX[port];
        u->u_pin_tx = s_uartPIN_TX[port];
        u->ur_tx.ur_buf = u->tx_buffer;
        u->ur_tx.ur_size = TX_BUF_SZ;
        u->ur_tx.ur_head = 0;
        u->ur_tx.ur_tail = 0;
        u->ur_rx.ur_buf = u->rx_buffer;
        u->ur_rx.ur_size = RX_BUF_SZ;
        u->ur_rx.ur_head = 0;
        u->ur_rx.ur_tail = 0;
        u->u_configured = 1;
    }

    return 0;
}
