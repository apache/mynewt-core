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
#include "mcu/mips_hal.h"
#include <assert.h>
#include <stdlib.h>

#include <mips/cpu.h>
#include <mips/hal.h>

#include "mcu/gic.h"

static const uint32_t UART_0_INT_NO = 24;
static const uint32_t UART_1_INT_NO = 25;

/* base values for the below functions */
static uint32_t* const UART_0_BASE = (uint32_t* const)0xb8101400;
static uint32_t* const UART_1_BASE = (uint32_t* const)0xb8101500;
static const uint32_t UART_CLOCK_FREQ = 1843200;

enum e_uart_regs {
    CR_UART_RBR_THR_DLL = 0,
    CR_UART_IER_DLH,
    CR_UART_IIR_FCR,
    CR_UART_LCR,
    CR_UART_MCR,
    CR_UART_LSR,
    CR_UART_MSR,
    CR_UART_SCRATCH,
    CR_UART_SOFT_RESET,
    CR_UART_ACC_BUF_STATUS
};

enum e_uart_lsr_bits {
    CR_UART_LSR_DR = 1,
    CR_UART_LSR_OE = 1 << 1,
    CR_UART_LSR_PE = 1 << 2,
    CR_UART_LSR_FE = 1 << 3,
    CR_UART_LSR_BI = 1 << 4,
    CR_UART_LSR_THRE = 1 << 5,
    CR_UART_LSR_TEMT = 1 << 6,
    CR_UART_LSR_RXFER = 1 << 7
};

enum e_uart_lcr_bits {
    CR_UART_LCR_WLS = 3,
    CR_UART_LCR_STOPB = 1 << 2,
    CR_UART_LCR_PEN = 1 << 3,
    CR_UART_LCR_EPS = 1 << 4,
    CR_UART_LCR_STPR = 1 << 5,
    CR_UART_LCR_BREAK = 1 << 6,
    CR_UART_LCR_DLAB = 1 << 7
};

static inline uint32_t*
uart_reg(int port, enum e_uart_regs offset)
{
    switch (port) {
        case 0:
        return UART_0_BASE + offset;
        case 1:
        return UART_1_BASE + offset;
    }
    return 0;
}

static inline uint32_t
uart_reg_read(int port, enum e_uart_regs offset)
{
    return *uart_reg(port, offset);
}

static inline void
uart_reg_write(int port, enum e_uart_regs offset, uint32_t data)
{
    *uart_reg(port, offset) = data;
}

struct hal_uart {
    volatile uint8_t u_rx_stall:1;
    volatile uint8_t u_rx_data;
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
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
uart_irq_handler(int port)
{
    uint8_t lsr = uart_reg_read(port, CR_UART_LSR);
    if (lsr & CR_UART_LSR_RXFER) {
        /* receive error */
        if (lsr & CR_UART_LSR_BI) {
            /* break */
        }
        if (lsr & CR_UART_LSR_FE) {
            /* framing error */
        }
        if (lsr & CR_UART_LSR_PE) {
            /* parity */
        }
    }
    if (lsr & CR_UART_LSR_DR) {
        /* data ready */
        uarts[port].u_rx_data = uart_reg_read(port, CR_UART_RBR_THR_DLL);
        int c = uarts[port].u_rx_func(uarts[port].u_func_arg,
                                        uarts[port].u_rx_data);
        if (c < 0) {
            /* disable rx interrupt */
            uart_reg_write(port, CR_UART_IER_DLH, uart_reg_read(1,
                                                    CR_UART_IER_DLH) & ~1);
            uarts[port].u_rx_stall = 1;
        }
    }
    if (lsr & CR_UART_LSR_THRE) {
        /* transmit holding reg empty */
        int c = uarts[port].u_tx_func(uarts[port].u_func_arg);
        if (c < 0) {
            /* disable tx interrupt */
            uart_reg_write(port, CR_UART_IER_DLH, uart_reg_read(1,
                                                    CR_UART_IER_DLH) & ~2);
            /* call tx done cb */
            if (uarts[port].u_tx_done) {
                uarts[port].u_tx_done(uarts[port].u_func_arg);
            }
        } else {
            /* write char out */
            uart_reg_write(port, CR_UART_RBR_THR_DLL, (uint32_t)c & 0xff);
        }
    }
}

void __attribute__((interrupt, keep_interrupts_masked))
_mips_isr_hw0(void)
{
    uart_irq_handler(0);
}

void __attribute__((interrupt, keep_interrupts_masked))
_mips_isr_hw1(void)
{
    uart_irq_handler(1);
}

void
hal_uart_start_rx(int port)
{
    if (uarts[port].u_rx_stall) {
        /* recover saved data */
        reg_t sr;
        __HAL_DISABLE_INTERRUPTS(sr);
        int c = uarts[port].u_rx_func(uarts[port].u_func_arg,
                                        uarts[port].u_rx_data);
        if (c >= 0) {
            uarts[port].u_rx_stall = 0;
            /* enable rx interrupt */
            uart_reg_write(port, CR_UART_IER_DLH, 1);
        }
        __HAL_ENABLE_INTERRUPTS(sr);
    }
}

void
hal_uart_start_tx(int port)
{
    uart_reg_write(port, CR_UART_IER_DLH, uart_reg_read(port,
        CR_UART_IER_DLH) | 2);
}

void
hal_uart_blocking_tx(int port, uint8_t data)
{
    /* wait for transmit holding register to be empty */
    while(!(uart_reg_read(port, CR_UART_LSR) & CR_UART_LSR_THRE)) {
    }
    /* write to transmit holding register */
    uart_reg_write(port, CR_UART_RBR_THR_DLL, data);
    /* wait for transmit holding register to be empty */
    while(!(uart_reg_read(port, CR_UART_LSR) & CR_UART_LSR_THRE)) {
    }
}

int
hal_uart_init(int port, void *arg)
{
    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    /* XXX: flow control currently unsupported */
    (void) flow_ctl;
    uarts[port].u_rx_stall = 0;

    /* work out divisor */
    uint16_t divisor = (UART_CLOCK_FREQ + (baudrate << 3)) / (baudrate << 4);
    /* write to divisor regs */
    uart_reg_write(port, CR_UART_LCR, CR_UART_LCR_DLAB);
    uart_reg_write(port, CR_UART_RBR_THR_DLL, divisor & 0xff);
    uart_reg_write(port, CR_UART_IER_DLH, divisor >> 8);
    /* write to config regs */

    if ((databits < 5) || (databits > 8) || (stopbits < 1) || (stopbits > 2)) {
        return -1;
    }
    uint8_t value = ((databits - 5) & 3) | ((stopbits << 1) & (1 << 2));
    switch (parity) {
    case HAL_UART_PARITY_NONE:
        break;
    case HAL_UART_PARITY_ODD:
        value |= 1 << 3;
        break;
    case HAL_UART_PARITY_EVEN:
        value |= (1 << 3) | (1 << 4);
        break;
    default:
        return -1;
    }
    uart_reg_write(port, CR_UART_LCR, value);
    uart_reg_write(port, CR_UART_MCR, 0);

    /* init gic, there shouldn't be any harm in calling this multiple times */
    if (gic_init() != 0) {
        return -1;
    }

    switch(port) {
    case 0:
        /* map UART0 to HW interrupt 0 */
        gic_map(UART_0_INT_NO, 0, port);
        gic_interrupt_active_high(UART_0_INT_NO);
        /* enable UART0 interrupt */
        gic_interrupt_set(UART_0_INT_NO);
        break;
    case 1:
        /* map UART1 to HW interrupt 1 */
        gic_map(UART_1_INT_NO, 0, port);
        gic_interrupt_active_high(UART_1_INT_NO);
        /* enable UART1 interrupt */
        gic_interrupt_set(UART_1_INT_NO);
        break;
    default:
        return -1;
    }

    /* enable rx interrupt */
    uart_reg_write(port, CR_UART_IER_DLH, 1);
    return 0;
}

int
hal_uart_close(int port)
{
    /* disable gic interrupts */
    switch(port) {
    case 0:
        /* unmap UART0 from HW interrupt 0 */
        gic_unmap(UART_0_INT_NO, port);
        /* disable UART0 interrupt */
        gic_interrupt_reset(UART_0_INT_NO);
        break;
    case 1:
        /* unmap UART1 from HW interrupt 1 */
        gic_unmap(UART_1_INT_NO, port);
        /* disable UART1 interrupt */
        gic_interrupt_reset(UART_1_INT_NO);
        break;
    default:
        return -1;
    }

    /* disable uart interrupts */
    uart_reg_write(port, CR_UART_IER_DLH, 0);
    uart_reg_write(port, CR_UART_MCR, 0);
    return 0;
}
