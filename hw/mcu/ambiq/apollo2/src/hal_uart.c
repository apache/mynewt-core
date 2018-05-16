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
#include <inttypes.h>
#include "os/mynewt.h"
#include "mcu/hal_apollo2.h"
#include "hal/hal_uart.h"
#include "mcu/cmsis_nvic.h"
#include "bsp/bsp.h"

#include "am_mcu_apollo.h"

/* Prevent CMSIS from breaking apollo2 macros. */
#undef UART

/* IRQ handler type */
typedef void apollo2_uart_irqh_t(void);

/*
 * Only one UART on Ambiq Apollo 1
 */
struct apollo2_uart {
    uint8_t u_open:1;
    uint8_t u_rx_stall:1;
    uint8_t u_tx_started:1;
    uint8_t u_rx_buf;
    uint8_t u_tx_buf[1];
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
};
static struct apollo2_uart uarts[UART_CNT];

static inline void
apollo2_uart_enable_tx_irq(void)
{
    AM_REGn(UART, 0, IER) |= (AM_REG_UART_IER_TXIM_M);
}

static inline void
apollo2_uart_disable_tx_irq(void)
{
    AM_REGn(UART, 0, IER) &= ~(AM_REG_UART_IER_TXIM_M);
}

static inline void
apollo2_uart_enable_rx_irq(void)
{
    AM_REGn(UART, 0, IER) |= (AM_REG_UART_IER_RTIM_M |
            AM_REG_UART_IER_RXIM_M);
}

static inline void
apollo2_uart_disable_rx_irq(void)
{
    AM_REGn(UART, 0, IER) &= ~(AM_REG_UART_IER_RTIM_M |
            AM_REG_UART_IER_RXIM_M);
}

int
hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
  hal_uart_rx_char rx_func, void *arg)
{
    struct apollo2_uart *u;

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

void
hal_uart_start_tx(int port)
{
    struct apollo2_uart *u;
    os_sr_t sr;
    int data;

    if (port >= UART_CNT) {
        return;
    }

    u = &uarts[port];
    if (!u->u_open) {
        return;
    }

    OS_ENTER_CRITICAL(sr);
    if (u->u_tx_started == 0) {
        while (1) {
            if (AM_BFRn(UART, 0, FR, TXFF)) {
                u->u_tx_started = 1;
                apollo2_uart_enable_tx_irq();
                break;
            }

            data = u->u_tx_func(u->u_func_arg);
            if (data < 0) {
                if (u->u_tx_done) {
                    u->u_tx_done(u->u_func_arg);
                }
                break;
            }

            AM_REGn(UART, 0, DR) = data;
        }
    }
    OS_EXIT_CRITICAL(sr);
}

void
hal_uart_start_rx(int port)
{
    struct apollo2_uart *u;
    os_sr_t sr;
    int rc;

    if (port >= UART_CNT) {
        return;
    }

    u = &uarts[port];
    if (!u->u_open) {
        return;
    }

    if (u->u_rx_stall) {
        OS_ENTER_CRITICAL(sr);
        rc = u->u_rx_func(u->u_func_arg, u->u_rx_buf);
        if (rc == 0) {
            u->u_rx_stall = 0;
            apollo2_uart_enable_rx_irq();
        }

        OS_EXIT_CRITICAL(sr);
    }
}

void
hal_uart_blocking_tx(int port, uint8_t data)
{
    struct apollo2_uart *u;

    if (port >= UART_CNT) {
        return;
    }

    u = &uarts[port];
    if (!u->u_open) {
        return;
    }

    while (AM_BFRn(UART, 0, FR, TXFF));
    AM_REGn(UART, 0, DR) = data;
}

static void
apollo2_uart_irqh_x(int num)
{
    struct apollo2_uart *u;
    uint32_t status;
    int data;
    int rc;

    os_trace_isr_enter();

    u = &uarts[num];

    status = AM_REGn(UART, 0, IES);
    AM_REGn(UART, 0, IEC) &= ~status;

    if (status & (AM_REG_UART_IES_TXRIS_M)) {
        if (u->u_tx_started) {
            while (1) {
                if (AM_BFRn(UART, 0, FR, TXFF)) {
                    break;
                }

                data = u->u_tx_func(u->u_func_arg);
                if (data < 0) {
                    if (u->u_tx_done) {
                        u->u_tx_done(u->u_func_arg);
                    }
                    apollo2_uart_disable_tx_irq();
                    u->u_tx_started = 0;
                    break;
                }

                AM_REGn(UART, 0, DR) = data;
            }
        }
    }

    if (status & (AM_REG_UART_IES_RXRIS_M | AM_REG_UART_IES_RTRIS_M)) {
        /* Service receive buffer */
        while (!AM_BFRn(UART, 0, FR, RXFE)) {
            u->u_rx_buf = AM_REGn(UART, 0, DR);
            rc = u->u_rx_func(u->u_func_arg, u->u_rx_buf);
            if (rc < 0) {
                u->u_rx_stall = 1;
                break;
            }
        }
    }

    os_trace_isr_exit();
}

#if MYNEWT_VAL(UART_0)
static void apollo2_uart_irqh_0(void) { apollo2_uart_irqh_x(0); }
#endif

#if MYNEWT_VAL(UART_0)
static void apollo2_uart_irqh_1(void) { apollo2_uart_irqh_x(1); }
#endif

static int
apollo2_uart_irq_info(int port, int *out_irqn, apollo2_uart_irqh_t **out_irqh)
{
    apollo2_uart_irqh_t *irqh;
    int irqn;

    switch (port) {
    case 0:
        irqn = UART0_IRQn;
        irqh = apollo2_uart_irqh_0;
        break;

    case 1:
        irqn = UART1_IRQn;
        irqh = apollo2_uart_irqh_1;
        break;

    default:
        return -1;
    }

    if (out_irqn != NULL) {
        *out_irqn = irqn;
    }
    if (out_irqh != NULL) {
        *out_irqh = irqh;
    }
    return 0;
}

static void
apollo2_uart_set_nvic(int port)
{
    apollo2_uart_irqh_t *irqh;
    int irqn;
    int rc;

    rc = apollo2_uart_irq_info(port, &irqn, &irqh);
    assert(rc == 0);

    NVIC_SetVector(irqn, (uint32_t)irqh);
}

int
hal_uart_init(int port, void *arg)
{
    struct apollo2_uart_cfg *cfg;
    uint32_t pincfg;

    cfg = arg;

    if (port >= UART_CNT) {
        return SYS_EINVAL;
    }

    switch (cfg->suc_pin_tx) {
    case 1:
        pincfg = AM_HAL_GPIO_FUNC(2);
        break;

    case 7:
        pincfg = AM_HAL_GPIO_FUNC(5);
        break;

    case 16:
        pincfg = AM_HAL_GPIO_FUNC(6);
        break;

    case 20:
    case 30:
        pincfg = AM_HAL_GPIO_FUNC(4);
        break;

    case 22:
    case 39:
        pincfg = AM_HAL_GPIO_FUNC(0);
        break;

    default:
        return SYS_EINVAL;
    }
    am_hal_gpio_pin_config(cfg->suc_pin_tx, pincfg);

    switch (cfg->suc_pin_rx) {
    case 2:
        pincfg = AM_HAL_GPIO_FUNC(2);
        break;

    case 11:
    case 17:
        pincfg = AM_HAL_GPIO_FUNC(6);
        break;

    case 21:
    case 31:
        pincfg = AM_HAL_GPIO_FUNC(4);
        break;

    case 23:
    case 40:
        pincfg = AM_HAL_GPIO_FUNC(0);
        break;

    default:
        return SYS_EINVAL;
    }
    pincfg |= AM_HAL_PIN_DIR_INPUT;
    am_hal_gpio_pin_config(cfg->suc_pin_rx, pincfg);

    /* RTS pin is optional. */
    if (cfg->suc_pin_rts != 0) {
        switch (cfg->suc_pin_rts) {
        case 3:
            pincfg = AM_HAL_GPIO_FUNC(0);
            break;

        case 5:
        case 37:
            pincfg = AM_HAL_GPIO_FUNC(2);
            break;

        case 13:
        case 35:
            pincfg = AM_HAL_GPIO_FUNC(6);
            break;

        case 41:
            pincfg = AM_HAL_GPIO_FUNC(7);
            break;

        default:
            return SYS_EINVAL;
        }
        am_hal_gpio_pin_config(cfg->suc_pin_rts, pincfg);
    }

    /* CTS pin is optional. */
    if (cfg->suc_pin_cts != 0) {
        switch (cfg->suc_pin_cts) {
        case 4:
            pincfg = AM_HAL_GPIO_FUNC(0);
            break;

        case 6:
        case 38:
            pincfg = AM_HAL_GPIO_FUNC(2);
            break;

        case 12:
        case 36:
            pincfg = AM_HAL_GPIO_FUNC(6);
            break;

        case 29:
            pincfg = AM_HAL_GPIO_FUNC(4);
            break;

        default:
            return SYS_EINVAL;
        }
        pincfg |= AM_HAL_PIN_DIR_INPUT;
        am_hal_gpio_pin_config(cfg->suc_pin_cts, pincfg);
    }

    apollo2_uart_set_nvic(port);
    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct apollo2_uart *u;
    am_hal_uart_config_t uart_cfg;
    int irqn;
    int rc;

    if (port >= UART_CNT) {
        return -1;
    }

    u = &uarts[port];
    if (u->u_open) {
        return -1;
    }

    switch (databits) {
    case 8:
        uart_cfg.ui32DataBits = AM_HAL_UART_DATA_BITS_8;
        break;
    case 7:
        uart_cfg.ui32DataBits = AM_HAL_UART_DATA_BITS_7;
        break;
    case 6:
        uart_cfg.ui32DataBits = AM_HAL_UART_DATA_BITS_6;
        break;
    case 5:
        uart_cfg.ui32DataBits = AM_HAL_UART_DATA_BITS_5;
        break;
    default:
        return -1;
    }

    switch (stopbits) {
    case 2:
        uart_cfg.bTwoStopBits = true;
        break;
    case 1:
    case 0:
        uart_cfg.bTwoStopBits = false;
        break;
    default:
        return -1;
    }

    rc = apollo2_uart_irq_info(port, &irqn, NULL);
    if (rc != 0) {
        return -1;
    }

    switch (parity) {
    case HAL_UART_PARITY_NONE:
        uart_cfg.ui32Parity = AM_HAL_UART_PARITY_NONE;
        break;
    case HAL_UART_PARITY_ODD:
        uart_cfg.ui32Parity = AM_HAL_UART_PARITY_ODD;
    case HAL_UART_PARITY_EVEN:
        uart_cfg.ui32Parity = AM_HAL_UART_PARITY_EVEN;
        break;
    }

    switch (flow_ctl) {
    case HAL_UART_FLOW_CTL_NONE:
        uart_cfg.ui32FlowCtrl = AM_HAL_UART_FLOW_CTRL_NONE;
        break;
    case HAL_UART_FLOW_CTL_RTS_CTS:
        uart_cfg.ui32FlowCtrl = AM_HAL_UART_FLOW_CTRL_RTS_CTS;
        break;
    }

    uart_cfg.ui32BaudRate = baudrate;

    am_hal_uart_pwrctrl_enable(port);
    am_hal_uart_clock_enable(port);

    /* Disable the UART before configuring it */
    am_hal_uart_disable(port);

    am_hal_uart_config(port, &uart_cfg);

    am_hal_uart_fifo_config(port,
              AM_HAL_UART_TX_FIFO_1_2 | AM_HAL_UART_RX_FIFO_1_2);

    NVIC_EnableIRQ(irqn);

    am_hal_uart_enable(port);

    apollo2_uart_enable_rx_irq();

    u->u_rx_stall = 0;
    u->u_tx_started = 0;
    u->u_open = 1;

    return 0;
}

int
hal_uart_close(int port)
{
    struct apollo2_uart *u;

    if (port >= UART_CNT) {
        return -1;
    }

    u = &uarts[port];
    if (!u->u_open) {
        return -1;
    }

    u->u_open = 0;
    am_hal_uart_disable(port);
    am_hal_uart_clock_disable(port);
    am_hal_uart_pwrctrl_disable(port);
    return 0;
}
