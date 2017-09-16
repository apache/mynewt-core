/**
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
#include "syscfg/syscfg.h"
#include "mcu/mips_hal.h"
#include "mcu/pps.h"
#include <assert.h>
#include <stdlib.h>

#include <xc.h>

#define UxMODE(U)           (base_address[U][0x0 / 0x4])
#define UxMODESET(U)        (base_address[U][0x8 / 0x4])
#define UxSTA(U)            (base_address[U][0x10 / 0x4])
#define UxTXREG(U)          (base_address[U][0x20 / 0x4])
#define UxRXREG(U)          (base_address[U][0x30 / 0x4])
#define UxBRG(U)            (base_address[U][0x40 / 0x4])

static volatile uint32_t* base_address[UART_CNT] = {
    (volatile uint32_t *)_UART1_BASE_ADDRESS,
    (volatile uint32_t *)_UART2_BASE_ADDRESS,
    (volatile uint32_t *)_UART3_BASE_ADDRESS,
    (volatile uint32_t *)_UART4_BASE_ADDRESS,
};

struct hal_uart {
    volatile uint8_t u_rx_stall:1;
    volatile uint8_t u_rx_data;
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
    const struct mips_uart_cfg *u_pins;
};
static struct hal_uart uarts[UART_CNT];

int
hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
  hal_uart_rx_char rx_func, void *arg)
{
    uarts[port].u_rx_func = rx_func;
    uarts[port].u_tx_func = tx_func;
    uarts[port].u_tx_done = tx_done;
    uarts[port].u_func_arg = arg;
    return 0;
}

static void
uart_disable_tx_int(int port)
{
    switch (port) {
    case 0:
        IEC1CLR = _IEC1_U1TXIE_MASK;
        break;
    case 1:
        IEC1CLR = _IEC1_U2TXIE_MASK;
        break;
    case 2:
        IEC2CLR = _IEC2_U3TXIE_MASK;
        break;
    case 3:
        IEC2CLR = _IEC2_U4TXIE_MASK;
        break;
    }
}

static void
uart_enable_tx_int(int port)
{
    switch (port) {
    case 0:
        IEC1SET = _IEC1_U1TXIE_MASK;
        break;
    case 1:
        IEC1SET = _IEC1_U2TXIE_MASK;
        break;
    case 2:
        IEC2SET = _IEC2_U3TXIE_MASK;
        break;
    case 3:
        IEC2SET = _IEC2_U4TXIE_MASK;
        break;
    }
}

static void
uart_disable_rx_int(int port)
{
    switch (port) {
    case 0:
        IEC1CLR = _IEC1_U1RXIE_MASK;
        break;
    case 1:
        IEC1CLR = _IEC1_U2RXIE_MASK;
        break;
    case 2:
        IEC1CLR = _IEC1_U3RXIE_MASK;
        break;
    case 3:
        IEC2CLR = _IEC2_U4RXIE_MASK;
        break;
    }
}

static void
uart_enable_rx_int(int port)
{
    switch (port) {
    case 0:
        IEC1SET = _IEC1_U1RXIE_MASK;
        break;
    case 1:
        IEC1SET = _IEC1_U2RXIE_MASK;
        break;
    case 2:
        IEC1SET = _IEC1_U3RXIE_MASK;
        break;
    case 3:
        IEC2SET = _IEC2_U4RXIE_MASK;
        break;
    }
}

static void
uart_receive_ready(int port)
{
    uarts[port].u_rx_data = UxRXREG(port);

    int c = uarts[port].u_rx_func(uarts[port].u_func_arg,
                                    uarts[port].u_rx_data);
    if (c < 0) {
        uart_disable_rx_int(port);
        uarts[port].u_rx_stall = 1;
    }
}

static void
uart_transmit_ready(int port)
{
    while(!(UxSTA(port) & _U1STA_UTXBF_MASK)) {
        int c = uarts[port].u_tx_func(uarts[port].u_func_arg);
        if (c < 0) {
            uart_disable_tx_int(port);
            /* call tx done cb */
            if (uarts[port].u_tx_done) {
                uarts[port].u_tx_done(uarts[port].u_func_arg);
            }
            break;
        }
        UxTXREG(port) = (uint32_t)c & 0xff;
    }
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART_1_VECTOR))) uart_1_isr(void)
{
    uint32_t sta = U1STA;
    if (sta & _U1STA_URXDA_MASK) {
        uart_receive_ready(0);
        IFS1CLR = _IFS1_U1RXIF_MASK;
    }
    if (sta & _U1STA_TRMT_MASK) {
        uart_transmit_ready(0);
        IFS1CLR = _IFS1_U1TXIF_MASK;
    }
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART_2_VECTOR))) uart_2_isr(void)
{
    uint32_t sta = U2STA;
    if (sta & _U2STA_URXDA_MASK) {
        uart_receive_ready(1);
        IFS1CLR = _IFS1_U2RXIF_MASK;
    }
    if (sta & _U2STA_TRMT_MASK) {
        uart_transmit_ready(1);
        IFS1CLR = _IFS1_U2TXIF_MASK;
    }
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART_3_VECTOR))) uart_3_isr(void)
{
    uint32_t sta = U3STA;
    if (sta & _U3STA_URXDA_MASK) {
        uart_receive_ready(2);
        IFS1CLR = _IFS1_U3RXIF_MASK;
    }
    if (sta & _U3STA_TRMT_MASK) {
        uart_transmit_ready(2);
        IFS2CLR = _IFS2_U3TXIF_MASK;
    }
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART_4_VECTOR))) uart_4_isr(void)
{
    uint32_t sta = U4STA;
    if (sta & _U4STA_URXDA_MASK) {
        uart_receive_ready(3);
        IFS2CLR = _IFS2_U4RXIF_MASK;
    }
    if (sta & _U4STA_TRMT_MASK) {
        uart_transmit_ready(3);
        IFS2CLR = _IFS2_U4TXIF_MASK;
    }
}

void
hal_uart_start_rx(int port)
{
    if (uarts[port].u_rx_stall) {
        /* recover saved data */
        uint32_t sr;
        __HAL_DISABLE_INTERRUPTS(sr);
        int c = uarts[port].u_rx_func(uarts[port].u_func_arg,
                                        uarts[port].u_rx_data);
        if (c >= 0) {
            uarts[port].u_rx_stall = 0;
            /* enable RX interrupt */
            uart_enable_rx_int(port);
        }
        __HAL_ENABLE_INTERRUPTS(sr);
    }
}

void
hal_uart_start_tx(int port)
{
    uart_enable_tx_int(port);
}

void
hal_uart_blocking_tx(int port, uint8_t data)
{
    while (!(UxSTA(port) & _U1STA_TRMT_MASK)) {
    }

    UxTXREG(port) = data;
}

int
hal_uart_init(int port, void *arg)
{
    if (port >= UART_CNT) {
        return -1;
    }

    uarts[port].u_pins = arg;

    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    uint32_t peripheral_clk = MYNEWT_VAL(CLOCK_FREQ) / 2;

    // check input
    if ((databits < 8) || (databits > 9) || (stopbits < 1) || (stopbits > 2)) {
        return -1;
    }

    /* XXX: flow control currently unsupported */
    (void) flow_ctl;
    uarts[port].u_rx_stall = 0;

    uint16_t mode = _U1MODE_BRGH_MASK | (stopbits >> 1);
    switch (parity) {
    case HAL_UART_PARITY_NONE:
        if (databits == 9) {
            mode |= _U1MODE_PDSEL_MASK;
        }
        break;
    case HAL_UART_PARITY_ODD:
        if (databits == 9) { // PIC does not do 9 bit data + parity.
            return -1;
        }
        mode |= _U1MODE_PDSEL1_MASK;
        break;
    case HAL_UART_PARITY_EVEN:
        if (databits == 9) {
           return -1;
        }
        mode |= _U1MODE_PDSEL0_MASK;
        break;
    default:
        return -1;
    }

    /* Configure TX/RX pins */
    if (uarts[port].u_pins) {
        int ret = 0;
        switch(port) {
        case 0:
            ret += pps_configure_output(uarts[port].u_pins->tx, U1TX_OUT_FUNC);
            ret += pps_configure_input(uarts[port].u_pins->rx, U1RX_IN_FUNC);
            break;
        case 1:
            ret += pps_configure_output(uarts[port].u_pins->tx, U2TX_OUT_FUNC);
            ret += pps_configure_input(uarts[port].u_pins->rx, U2RX_IN_FUNC);
            break;
        case 2:
            ret += pps_configure_output(uarts[port].u_pins->tx, U3TX_OUT_FUNC);
            ret += pps_configure_input(uarts[port].u_pins->rx, U3RX_IN_FUNC);
            break;
        case 3:
            ret += pps_configure_output(uarts[port].u_pins->tx, U4TX_OUT_FUNC);
            ret += pps_configure_input(uarts[port].u_pins->rx, U4RX_IN_FUNC);
            break;
        default:
            return -1;
        }
        if (ret) {
            return -1;
        }
    }

    uint16_t divisor = peripheral_clk / (4 * baudrate) - 1;

    UxMODE(port) = 0;
    __asm__("nop");
    UxBRG(port) = divisor;
    UxMODE(port) = mode;
    UxSTA(port) = _U1STA_URXEN_MASK | _U1STA_UTXEN_MASK;

    switch (port) {
    case 0:
        /* clear RX interrupt flag */
        IFS1CLR = _IFS1_U1RXIF_MASK;

        /* enable RX interrupt */
        IEC1SET = _IEC1_U1RXIE_MASK;

        /* set interrupt priority */
        IPC7CLR = _IPC7_U1IP_MASK;
        IPC7SET = (1 << _IPC7_U1IP_POSITION); // priority 1
        /* set interrupt subpriority */
        IPC7CLR = _IPC7_U1IS_MASK;
        IPC7SET = (0 << _IPC7_U1IS_POSITION); // subpriority 0
        break;
    case 1:
        /* clear RX interrupt flag */
        IFS1CLR = _IFS1_U2RXIF_MASK;

        /* enable RX interrupt */
        IEC1SET = _IEC1_U2RXIE_MASK;

        /* set interrupt priority */
        IPC9CLR = _IPC9_U2IP_MASK;
        IPC9SET = (1 << _IPC9_U2IP_POSITION); // priority 1
        /* set interrupt subpriority */
        IPC9CLR = _IPC9_U2IS_MASK;
        IPC9SET = (0 << _IPC9_U2IS_POSITION); // subpriority 0
        break;
    case 2:
        /* clear RX interrupt flag */
        IFS1CLR = _IFS1_U3RXIF_MASK;

        /* enable RX interrupt */
        IEC1SET = _IEC1_U3RXIE_MASK;

        /* set interrupt priority */
        IPC9CLR = _IPC9_U3IP_MASK;
        IPC9SET = (1 << _IPC9_U3IP_POSITION); // priority 1
        /* set interrupt subpriority */
        IPC9CLR = _IPC9_U3IS_MASK;
        IPC9SET = (0 << _IPC9_U3IS_POSITION); // subpriority 0
        break;
    case 3:
        /* clear RX interrupt flag */
        IFS2CLR = _IFS2_U4RXIF_MASK;

        /* enable RX interrupt */
        IEC2SET = _IEC2_U4RXIE_MASK;

        /* set interrupt priority */
        IPC9CLR = _IPC9_U4IP_MASK;
        IPC9SET = (1 << _IPC9_U4IP_POSITION); // priority 1
        /* set interrupt subpriority */
        IPC9CLR = _IPC9_U4IS_MASK;
        IPC9SET = (0 << _IPC9_U4IS_POSITION); // subpriority 0
        break;
    }

    UxMODESET(port) = _U1MODE_ON_MASK;

    return 0;
}

int
hal_uart_close(int port)
{
    if (port >= UART_CNT) {
        return -1;
    }

    UxMODE(port) = 0;
    uart_disable_rx_int(port);

    return 0;
}
