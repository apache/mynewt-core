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

#include <assert.h>
#include <stdlib.h>
#include "os/mynewt.h"
#include "hal/hal_uart.h"
#include "bsp/bsp.h"
#include "mcu/mips_hal.h"
#include "mcu/pps.h"

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
    (volatile uint32_t *)_UART5_BASE_ADDRESS,
    (volatile uint32_t *)_UART6_BASE_ADDRESS
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
        IEC3CLR = _IEC3_U1TXIE_MASK;
        break;
    case 1:
        IEC4CLR = _IEC4_U2TXIE_MASK;
        break;
    case 2:
        IEC4CLR = _IEC4_U3TXIE_MASK;
        break;
    case 3:
        IEC5CLR = _IEC5_U4TXIE_MASK;
        break;
    case 4:
        IEC5CLR = _IEC5_U5TXIE_MASK;
        break;
    case 5:
        IEC5CLR = _IEC5_U6TXIE_MASK;
        break;
    }
}

static void
uart_enable_tx_int(int port)
{
    switch (port) {
    case 0:
        IEC3SET = _IEC3_U1TXIE_MASK;
        break;
    case 1:
        IEC4SET = _IEC4_U2TXIE_MASK;
        break;
    case 2:
        IEC4SET = _IEC4_U3TXIE_MASK;
        break;
    case 3:
        IEC5SET = _IEC5_U4TXIE_MASK;
        break;
    case 4:
        IEC5SET = _IEC5_U5TXIE_MASK;
        break;
    case 5:
        IEC5SET = _IEC5_U6TXIE_MASK;
        break;
    }
}

static void
uart_disable_rx_int(int port)
{
    switch (port) {
    case 0:
        IEC3CLR = _IEC3_U1RXIE_MASK;
        break;
    case 1:
        IEC4CLR = _IEC4_U2RXIE_MASK;
        break;
    case 2:
        IEC4CLR = _IEC4_U3RXIE_MASK;
        break;
    case 3:
        IEC5CLR = _IEC5_U4RXIE_MASK;
        break;
    case 4:
        IEC5CLR = _IEC5_U5RXIE_MASK;
        break;
    case 5:
        IEC5CLR = _IEC5_U6RXIE_MASK;
        break;
    }
}

static void
uart_enable_rx_int(int port)
{
    switch (port) {
    case 0:
        IEC3SET = _IEC3_U1RXIE_MASK;
        break;
    case 1:
        IEC4SET = _IEC4_U2RXIE_MASK;
        break;
    case 2:
        IEC4SET = _IEC4_U3RXIE_MASK;
        break;
    case 3:
        IEC5SET = _IEC5_U4RXIE_MASK;
        break;
    case 4:
        IEC5SET = _IEC5_U5RXIE_MASK;
        break;
    case 5:
        IEC5SET = _IEC5_U6RXIE_MASK;
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
    int c;

    while(!(UxSTA(port) & _U1STA_UTXBF_MASK)) {
        c = uarts[port].u_tx_func(uarts[port].u_func_arg);
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
__attribute__((interrupt(IPL1AUTO), vector(_UART1_RX_VECTOR))) uart_1_rx_isr(void)
{
    uart_receive_ready(0);
    IFS3CLR = _IFS3_U1RXIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART1_TX_VECTOR))) uart_1_tx_isr(void)
{
    uart_transmit_ready(0);
    IFS3CLR = _IFS3_U1TXIF_MASK;
}


void
__attribute__((interrupt(IPL1AUTO), vector(_UART2_RX_VECTOR))) uart_2_rx_isr(void)
{
    uart_receive_ready(1);
    IFS4CLR = _IFS4_U2RXIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART2_TX_VECTOR))) uart_2_tx_isr(void)
{
    uart_transmit_ready(1);
    IFS4CLR = _IFS4_U2TXIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART3_RX_VECTOR))) uart_3_rx_isr(void)
{
    uart_receive_ready(2);
    IFS4CLR = _IFS4_U3RXIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART3_TX_VECTOR))) uart_3_tx_isr(void)
{
    uart_transmit_ready(2);
    IFS4CLR = _IFS4_U3TXIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART4_RX_VECTOR))) uart_4_rx_isr(void)
{
    uart_receive_ready(3);
    IFS5CLR = _IFS5_U4RXIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART4_TX_VECTOR))) uart_4_tx_isr(void)
{
    uart_transmit_ready(3);
    IFS5CLR = _IFS5_U4TXIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART5_RX_VECTOR))) uart_5_rx_isr(void)
{
    uart_receive_ready(4);
    IFS5CLR = _IFS5_U5RXIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART5_TX_VECTOR))) uart_5_tx_isr(void)
{
    uart_transmit_ready(4);
    IFS5CLR = _IFS5_U5TXIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART6_RX_VECTOR))) uart_6_rx_isr(void)
{
    uart_receive_ready(5);
    IFS5CLR = _IFS5_U6RXIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_UART6_TX_VECTOR))) uart_6_tx_isr(void)
{
    uart_transmit_ready(5);
    IFS5CLR = _IFS5_U6TXIF_MASK;
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
    /* wait for transmit holding register to be empty */
    while(!(UxSTA(port) & _U1STA_TRMT_MASK)) {
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
        case 4:
            ret += pps_configure_output(uarts[port].u_pins->tx, U5TX_OUT_FUNC);
            ret += pps_configure_input(uarts[port].u_pins->rx, U5RX_IN_FUNC);
            break;
        case 5:
            ret += pps_configure_output(uarts[port].u_pins->tx, U6TX_OUT_FUNC);
            ret += pps_configure_input(uarts[port].u_pins->rx, U6RX_IN_FUNC);
            break;
        default:
            return -1;
        }
        if (ret) {
            return -1;
        }
    }

    uint16_t divisor = peripheral_clk / (4 * baudrate) - 1;

    /* disable */
    UxMODE(port) = 0;
    __asm__("nop");
    UxBRG(port) = divisor;
    UxMODE(port) = mode;
    UxSTA(port) = _U1STA_URXEN_MASK | _U1STA_UTXEN_MASK;

    switch (port) {
    case 0:
        /* clear RX interrupt flag */
        IFS3CLR = _IFS3_U1RXIF_MASK;

        /* enable RX interrupt */
        IEC3SET = _IEC3_U1RXIE_MASK;

        /* set rx interrupt priority */
        IPC28CLR = _IPC28_U1RXIP_MASK;
        IPC28SET = (1 << _IPC28_U1RXIP_POSITION); // priority 1
        /* set rx interrupt subpriority */
        IPC28CLR = _IPC28_U1RXIS_MASK;
        IPC28SET = (0 << _IPC28_U1RXIS_POSITION); // subpriority 0

        /* set tx interrupt priority */
        IPC28CLR = _IPC28_U1TXIP_MASK;
        IPC28SET = (1 << _IPC28_U1TXIP_POSITION); // priority 1
        /* set tx interrupt subpriority */
        IPC28CLR = _IPC28_U1TXIS_MASK;
        IPC28SET = (0 << _IPC28_U1TXIS_POSITION); // subpriority 0
        break;
    case 1:
        /* clear RX interrupt flag */
        IFS4CLR = _IFS4_U2RXIF_MASK;

        /* enable RX interrupt */
        IEC4SET = _IEC4_U2RXIE_MASK;

        /* set rx interrupt priority */
        IPC36CLR = _IPC36_U2RXIP_MASK;
        IPC36SET = (1 << _IPC36_U2RXIP_POSITION); // priority 1
        /* set rx interrupt subpriority */
        IPC36CLR = _IPC36_U2RXIS_MASK;
        IPC36SET = (0 << _IPC36_U2RXIS_POSITION); // subpriority 0

        /* set tx interrupt priority */
        IPC36CLR = _IPC36_U2TXIP_MASK;
        IPC36SET = (1 << _IPC36_U2TXIP_POSITION); // priority 1
        /* set tx interrupt subpriority */
        IPC36CLR = _IPC36_U2TXIS_MASK;
        IPC36SET = (0 << _IPC36_U2TXIS_POSITION); // subpriority 0
        break;
    case 2:
        /* clear RX interrupt flag */
        IFS4CLR = _IFS4_U3RXIF_MASK;

        /* enable RX interrupt */
        IEC4SET = _IEC4_U3RXIE_MASK;

        /* set rx interrupt priority */
        IPC39CLR = _IPC39_U3RXIP_MASK;
        IPC39SET = (1 << _IPC39_U3RXIP_POSITION); // priority 1
        /* set rx interrupt subpriority */
        IPC39CLR = _IPC39_U3RXIS_MASK;
        IPC39SET = (0 << _IPC39_U3RXIS_POSITION); // subpriority 0

        /* set tx interrupt priority */
        IPC39CLR = _IPC39_U3TXIP_MASK;
        IPC39SET = (1 << _IPC39_U3TXIP_POSITION); // priority 1
        /* set tx interrupt subpriority */
        IPC39CLR = _IPC39_U3TXIS_MASK;
        IPC39SET = (0 << _IPC39_U3TXIS_POSITION); // subpriority 0
        break;
    case 3:
        /* clear RX interrupt flag */
        IFS5CLR = _IFS5_U4RXIF_MASK;

        /* enable RX interrupt */
        IEC5SET = _IEC5_U4RXIE_MASK;

        /* set rx interrupt priority */
        IPC42CLR = _IPC42_U4RXIP_MASK;
        IPC42SET = (1 << _IPC42_U4RXIP_POSITION); // priority 1
        /* set rx interrupt subpriority */
        IPC42CLR = _IPC42_U4RXIS_MASK;
        IPC42SET = (0 << _IPC42_U4RXIS_POSITION); // subpriority 0

        /* set tx interrupt priority */
        IPC43CLR = _IPC43_U4TXIP_MASK;
        IPC43SET = (1 << _IPC43_U4TXIP_POSITION); // priority 1
        /* set tx interrupt subpriority */
        IPC43CLR = _IPC43_U4TXIS_MASK;
        IPC43SET = (0 << _IPC43_U4TXIS_POSITION); // subpriority 0
        break;
    case 4:
        /* clear RX interrupt flag */
        IFS5CLR = _IFS5_U5RXIF_MASK;

        /* enable RX interrupt */
        IEC5SET = _IEC5_U5RXIE_MASK;

        /* set rx interrupt priority */
        IPC45CLR = _IPC45_U5RXIP_MASK;
        IPC45SET = (1 << _IPC45_U5RXIP_POSITION); // priority 1
        /* set rx interrupt subpriority */
        IPC45CLR = _IPC45_U5RXIS_MASK;
        IPC45SET = (0 << _IPC45_U5RXIS_POSITION); // subpriority 0

        /* set tx interrupt priority */
        IPC45CLR = _IPC45_U5TXIP_MASK;
        IPC45SET = (1 << _IPC45_U5TXIP_POSITION); // priority 1
        /* set tx interrupt subpriority */
        IPC45CLR = _IPC45_U5TXIS_MASK;
        IPC45SET = (0 << _IPC45_U5TXIS_POSITION); // subpriority 0
        break;
    case 5:
        /* clear RX interrupt flag */
        IFS5CLR = _IFS5_U6RXIF_MASK;

        /* enable RX interrupt */
        IEC5SET = _IEC5_U6RXIE_MASK;

        /* set rx interrupt priority */
        IPC47CLR = _IPC47_U6RXIP_MASK;
        IPC47SET = (1 << _IPC47_U6RXIP_POSITION); // priority 1
        /* set rx interrupt subpriority */
        IPC47CLR = _IPC47_U6RXIS_MASK;
        IPC47SET = (0 << _IPC47_U6RXIS_POSITION); // subpriority 0

        /* set tx interrupt priority */
        IPC47CLR = _IPC47_U6TXIP_MASK;
        IPC47SET = (1 << _IPC47_U6TXIP_POSITION); // priority 1
        /* set tx interrupt subpriority */
        IPC47CLR = _IPC47_U6TXIS_MASK;
        IPC47SET = (0 << _IPC47_U6TXIS_POSITION); // subpriority 0
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
