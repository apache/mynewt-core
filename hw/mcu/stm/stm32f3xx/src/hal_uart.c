/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hal/hal_uart.h"
#include "hal/hal_gpio.h"
#include "bsp/cmsis_nvic.h"
#include "bsp/bsp.h"
#include "mcu/stm32f30x.h"
#include "mcu/stm32f30x_usart.h"
#include "mcu/stm32f30x_rcc.h"
#include "mcu/stm32f3_bsp.h"
#include <assert.h>
#include <stdlib.h>

struct uart {
    USART_TypeDef *u_regs;
    uint8_t u_open:1;
    uint8_t u_rx_stall:1;
    uint8_t u_tx_end:1;
    uint8_t u_rx_data;
    uart_rx_char u_rx_func;
    uart_tx_char u_tx_func;
    uart_tx_done u_tx_done;
    void *u_func_arg;
};
static struct uart uarts[UART_CNT];

struct uart_irq {
    struct uart *ui_uart;
    volatile uint32_t ui_cnt;
};
static struct uart_irq uart_irqs[3];

int
uart_init_cbs(int port, uart_tx_char tx_func, uart_tx_done tx_done,
  uart_rx_char rx_func, void *arg)
{
    struct uart *u;

    if (port >= UART_CNT) {
        return -1;
    }
    u = &uarts[port];
    if (u->u_open) {
        return -1;
    }
    u->u_rx_func = rx_func;
    u->u_tx_func = tx_func;
    u->u_tx_done = tx_done;
    u->u_func_arg = arg;
    return 0;
}

static void
uart_irq_handler(int num)
{
    struct uart_irq *ui;
    struct uart *u;
    USART_TypeDef *regs;
    uint32_t isr;
    int data;
    int rc;

    ui = &uart_irqs[num];
    ++ui->ui_cnt;
    u = ui->ui_uart;
    regs = u->u_regs;

    isr = regs->ISR;
    if (isr & USART_ISR_RXNE) {
        data = regs->RDR;
        rc = u->u_rx_func(u->u_func_arg, data);
        if (rc < 0) {
            regs->CR1 &= ~USART_CR1_RXNEIE;
            u->u_rx_data = data;
            u->u_rx_stall = 1;
        }
    }
    if (isr & USART_ISR_TXE) {
        data = u->u_tx_func(u->u_func_arg);
        if (data < 0) {
            regs->CR1 &= ~USART_CR1_TXEIE;
            regs->CR1 |= USART_CR1_TCIE;
            u->u_tx_end = 1;
        } else {
            regs->TDR = data;
        }
    }
    if (u->u_tx_end == 1 && isr & USART_ISR_TC) {
        if (u->u_tx_done) {
            u->u_tx_done(u->u_func_arg);
        }
        u->u_tx_end = 0;
        regs->CR1 &= ~USART_CR1_TCIE;
    }
}

void
uart_start_rx(int port)
{
    struct uart *u;
    int sr;
    int rc;

    u = &uarts[port];
    if (u->u_rx_stall) {
        __HAL_DISABLE_INTERRUPTS(sr);
        rc = u->u_rx_func(u->u_func_arg, u->u_rx_data);
        if (rc == 0) {
            u->u_rx_stall = 0;
            u->u_regs->CR1 |= USART_CR1_RXNEIE;
        }
        __HAL_ENABLE_INTERRUPTS(sr);
    }
}

void
uart_start_tx(int port)
{
    struct uart *u;
    int sr;

    u = &uarts[port];
    __HAL_DISABLE_INTERRUPTS(sr);
    u->u_regs->CR1 &= ~USART_CR1_TCIE;
    u->u_regs->CR1 |= USART_CR1_TXEIE;
    u->u_tx_end = 0;
    __HAL_ENABLE_INTERRUPTS(sr);
}

static void
uart_irq1(void)
{
    uart_irq_handler(0);
}

static void
uart_irq2(void)
{
    uart_irq_handler(1);

}

static void
uart_irq3(void)
{
    uart_irq_handler(2);
}

static void
hal_uart_set_nvic(IRQn_Type irqn, struct uart *uart)
{
    uint32_t isr;
    struct uart_irq *ui = NULL;

    switch (irqn) {
    case USART1_IRQn:
        isr = (uint32_t)&uart_irq1;
        ui = &uart_irqs[0];
        break;
    case USART2_IRQn:
        isr = (uint32_t)&uart_irq2;
        ui = &uart_irqs[1];
        break;
    case USART3_IRQn:
        isr = (uint32_t)&uart_irq3;
        ui = &uart_irqs[2];
        break;
    default:
        assert(0);
        break;
    }
/*
  XXX need somehow to detect where these exist or not
    case UART4_IRQn:
    case UART5_IRQn:
*/
    if (ui) {
        ui->ui_uart = uart;

        NVIC_SetVector(irqn, isr);
        NVIC_EnableIRQ(irqn);
    }
}

int
uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum uart_parity parity, enum uart_flow_ctl flow_ctl)
{
    struct uart *u;
    const struct stm32f3_uart_cfg *cfg;
    USART_InitTypeDef uart;

    if (port >= UART_CNT) {
        return -1;
    }

    u = &uarts[port];
    if (u->u_open) {
        return -1;
    }
    cfg = bsp_uart_config(port);
    assert(cfg);

    /*
     * RCC
     * pin config
     * UART config
     * nvic config
     * enable uart
     */
    uart.USART_BaudRate = baudrate;
    switch (databits) {
    case 8:
        uart.USART_WordLength = USART_WordLength_8b;
        break;
    case 9:
        uart.USART_WordLength = USART_WordLength_9b;
        break;
    default:
        assert(0);
        return -1;
    }

    switch (stopbits) {
    case 1:
        uart.USART_StopBits = USART_StopBits_1;
        break;
    case 2:
        uart.USART_StopBits = USART_StopBits_2;
        break;
    default:
        return -1;
    }

    switch (parity) {
    case UART_PARITY_NONE:
        uart.USART_Parity = USART_Parity_No;
        break;
    case UART_PARITY_ODD:
        uart.USART_Parity = USART_Parity_Odd;
        break;
    case UART_PARITY_EVEN:
        uart.USART_Parity = USART_Parity_Even;
        break;
    }

    uart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    switch (flow_ctl) {
    case UART_FLOW_CTL_NONE:
        uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
        break;
    case UART_FLOW_CTL_RTS_CTS:
        uart.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS_CTS;
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
    cfg->suc_rcc_cmd(cfg->suc_rcc_dev, ENABLE);

    hal_gpio_init_af(cfg->suc_pin_tx, cfg->suc_pin_af, 0);
    hal_gpio_init_af(cfg->suc_pin_rx, cfg->suc_pin_af, 0);
    if (flow_ctl == UART_FLOW_CTL_RTS_CTS) {
        hal_gpio_init_af(cfg->suc_pin_rts, cfg->suc_pin_af, 0);
        hal_gpio_init_af(cfg->suc_pin_cts, cfg->suc_pin_af, 0);
    }

    USART_Init(cfg->suc_uart, &uart);

    u->u_regs = cfg->suc_uart;

    (void)u->u_regs->RDR;
    (void)u->u_regs->ISR;
    hal_uart_set_nvic(cfg->suc_irqn, u);

    u->u_regs->CR1 |= USART_CR1_RXNEIE;

    USART_Cmd(u->u_regs, ENABLE);
    u->u_open = 1;

    return 0;
}
