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
#include "hal/hal_gpio.h"
#include "mcu/cmsis_nvic.h"
#include "mcu/stm32_hal.h"
#include "bsp/bsp.h"
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
    const struct stm32_uart_cfg *u_cfg;
};
static struct hal_uart uarts[UART_CNT];

struct hal_uart_irq {
    struct hal_uart *ui_uart;
    volatile uint32_t ui_cnt;
};

#if defined(UART8_BASE)
static struct hal_uart_irq uart_irqs[8];
#elif defined(UART7_BASE)
static struct hal_uart_irq uart_irqs[7];
#elif defined(USART6_BASE)
static struct hal_uart_irq uart_irqs[6];
#elif defined(UART5_BASE)
static struct hal_uart_irq uart_irqs[5];
#elif defined(UART4_BASE)
static struct hal_uart_irq uart_irqs[4];
#else
static struct hal_uart_irq uart_irqs[3];
#endif

#if defined(STM32F3) || defined(STM32F7)
#  define STATUS(x)     ((x)->ISR)
#  define RXNE          USART_ISR_RXNE
#  define TXE           USART_ISR_TXE
#  define TC            USART_ISR_TC
#  define RXDR(x)       ((x)->RDR)
#  define TXDR(x)       ((x)->TDR)
#  define BAUD(x,y)     UART_DIV_SAMPLING16((x), (y))
#else
#  define STATUS(x)     ((x)->SR)
#  define RXNE          USART_SR_RXNE
#  define TXE           USART_SR_TXE
#  define TC            USART_SR_TC
#  define RXDR(x)       ((x)->DR)
#  define TXDR(x)       ((x)->DR)
#  define BAUD(x,y)     UART_BRR_SAMPLING16((x), (y))
#endif

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

    isr = STATUS(regs);
    if (isr & RXNE) {
        data = RXDR(regs);
        rc = u->u_rx_func(u->u_func_arg, data);
        if (rc < 0) {
            regs->CR1 &= ~USART_CR1_RXNEIE;
            u->u_rx_data = data;
            u->u_rx_stall = 1;
        }
    }
    if (isr & (TXE | TC)) {
        cr1 = regs->CR1;
        if (isr & TXE) {
            data = u->u_tx_func(u->u_func_arg);
            if (data < 0) {
                cr1 &= ~USART_CR1_TXEIE;
                cr1 |= USART_CR1_TCIE;
                u->u_tx_end = 1;
            } else {
                TXDR(regs) = data;
            }
        }
        if (u->u_tx_end == 1 && isr & TC) {
            if (u->u_tx_done) {
                u->u_tx_done(u->u_func_arg);
            }
            u->u_tx_end = 0;
            cr1 &= ~USART_CR1_TCIE;
        }
        regs->CR1 = cr1;
    }
#if defined(STM32F3) || defined(STM32F7)
    /* clear overrun */
    if (isr & USART_ISR_ORE) {
        regs->ICR |= USART_ICR_ORECF;
    }
#endif
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

    while (!(STATUS(regs) & TXE));

    TXDR(regs) = data;

    /*
     * Waits for TX to complete.
     */
    while (!(STATUS(regs) & TC));
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

#ifdef USART3_BASE
static void
uart_irq3(void)
{
    uart_irq_handler(2);
}
#endif

#ifdef UART4_BASE
static void
uart_irq4(void)
{
    uart_irq_handler(3);
}
#endif

#ifdef UART5_BASE
static void
uart_irq5(void)
{
    uart_irq_handler(4);
}
#endif

#ifdef USART6_BASE
static void
uart_irq6(void)
{
    uart_irq_handler(5);
}
#endif

#ifdef UART7_BASE
static void
uart_irq7(void)
{
    uart_irq_handler(6);
}
#endif

#ifdef UART8_BASE
static void
uart_irq8(void)
{
    uart_irq_handler(7);
}
#endif

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
#ifdef USART3_BASE
    case USART3_IRQn:
        isr = (uint32_t)&uart_irq3;
        ui = &uart_irqs[2];
        break;
#endif
#ifdef UART4_BASE
    case UART4_IRQn:
        isr = (uint32_t)&uart_irq4;
        ui = &uart_irqs[3];
        break;
#endif
#ifdef UART5_BASE
    case UART5_IRQn:
        isr = (uint32_t)&uart_irq5;
        ui = &uart_irqs[4];
        break;
#endif
#ifdef USART6_BASE
    case USART6_IRQn:
        isr = (uint32_t)&uart_irq6;
        ui = &uart_irqs[5];
        break;
#endif
#ifdef UART7_BASE
    case UART7_IRQn:
        isr = (uint32_t)&uart_irq7;
        ui = &uart_irqs[6];
        break;
#endif
#ifdef UART8_BASE
    case UART8_IRQn:
        isr = (uint32_t)&uart_irq8;
        ui = &uart_irqs[7];
        break;
#endif
    default:
        assert(0);
        break;
    }

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
    const struct stm32_uart_cfg *cfg;
    uint32_t cr1, cr2, cr3;
#if defined(STM32F1)
    GPIO_InitTypeDef gpio;
#endif

    if (port >= UART_CNT) {
        return -1;
    }

    u = &uarts[port];
    if (u->u_open) {
        return -1;
    }
    cfg = u->u_cfg;
    assert(cfg);

#if defined(STM32F1)
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pull = GPIO_PULLUP;
    hal_gpio_init_stm(cfg->suc_pin_tx, &gpio);
    if (flow_ctl == HAL_UART_FLOW_CTL_RTS_CTS) {
        hal_gpio_init_stm(cfg->suc_pin_rts, &gpio);
    }

    gpio.Mode = GPIO_MODE_AF_INPUT;
    hal_gpio_init_stm(cfg->suc_pin_rx, &gpio);
    if (flow_ctl == HAL_UART_FLOW_CTL_RTS_CTS) {
        hal_gpio_init_stm(cfg->suc_pin_cts, &gpio);
    }

    if (cfg->suc_pin_remap_fn) {
        cfg->suc_pin_remap_fn();
    }
#endif

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

    cr1 &= ~(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS | USART_CR1_RE);
#if !defined(STM32F1)
    cr1 &= ~(USART_CR1_OVER8);
#endif
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

#if !defined(STM32F1)
    cr1 |= (UART_MODE_RX | UART_MODE_TX | UART_OVERSAMPLING_16);
#else
    cr1 |= (UART_MODE_TX_RX | UART_OVERSAMPLING_16);
#endif

    *cfg->suc_rcc_reg |= cfg->suc_rcc_dev;

#if !defined(STM32F1)
    hal_gpio_init_af(cfg->suc_pin_tx, cfg->suc_pin_af, 0, 0);
    hal_gpio_init_af(cfg->suc_pin_rx, cfg->suc_pin_af, 0, 0);
    if (flow_ctl == HAL_UART_FLOW_CTL_RTS_CTS) {
        hal_gpio_init_af(cfg->suc_pin_rts, cfg->suc_pin_af, 0, 0);
        hal_gpio_init_af(cfg->suc_pin_cts, cfg->suc_pin_af, 0, 0);
    }
#endif

    u->u_regs = cfg->suc_uart;
    u->u_regs->CR3 = cr3;
    u->u_regs->CR2 = cr2;
    u->u_regs->CR1 = cr1;
#ifdef USART6_BASE
    if (cfg->suc_uart == USART1 || cfg->suc_uart == USART6) {
#else
    if (cfg->suc_uart == USART1) {
#endif
        u->u_regs->BRR = BAUD(HAL_RCC_GetPCLK2Freq(), baudrate);
    } else {
        u->u_regs->BRR = BAUD(HAL_RCC_GetPCLK1Freq(), baudrate);
    }

    (void)RXDR(u->u_regs);
    (void)STATUS(u->u_regs);
    hal_uart_set_nvic(cfg->suc_irqn, u);

    u->u_regs->CR1 |= (USART_CR1_RXNEIE | USART_CR1_UE);
    u->u_open = 1;

    return 0;
}

int
hal_uart_init(int port, void *arg)
{
    struct hal_uart *u;

    if (port >= UART_CNT) {
        return -1;
    }
    u = &uarts[port];
    u->u_cfg = (const struct stm32_uart_cfg *)arg;

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
