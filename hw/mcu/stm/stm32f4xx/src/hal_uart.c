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
#include "hal/hal_gpio.h"
#include "bsp/cmsis_nvic.h"
#include "bsp/bsp.h"
#include "mcu/stm32f4xx.h"
#include "mcu/stm32f4xx_hal_dma.h"
#include "mcu/stm32f4xx_hal_uart.h"
#include "mcu/stm32f4xx_hal_rcc.h"
#include "mcu/stm32f4_bsp.h"
#include <assert.h>
#include <stdlib.h>

struct hal_uart {
    USART_TypeDef *u_regs;
    uint8_t u_open:1;
    uint8_t u_rx_stall:1;
    uint8_t u_tx_end:1;
    uint8_t u_rx_data;
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
};
static struct hal_uart uarts[UART_CNT];

struct hal_uart_irq {
    struct hal_uart *ui_uart;
    volatile uint32_t ui_cnt;
};
static struct hal_uart_irq uart_irqs[6];

int
hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
  hal_uart_rx_char rx_func, void *arg)
{
    struct hal_uart *u;

    u = &uarts[port];
    if (port >= UART_CNT || u->u_open) {
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
    struct hal_uart_irq *ui;
    struct hal_uart *u;
    USART_TypeDef *regs;
    uint32_t isr;
    uint32_t cr1;
    int data;
    int rc;

    ui = &uart_irqs[num];
    ++ui->ui_cnt;
    u = ui->ui_uart;
    regs = u->u_regs;

    isr = regs->SR;
    if (isr & USART_SR_RXNE) {
        data = regs->DR;
        rc = u->u_rx_func(u->u_func_arg, data);
        if (rc < 0) {
            regs->CR1 &= ~USART_CR1_RXNEIE;
            u->u_rx_data = data;
            u->u_rx_stall = 1;
        }
    }
    if (isr & (USART_SR_TXE | USART_SR_TC)) {
        cr1 = regs->CR1;
        if (isr & USART_SR_TXE) {
            data = u->u_tx_func(u->u_func_arg);
            if (data < 0) {
                cr1 &= ~USART_CR1_TXEIE;
                cr1 |= USART_CR1_TCIE;
                u->u_tx_end = 1;
            } else {
                regs->DR = data;
            }
        }
        if (u->u_tx_end == 1 && isr & USART_SR_TC) {
            if (u->u_tx_done) {
                u->u_tx_done(u->u_func_arg);
            }
            u->u_tx_end = 0;
            cr1 &= ~USART_CR1_TCIE;
        }
        regs->CR1 = cr1;
    }
}

void
hal_uart_start_rx(int port)
{
    struct hal_uart *u;
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
hal_uart_start_tx(int port)
{
    struct hal_uart *u;
    int sr;

    u = &uarts[port];
    __HAL_DISABLE_INTERRUPTS(sr);
    u->u_regs->CR1 &= ~USART_CR1_TCIE;
    u->u_regs->CR1 |= USART_CR1_TXEIE;
    u->u_tx_end = 0;
    __HAL_ENABLE_INTERRUPTS(sr);
}

void
hal_uart_blocking_tx(int port, uint8_t data)
{
    struct hal_uart *u;
    USART_TypeDef *regs;

    u = &uarts[port];
    if (port >= UART_CNT || !u->u_open) {
        return;
    }
    regs = u->u_regs;

    while (!(regs->SR & USART_SR_TXE));

    regs->DR = data;

    /*
     * Waits for TX to complete.
     */
    while (!(regs->SR & USART_SR_TC));
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
uart_irq4(void)
{
    uart_irq_handler(3);
}

static void
uart_irq5(void)
{
    uart_irq_handler(4);
}

static void
uart_irq6(void)
{
    uart_irq_handler(5);
}

static void
hal_uart_set_nvic(IRQn_Type irqn, struct hal_uart *uart)
{
    uint32_t isr;
    struct hal_uart_irq *ui = NULL;

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
    case UART4_IRQn:
        isr = (uint32_t)&uart_irq4;
        ui = &uart_irqs[3];
        break;
    case UART5_IRQn:
        isr = (uint32_t)&uart_irq5;
        ui = &uart_irqs[4];
        break;
    case USART6_IRQn:
        isr = (uint32_t)&uart_irq6;
        ui = &uart_irqs[5];
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
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct hal_uart *u;
    const struct stm32f4_uart_cfg *cfg;
    uint32_t cr1, cr2, cr3;

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
    cr1 = cfg->suc_uart->CR1;
    cr2 = cfg->suc_uart->CR2;
    cr3 = cfg->suc_uart->CR3;

    cr1 &= ~(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS | USART_CR1_RE |
      USART_CR1_OVER8);
    cr2 &= ~(USART_CR2_STOP);
    cr3 &= ~(USART_CR3_RTSE | USART_CR3_CTSE);

    switch (databits) {
    case 8:
        cr1 |= UART_WORDLENGTH_8B;
        break;
    case 9:
        cr1 |= UART_WORDLENGTH_9B;
        break;
    default:
        assert(0);
        return -1;
    }

    switch (stopbits) {
    case 1:
        cr2 |= UART_STOPBITS_1;
        break;
    case 2:
        cr2 |= UART_STOPBITS_2;
        break;
    default:
        return -1;
    }

    switch (parity) {
    case HAL_UART_PARITY_NONE:
        cr1 |= UART_PARITY_NONE;
        break;
    case HAL_UART_PARITY_ODD:
        cr1 |= UART_PARITY_ODD;
        break;
    case HAL_UART_PARITY_EVEN:
        cr1 |= UART_PARITY_EVEN;
        break;
    }

    switch (flow_ctl) {
    case HAL_UART_FLOW_CTL_NONE:
        cr3 |= UART_HWCONTROL_NONE;
        break;
    case HAL_UART_FLOW_CTL_RTS_CTS:
        cr3 |= UART_HWCONTROL_RTS_CTS;
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

    cr1 |= (UART_MODE_RX | UART_MODE_TX | UART_OVERSAMPLING_16);

    *cfg->suc_rcc_reg |= cfg->suc_rcc_dev;

    hal_gpio_init_af(cfg->suc_pin_tx, cfg->suc_pin_af, 0);
    hal_gpio_init_af(cfg->suc_pin_rx, cfg->suc_pin_af, 0);
    if (flow_ctl == HAL_UART_FLOW_CTL_RTS_CTS) {
        hal_gpio_init_af(cfg->suc_pin_rts, cfg->suc_pin_af, 0);
        hal_gpio_init_af(cfg->suc_pin_cts, cfg->suc_pin_af, 0);
    }

    u->u_regs = cfg->suc_uart;
    u->u_regs->CR3 = cr3;
    u->u_regs->CR2 = cr2;
    u->u_regs->CR1 = cr1;
    if (cfg->suc_uart == USART1 || cfg->suc_uart == USART6) {
        u->u_regs->BRR = UART_BRR_SAMPLING16(HAL_RCC_GetPCLK2Freq(), baudrate);
    } else {
        u->u_regs->BRR = UART_BRR_SAMPLING16(HAL_RCC_GetPCLK1Freq(), baudrate);
    }

    (void)u->u_regs->DR;
    (void)u->u_regs->SR;
    hal_uart_set_nvic(cfg->suc_irqn, u);

    u->u_regs->CR1 |= (USART_CR1_RXNEIE | USART_CR1_UE);
    u->u_open = 1;

    return 0;
}

int
hal_uart_close(int port)
{
    struct hal_uart *u;

    if (port >= UART_CNT) {
        return -1;
    }
    u = &uarts[port];

    u->u_open = 0;
    u->u_regs->CR1 = 0;

    return 0;
}
