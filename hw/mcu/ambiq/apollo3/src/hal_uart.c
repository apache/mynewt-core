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
#include "mcu/hal_apollo3.h"
#include "hal/hal_uart.h"
#include "mcu/cmsis_nvic.h"
#include "bsp/bsp.h"

#include "am_mcu_apollo.h"

/* Prevent CMSIS from breaking apollo3 macros. */
#undef UART

static uint8_t g_tx_buffer[256];
static uint8_t g_rx_buffer[2];
const am_hal_uart_config_t g_sUartConfig =
{
    /* Standard UART settings: 115200-8-N-1 */
    .ui32BaudRate = 115200,
    .ui32DataBits = AM_HAL_UART_DATA_BITS_8,
    .ui32Parity = AM_HAL_UART_PARITY_NONE,
    .ui32StopBits = AM_HAL_UART_ONE_STOP_BIT,
    .ui32FlowControl = AM_HAL_UART_FLOW_CTRL_NONE,

    /* Set TX and RX FIFOs to interrupt at half-full. */
    .ui32FifoLevels = (AM_HAL_UART_TX_FIFO_1_2 |
                       AM_HAL_UART_RX_FIFO_1_2),

    /* Buffers */
    .pui8TxBuffer = g_tx_buffer,
    .ui32TxBufferSize = sizeof(g_tx_buffer),
    .pui8RxBuffer = g_rx_buffer,
    .ui32RxBufferSize = sizeof(g_rx_buffer),
};

/* IRQ handler type */
typedef void apollo3_uart_irqh_t(void);

/*
 * 2 UART on Ambiq Apollo 3
 */
struct apollo3_uart {
    uint8_t u_open:1;
    uint8_t u_rx_stall:1;
    uint8_t u_tx_started:1;
    uint8_t u_rx_buf;
    uint8_t u_tx_buf[1];
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
    void *uart_handle;
};
static struct apollo3_uart uarts[UART_CNT];

static inline void
apollo3_uart_enable_tx_irq(void)
{
    UARTn(0)->IER |= (AM_HAL_UART_INT_TX);
}

static inline void
apollo3_uart_disable_tx_irq(void)
{
    UARTn(0)->IER &= ~(AM_HAL_UART_INT_TX);
}

static inline void
apollo3_uart_enable_rx_irq(void)
{
    UARTn(0)->IER |= (AM_HAL_UART_INT_RX |
            AM_HAL_UART_INT_RX_TMOUT);
}

static inline void
apollo3_uart_disable_rx_irq(void)
{
    UARTn(0)->IER &= ~(AM_HAL_UART_INT_RX |
            AM_HAL_UART_INT_RX_TMOUT);
}

int
hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
  hal_uart_rx_char rx_func, void *arg)
{
    struct apollo3_uart *u;

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
    struct apollo3_uart *u;
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
            if (UARTn(0)->FR&UART0_FR_TXFF_Msk) {
                u->u_tx_started = 1;
                apollo3_uart_enable_tx_irq();
                break;
            }

            data = u->u_tx_func(u->u_func_arg);
            if (data < 0) {
                if (u->u_tx_done) {
                    u->u_tx_done(u->u_func_arg);
                }
                break;
            }

            UARTn(0)->DR = data;
        }
    }
    OS_EXIT_CRITICAL(sr);
}

void
hal_uart_start_rx(int port)
{
    struct apollo3_uart *u;
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
            apollo3_uart_enable_rx_irq();
        }

        OS_EXIT_CRITICAL(sr);
    }
}

void
hal_uart_blocking_tx(int port, uint8_t data)
{
    struct apollo3_uart *u;

    if (port >= UART_CNT) {
        return;
    }

    u = &uarts[port];
    if (!u->u_open) {
        return;
    }

    while (UARTn(0)->FR & UART0_FR_TXFF_Msk);
    UARTn(0)->DR = data;
}

static void
apollo3_uart_irqh_x(int num)
{
    struct apollo3_uart *u;
    uint32_t status;
    int data;
    int rc;

    os_trace_isr_enter();

    u = &uarts[num];

    status = UARTn(0)->IES;
    UARTn(0)->IEC &= ~status;

    if (status & (UART0_IES_TXRIS_Msk)) {
        if (u->u_tx_started) {
            while (1) {
                if (UARTn(0)->FR & UART0_FR_TXFF_Msk) {
                    break;
                }

                data = u->u_tx_func(u->u_func_arg);
                if (data < 0) {
                    if (u->u_tx_done) {
                        u->u_tx_done(u->u_func_arg);
                    }
                    apollo3_uart_disable_tx_irq();
                    u->u_tx_started = 0;
                    break;
                }

                UARTn(0)->DR = data;
            }
        }
    }

    if (status & (UART0_IES_RXRIS_Msk | UART0_IES_RTRIS_Msk)) {
        /* Service receive buffer */
        while (!(UARTn(0)->FR & UART0_FR_RXFE_Msk)) {
            u->u_rx_buf = UARTn(0)->DR;
            rc = u->u_rx_func(u->u_func_arg, u->u_rx_buf);
            if (rc < 0) {
                u->u_rx_stall = 1;
                break;
            }
        }
    }

    os_trace_isr_exit();
}

static void apollo3_uart_irqh_0(void) { apollo3_uart_irqh_x(0); }
static void apollo3_uart_irqh_1(void) { apollo3_uart_irqh_x(1); }

static int
apollo3_uart_irq_info(int port, int *out_irqn, apollo3_uart_irqh_t **out_irqh)
{
    apollo3_uart_irqh_t *irqh;
    int irqn;

    switch (port) {
    case 0:
        irqn = UART0_IRQn;
        irqh = apollo3_uart_irqh_0;
        break;

    case 1:
        irqn = UART1_IRQn;
        irqh = apollo3_uart_irqh_1;
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
apollo3_uart_set_nvic(int port)
{
    apollo3_uart_irqh_t *irqh;
    int irqn;
    int rc;

    rc = apollo3_uart_irq_info(port, &irqn, &irqh);
    assert(rc == 0);

    NVIC_SetVector(irqn, (uint32_t)irqh);
}

int
hal_uart_init(int port, void *arg)
{
    struct apollo3_uart_cfg *cfg;
    am_hal_gpio_pincfg_t pincfg;
    am_hal_uart_clock_speed_e uart_clk_speed;

    cfg = arg;

    if (port >= UART_CNT) {
        return SYS_EINVAL;
    }

    am_hal_uart_initialize(port, &(uarts[port].uart_handle));

    am_hal_uart_power_control(uarts[port].uart_handle, AM_HAL_SYSCTRL_WAKE, false);

    uart_clk_speed = eUART_CLK_SPEED_DEFAULT;
    am_hal_uart_control(uarts[port].uart_handle, AM_HAL_UART_CONTROL_CLKSEL, &uart_clk_speed);
    am_hal_uart_configure(uarts[port].uart_handle, &g_sUartConfig);

    switch (port) {
    case 0:
        switch (cfg->suc_pin_tx) {
        case 22:
        case 39:
        case 48:
            pincfg.uFuncSel = 0;
            break;
        case 1:
            pincfg.uFuncSel = 2;
            break;
        case 20:
        case 30:
            pincfg.uFuncSel = 4;
            break;
        case 7:
            pincfg.uFuncSel = 5;
            break;
        case 16:
        case 26:
        case 28:
        case 41:
        case 44:
            pincfg.uFuncSel = 6;
            break;
        default:
            return SYS_EINVAL;
        }
        break;
    case 1:
        switch (cfg->suc_pin_tx) {
        case 10:
        case 24:
        case 42:
            pincfg.uFuncSel = 0;
            break;
        case 39:
            pincfg.uFuncSel = 1;
            break;
        case 14:
        case 35:
            pincfg.uFuncSel = 2;
            break;
        case 20:
        case 37:
            pincfg.uFuncSel = 5;
            break;
        case 8:
        case 18:
        case 46:
            pincfg.uFuncSel = 6;
            break;
        case 12:
            pincfg.uFuncSel = 7;
            break;
        default:
            return SYS_EINVAL;
        }
        break;
    default:
        return SYS_EINVAL;
    }
    pincfg.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA;
    am_hal_gpio_pinconfig(cfg->suc_pin_tx, pincfg);

    switch (port) {
    case 0:
        switch (cfg->suc_pin_rx) {
        case 23:
        case 27:
        case 40:
        case 49:
            pincfg.uFuncSel = 0;
            break;
        case 2:
            pincfg.uFuncSel = 2;
            break;
        case 21:
        case 31:
            pincfg.uFuncSel = 4;
            break;
        case 11:
        case 17:
        case 29:
        case 34:
        case 45:
            pincfg.uFuncSel = 6;
            break;
        default:
            return SYS_EINVAL;
        }
        break;
    case 1:
        switch (cfg->suc_pin_rx) {
        case 2:
        case 25:
        case 43:
            pincfg.uFuncSel = 0;
            break;
        case 40:
            pincfg.uFuncSel = 1;
            break;
        case 15:
        case 36:
            pincfg.uFuncSel = 2;
            break;
        case 4:
        case 21:
            pincfg.uFuncSel = 5;
            break;
        case 9:
        case 19:
        case 38:
        case 47:
            pincfg.uFuncSel = 6;
            break;
        case 13:
            pincfg.uFuncSel = 7;
            break;
        default:
            return SYS_EINVAL;
        }
        break;
    default:
        return SYS_EINVAL;
    }
    am_hal_gpio_pinconfig(cfg->suc_pin_rx, pincfg);

    /* RTS pin is optional. */
    if (cfg->suc_pin_rts >= 0) {
        switch (port) {
            case 0:
                switch (cfg->suc_pin_rts) {
                case 3:
                    pincfg.uFuncSel = 0;
                    break;
                case 5:
                case 37:
                    pincfg.uFuncSel = 2;
                    break;
                case 18:
                    pincfg.uFuncSel = 4;
                    break;
                case 34:
                    pincfg.uFuncSel = 5;
                    break;
                case 13:
                case 35:
                    pincfg.uFuncSel = 6;
                    break;
                case 41:
                    pincfg.uFuncSel = 7;
                    break;
                default:
                    return SYS_EINVAL;
                }
                break;
            case 1:
                switch (cfg->suc_pin_rts) {
                case 44:
                    pincfg.uFuncSel = 0;
                    break;
                case 34:
                    pincfg.uFuncSel = 2;
                    break;
                case 10:
                case 30:
                case 41:
                    pincfg.uFuncSel = 5;
                    break;
                case 16:
                case 20:
                case 31:
                    pincfg.uFuncSel = 7;
                    break;
                default:
                    return SYS_EINVAL;
                }
                break;
            default:
                return SYS_EINVAL;
        }
        pincfg.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA;
        am_hal_gpio_pinconfig(cfg->suc_pin_rts, pincfg);
    }

    /* CTS pin is optional. */
    if (cfg->suc_pin_cts >= 0) {
        switch (port) {
        case 0:
            switch (cfg->suc_pin_cts) {
            case 4:
                pincfg.uFuncSel = 0;
                break;
            case 6:
            case 38:
                pincfg.uFuncSel = 2;
                break;
            case 24:
            case 29:
                pincfg.uFuncSel = 4;
                break;
            case 33:
                pincfg.uFuncSel = 5;
                break;
            case 12:
            case 36:
                pincfg.uFuncSel = 6;
                break;
            default:
                return SYS_EINVAL;
            }
            break;
        case 1:
            switch (cfg->suc_pin_cts) {
              case 45:
                  pincfg.uFuncSel = 0;
                  break;
              case 11:
              case 29:
              case 36:
              case 41:
                  pincfg.uFuncSel = 5;
                  break;
              case 17:
              case 21:
              case 26:
              case 32:
                  pincfg.uFuncSel = 7;
                  break;
              default:
                  return SYS_EINVAL;
            }
            break;
        default:
            return SYS_EINVAL;
        }
        am_hal_gpio_pinconfig(cfg->suc_pin_cts, pincfg);
    }

    apollo3_uart_set_nvic(port);

    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct apollo3_uart *u;
    int irqn;
    int rc;

    am_hal_uart_config_t uart_cfg = {
        /* Set TX and RX FIFOs to interrupt at half-full. */
        .ui32FifoLevels = (AM_HAL_UART_TX_FIFO_1_2 |
                        AM_HAL_UART_RX_FIFO_1_2),

        /* The default interface will just use polling instead of buffers. */
        .pui8TxBuffer = 0,
        .ui32TxBufferSize = 0,
        .pui8RxBuffer = 0,
        .ui32RxBufferSize = 0,
    };

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
        uart_cfg.ui32StopBits = AM_HAL_UART_TWO_STOP_BITS;
        break;
    case 1:
        uart_cfg.ui32StopBits = AM_HAL_UART_ONE_STOP_BIT;
        break;
    default:
        return -1;
    }

    rc = apollo3_uart_irq_info(port, &irqn, NULL);
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
        uart_cfg.ui32FlowControl = AM_HAL_UART_FLOW_CTRL_NONE;
        break;
    case HAL_UART_FLOW_CTL_RTS_CTS:
        uart_cfg.ui32FlowControl = AM_HAL_UART_FLOW_CTRL_RTS_CTS;
        break;
    }

    uart_cfg.ui32BaudRate = baudrate;

    am_hal_uart_configure(uarts[port].uart_handle, &uart_cfg);

    NVIC_EnableIRQ(irqn);

    apollo3_uart_enable_rx_irq();

    u->u_rx_stall = 0;
    u->u_tx_started = 0;
    u->u_open = 1;

    return 0;
}

int
hal_uart_close(int port)
{
    struct apollo3_uart *u;

    if (port >= UART_CNT) {
        return -1;
    }

    u = &uarts[port];
    if (!u->u_open) {
        return -1;
    }

    u->u_open = 0;
    AM_CRITICAL_BEGIN
    UARTn(port)->CR_b.UARTEN = 0;
    UARTn(port)->CR_b.RXE = 0;
    UARTn(port)->CR_b.TXE = 0;
    AM_CRITICAL_END
    UARTn(0)->CR_b.CLKEN = 0;
    am_hal_pwrctrl_periph_disable(port);
    return 0;
}
