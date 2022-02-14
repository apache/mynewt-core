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
#include <string.h>
#include <bsp.h>
#include "uart/uart.h"
#include "mcu/da1469x_pd.h"
#include "mcu/da1469x_hal.h"
#include "hal/hal_gpio.h"
#include "uart_da1469x/uart_da1469x.h"

static void da1469x_uart_uart_isr(void);
static void da1469x_uart_uart2_isr(void);
static void da1469x_uart_uart3_isr(void);

struct da1469x_uart_hw_data {
    UART2_Type *regs;
    IRQn_Type irqn;
    mcu_gpio_func rx_pin_func;
    mcu_gpio_func tx_pin_func;
    mcu_gpio_func rts_pin_func;
    mcu_gpio_func cts_pin_func;
    /* Mask for (RE)SET_CLK_COM_REG clock enable */
    uint8_t clk_com_enable_mask;
    /* Mask for (RE)SET_CLK_COM_REG clock selection */
    uint8_t clk_com_clk_sel_mask;
    void (*isr)(void);
};

static const struct da1469x_uart_hw_data da1469x_uart0_hw = {
    .regs = (UART2_Type *)UART,
    .irqn = UART_IRQn,
    .rx_pin_func = MCU_GPIO_FUNC_UART_RX,
    .tx_pin_func = MCU_GPIO_FUNC_UART_TX,
    .rts_pin_func = MCU_GPIO_FUNC_GPIO,
    .cts_pin_func = MCU_GPIO_FUNC_GPIO,
    .clk_com_enable_mask = CRG_COM_RESET_CLK_COM_REG_UART_ENABLE_Msk,
    .clk_com_clk_sel_mask = 0,
    .isr = da1469x_uart_uart_isr,
};

static const struct da1469x_uart_hw_data da1469x_uart1_hw = {
    .regs = (UART2_Type *)UART2,
    .irqn = UART2_IRQn,
    .rx_pin_func = MCU_GPIO_FUNC_UART2_RX,
    .tx_pin_func = MCU_GPIO_FUNC_UART2_TX,
    .rts_pin_func = MCU_GPIO_FUNC_UART2_RTSN,
    .cts_pin_func = MCU_GPIO_FUNC_UART2_CTSN,
    .clk_com_enable_mask = CRG_COM_RESET_CLK_COM_REG_UART2_ENABLE_Msk,
    .clk_com_clk_sel_mask = CRG_COM_RESET_CLK_COM_REG_UART2_CLK_SEL_Msk,
    .isr = da1469x_uart_uart2_isr,
};

static const struct da1469x_uart_hw_data da1469x_uart2_hw = {
    .regs = (UART2_Type *)UART3,
    .irqn = UART3_IRQn,
    .rx_pin_func = MCU_GPIO_FUNC_UART3_RX,
    .tx_pin_func = MCU_GPIO_FUNC_UART3_TX,
    .rts_pin_func = MCU_GPIO_FUNC_UART3_RTSN,
    .cts_pin_func = MCU_GPIO_FUNC_UART3_CTSN,
    .clk_com_enable_mask = CRG_COM_RESET_CLK_COM_REG_UART3_ENABLE_Msk,
    .clk_com_clk_sel_mask = CRG_COM_RESET_CLK_COM_REG_UART3_CLK_SEL_Msk,
    .isr = da1469x_uart_uart3_isr,
};

static struct da1469x_uart_dev *da1469x_dev0;
static struct da1469x_uart_dev *da1469x_dev1;
static struct da1469x_uart_dev *da1469x_dev2;

static void da1469x_uart_uart_configure(struct da1469x_uart_dev *dev);
static void da1469x_uart_uart_shutdown(struct da1469x_uart_dev *dev);

static void
da1469x_uart_on_wakeup_callout_cb(struct os_event *ev)
{
    struct da1469x_uart_dev *dev = ev->ev_arg;

    da1469x_uart_uart_configure(dev);
}

static void
da1469x_uart_on_wakeup_gpio_cb(void *arg)
{
    struct da1469x_uart_dev *dev = arg;

    hal_gpio_irq_disable(dev->da1469x_cfg.pin_rx);
    hal_gpio_irq_release(dev->da1469x_cfg.pin_rx);

    os_callout_reset(&dev->wakeup_callout,
                     os_time_ms_to_ticks32(MYNEWT_VAL(UART_DA1469X_WAKEUP_DELAY_MS)));
}

static void
da1469x_uart_on_setup_wakeup_cb(struct os_event *ev)
{
    struct da1469x_uart_dev *dev = ev->ev_arg;

    hal_gpio_irq_init(dev->da1469x_cfg.pin_rx, da1469x_uart_on_wakeup_gpio_cb, dev,
                      HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
    hal_gpio_irq_enable(dev->da1469x_cfg.pin_rx);
}

static void
da1469x_uart_setup_wakeup(struct da1469x_uart_dev *dev)
{
    os_eventq_put(os_eventq_dflt_get(), &dev->setup_wakeup_event);
}

static inline void
da1469x_uart_tx_intr_enable(struct da1469x_uart_dev *dev)
{
    dev->hw->regs->UART2_IER_DLH_REG |= UART2_UART2_IER_DLH_REG_PTIME_DLH7_Msk |
                                        UART2_UART2_IER_DLH_REG_ETBEI_DLH1_Msk;
}

static inline void
da1469x_uart_tx_intr_disable(struct da1469x_uart_dev *dev)
{
    dev->hw->regs->UART2_IER_DLH_REG &= ~(UART2_UART2_IER_DLH_REG_PTIME_DLH7_Msk |
                                          UART2_UART2_IER_DLH_REG_ETBEI_DLH1_Msk);
}

static inline void
da1469x_uart_lsr_intr_enable(struct da1469x_uart_dev *dev)
{
    dev->hw->regs->UART2_IER_DLH_REG |= UART2_UART2_IER_DLH_REG_ELSI_DLH2_Msk;
}

static inline void
da1469x_uart_rx_intr_enable(struct da1469x_uart_dev *dev)
{
    dev->hw->regs->UART2_IER_DLH_REG |= UART2_UART2_IER_DLH_REG_ERBFI_DLH0_Msk;
}

static inline void
da1469x_uart_rx_intr_disable(struct da1469x_uart_dev *dev)
{
    dev->hw->regs->UART2_IER_DLH_REG &= ~UART2_UART2_IER_DLH_REG_ERBFI_DLH0_Msk;
}

static void
da1469x_uart_isr_thr_empty(struct da1469x_uart_dev *dev)
{
    struct uart_conf *uc = &dev->uc;
    int ch;

    while (dev->hw->regs->UART2_USR_REG & UART2_UART2_USR_REG_UART_TFNF_Msk) {
        ch = uc->uc_tx_char(uc->uc_cb_arg);
        if (ch < 0) {
            da1469x_uart_tx_intr_disable(dev);
            dev->tx_started = 0;
            if (uc->uc_tx_done) {
                uc->uc_tx_done(uc->uc_cb_arg);
            }
            break;
        }

        dev->hw->regs->UART2_RBR_THR_DLL_REG = ch;
    }
}

static void
da1469x_uart_isr_recv_data(struct da1469x_uart_dev *dev)
{
    int ch;

    dev->rx_data = dev->hw->regs->UART2_RBR_THR_DLL_REG;

    ch = dev->uc.uc_rx_char(dev->uc.uc_cb_arg, dev->rx_data);
    if (ch < 0) {
        da1469x_uart_rx_intr_disable(dev);
        dev->rx_stalled = 1;
    }
}

static void
da1469x_uart_isr(struct da1469x_uart_dev *dev)
{
    UART2_Type *uart = dev->hw->regs;
    bool no_intr = false;

    os_trace_isr_enter();

    while (!no_intr) {
        /*
         * NOTE: should be UART2_UART2_IIR_FCR_REG_IIR_FCR_Msk below but this is
         *     defined (incorrectly) as 0xFF so incorrect value is read.
         */
        switch (uart->UART2_IIR_FCR_REG & 0x0F) {
        case 0x01: /* no interrupt pending */
            no_intr = true;
            break;
        case 0x02: /* THR empty */
            da1469x_uart_isr_thr_empty(dev);
            break;
        case 0x04: /* received data available */
            da1469x_uart_isr_recv_data(dev);
            break;
        case 0x06: /* receiver line status */
            if (uart->UART2_LSR_REG & UART2_UART2_LSR_REG_UART_BI_Msk) {
                da1469x_uart_uart_shutdown(dev);
                da1469x_uart_setup_wakeup(dev);
                no_intr = true;
            }
            break;
        case 0x07: /* busy detect */
            (void)uart->UART2_USR_REG;
            da1469x_uart_uart_shutdown(dev);
            da1469x_uart_setup_wakeup(dev);
            no_intr = true;
            break;
        case 0x0c: /* character timeout */
            break;
        default:
            assert(0);
            break;
        }
    }

    os_trace_isr_exit();
}

static void
da1469x_uart_uart_isr(void)
{
    da1469x_uart_isr(da1469x_dev0);
}

static void
da1469x_uart_uart2_isr(void)
{
    da1469x_uart_isr(da1469x_dev1);
}

static void
da1469x_uart_uart3_isr(void)
{
    da1469x_uart_isr(da1469x_dev2);
}

static void
da1469x_uart_uart_configure(struct da1469x_uart_dev *dev)
{
    struct uart_conf *uc = &dev->uc;
    uint32_t baudrate_cfg;
    uint32_t reg;
    UART2_Type *uart = dev->hw->regs;
    IRQn_Type irqn = dev->hw->irqn;
    uint32_t uart_clock = 32000000;

    da1469x_pd_acquire(MCU_PD_DOMAIN_COM);

    NVIC_DisableIRQ(irqn);

    if (dev->da1469x_cfg.pin_rx >= 0) {
        /* Turn RX pin to GPIO input during configuration to avoid busy state */
        mcu_gpio_set_pin_function(dev->da1469x_cfg.pin_rx, MCU_GPIO_MODE_INPUT,
                                  MCU_GPIO_FUNC_GPIO);
    }

    /* Reset UART */
    CRG_COM->RESET_CLK_COM_REG = dev->hw->clk_com_enable_mask;
    CRG_COM->SET_CLK_COM_REG = dev->hw->clk_com_enable_mask;
    if (uc->uc_speed > 2000000) {
        /* System core clock should be 96 MHz */
        if (!MYNEWT_VAL_CHOICE(MCU_SYSCLK_SOURCE, PLL96)) {
            assert(0);
        }
        uart_clock = 96000000;
        /* Select DIV1 */
        CRG_COM->SET_CLK_COM_REG = dev->hw->clk_com_clk_sel_mask;
    } else {
        /* Select DIVN, always 32MHz */
        CRG_COM->RESET_CLK_COM_REG = dev->hw->clk_com_clk_sel_mask;
    }
    uart->UART2_SRR_REG = UART2_UART2_SRR_REG_UART_UR_Msk |
                          UART2_UART2_SRR_REG_UART_RFR_Msk |
                          UART2_UART2_SRR_REG_UART_XFR_Msk;

    /* Configure baudrate */
    baudrate_cfg = (uart_clock - 1 + (uc->uc_speed / 2)) / uc->uc_speed;

    /* If requested clock exceeds limit, set maximum baudrate */
    if (baudrate_cfg < 16) {
        baudrate_cfg = 16;
    }
    uart->UART2_LCR_REG |= UART2_UART2_LCR_REG_UART_DLAB_Msk;
    uart->UART2_IER_DLH_REG = (baudrate_cfg >> 12) & 0xff;
    uart->UART2_RBR_THR_DLL_REG = (baudrate_cfg >> 4) & 0xff;
    uart->UART2_DLF_REG = baudrate_cfg & 0x0f;
    uart->UART2_LCR_REG &= ~UART2_UART2_LCR_REG_UART_DLAB_Msk;

    /* Configure frame */
    reg = 0;
    switch (uc->uc_parity) {
    case UART_PARITY_NONE:
        break;
    case UART_PARITY_EVEN:
        reg |= UART2_UART2_LCR_REG_UART_EPS_Msk;
        /* no break */
    case UART_PARITY_ODD:
        reg |= UART2_UART2_LCR_REG_UART_PEN_Msk;
        break;
    }
    reg |= (uc->uc_stopbits - 1) << UART2_UART2_LCR_REG_UART_STOP_Pos;
    reg |= (uc->uc_databits - 5) << UART2_UART2_LCR_REG_UART_DLS_Pos;
    uart->UART2_LCR_REG = reg;

    /* Enable flow-control if requested and supported */
    if (uc->uc_flow_ctl == UART_FLOW_CTL_RTS_CTS) {
        uart->UART2_MCR_REG |= UART2_UART2_MCR_REG_UART_AFCE_Msk |
                               UART2_UART2_MCR_REG_UART_RTS_Msk;
    }
    /* Enable hardware FIFO */
    uart->UART2_SFE_REG = UART2_UART2_SFE_REG_UART_SHADOW_FIFO_ENABLE_Msk;
    uart->UART2_SRT_REG = 0;
    uart->UART2_STET_REG = 0;

    dev->active = 1;
    dev->rx_stalled = 0;
    dev->tx_started = 0;

    da1469x_uart_lsr_intr_enable(dev);

    mcu_gpio_set_pin_function(dev->da1469x_cfg.pin_tx, MCU_GPIO_MODE_OUTPUT,
                              dev->hw->tx_pin_func);

    if (dev->da1469x_cfg.pin_rx >= 0) {
        mcu_gpio_set_pin_function(dev->da1469x_cfg.pin_rx,
                                  dev->da1469x_cfg.rx_pullup ? MCU_GPIO_MODE_INPUT_PULLUP : MCU_GPIO_MODE_INPUT,
                                  dev->hw->rx_pin_func);
        da1469x_uart_rx_intr_enable(dev);
    }

    if (dev->da1469x_cfg.pin_rts >= 0) {
        mcu_gpio_set_pin_function(dev->da1469x_cfg.pin_rts, MCU_GPIO_MODE_OUTPUT,
                                  dev->hw->rts_pin_func);
    }

    if (dev->da1469x_cfg.pin_cts >= 0) {
        mcu_gpio_set_pin_function(dev->da1469x_cfg.pin_cts, MCU_GPIO_MODE_INPUT,
                                  dev->hw->cts_pin_func);
    }

    NVIC_ClearPendingIRQ(irqn);
    NVIC_EnableIRQ(irqn);
}

static void
da1469x_uart_uart_shutdown(struct da1469x_uart_dev *dev)
{
    dev->active = 0;

    /*
     * Reset UART before disabling clock to avoid any unexpected behavior after
     * clock is enabled again.
     */
    dev->hw->regs->UART2_SRR_REG = UART2_UART2_SRR_REG_UART_UR_Msk |
                                   UART2_UART2_SRR_REG_UART_RFR_Msk |
                                   UART2_UART2_SRR_REG_UART_XFR_Msk;
    NVIC_DisableIRQ(dev->hw->irqn);
    NVIC_ClearPendingIRQ(dev->hw->irqn);

    CRG_COM->RESET_CLK_COM_REG = dev->hw->clk_com_enable_mask;

    da1469x_pd_release(MCU_PD_DOMAIN_COM);
}

static int
da1469x_uart_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct da1469x_uart_dev *dev = (struct da1469x_uart_dev *)odev;
    struct uart_conf *uc = (struct uart_conf *)arg;
    (void)wait;

    if (!uc) {
        return OS_EINVAL;
    }

    if (odev->od_flags & OS_DEV_F_STATUS_OPEN) {
        return OS_EBUSY;
    }

    dev->uc = *uc;

    if (uc->uc_speed < 1200 ||
        uc->uc_speed > ((dev->hw->regs == (UART2_Type *)UART) ? 2000000 : 6000000)) {
        return OS_INVALID_PARM;
    }

    if ((uc->uc_databits < 5) || (uc->uc_databits > 8) ||
        (uc->uc_stopbits < 1) || (uc->uc_stopbits > 2)) {
        return OS_INVALID_PARM;
    }

    if ((uc->uc_parity != UART_PARITY_NONE) &&
        (uc->uc_parity != UART_PARITY_ODD) &&
        (uc->uc_parity != UART_PARITY_EVEN)) {
        return OS_INVALID_PARM;
    }

    if (uc->uc_flow_ctl == UART_FLOW_CTL_RTS_CTS) {
#if MYNEWT_VAL(UART_0)
        if (dev->hw->regs == (UART2_Type *)UART) {
            return OS_INVALID_PARM;
        }
#endif
        if (dev->da1469x_cfg.pin_rts < 0 || dev->da1469x_cfg.pin_cts < 0) {
            return OS_INVALID_PARM;
        }
    }

    da1469x_uart_uart_configure(dev);

    return OS_OK;
}

static int
da1469x_uart_close(struct os_dev *odev)
{
    struct da1469x_uart_dev *dev = (struct da1469x_uart_dev *)odev;

    da1469x_uart_uart_shutdown(dev);

    return OS_OK;
}

static void
da1469x_uart_drain_tx(struct da1469x_uart_dev *dev)
{
    struct uart_conf *uc = &dev->uc;
    int ch;

    while (1) {
        ch = uc->uc_tx_char(uc->uc_cb_arg);
        if (ch < 0) {
            if (uc->uc_tx_done) {
                uc->uc_tx_done(uc->uc_cb_arg);
            }
            break;
        }
    }
}

static void
da1469x_uart_start_tx(struct uart_dev *udev)
{
    struct da1469x_uart_dev *dev = (struct da1469x_uart_dev *)udev;
    os_sr_t sr;

    if (!dev->active) {
        da1469x_uart_drain_tx(dev);
        return;
    }

    OS_ENTER_CRITICAL(sr);

    if (dev->tx_started == 0) {
        dev->tx_started = 1;
        da1469x_uart_tx_intr_enable(dev);
    }

    OS_EXIT_CRITICAL(sr);
}

static void
da1469x_uart_start_rx(struct uart_dev *udev)
{
    struct da1469x_uart_dev *dev = (struct da1469x_uart_dev *)udev;
    int ch;
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);

    if (dev->rx_stalled) {
        ch = dev->uc.uc_rx_char(dev->uc.uc_cb_arg, dev->rx_data);
        if (ch >= 0) {
            dev->rx_stalled = 0;
            da1469x_uart_rx_intr_enable(dev);
        }
    }

    OS_EXIT_CRITICAL(sr);
}

static void
da1469x_uart_blocking_tx(struct uart_dev *udev, uint8_t byte)
{
    struct da1469x_uart_dev *dev = (struct da1469x_uart_dev *)udev;
    UART2_Type *uart = dev->hw->regs;

    if (!dev->active) {
        return;
    }

    while (!(uart->UART2_USR_REG & UART2_UART2_USR_REG_UART_TFNF_Msk)) {
        /* Wait until FIFO has free space */
    }

    uart->UART2_RBR_THR_DLL_REG = byte;

    while (!(uart->UART2_USR_REG & UART2_UART2_USR_REG_UART_TFE_Msk) ||
            (uart->UART2_USR_REG & UART2_UART2_USR_REG_UART_BUSY_Msk)) {
        /* Wait until FIFO is empty and UART finished tx */
    }
}

static int
da1469x_uart_init(struct os_dev *odev, void *arg)
{
    struct da1469x_uart_dev *dev = (struct da1469x_uart_dev *)odev;
    struct da1469x_uart_cfg *cfg = arg;

    if (cfg->pin_rx >= 0) {
        os_callout_init(&dev->wakeup_callout, os_eventq_dflt_get(),
                        da1469x_uart_on_wakeup_callout_cb, dev);

        dev->setup_wakeup_event.ev_cb = da1469x_uart_on_setup_wakeup_cb;
        dev->setup_wakeup_event.ev_arg = dev;
    }

    OS_DEV_SETHANDLERS(odev, da1469x_uart_open, da1469x_uart_close);

    dev->dev.ud_funcs.uf_start_tx = da1469x_uart_start_tx;
    dev->dev.ud_funcs.uf_start_rx = da1469x_uart_start_rx;
    dev->dev.ud_funcs.uf_blocking_tx = da1469x_uart_blocking_tx;

    dev->da1469x_cfg = *cfg;

    NVIC_DisableIRQ(dev->hw->irqn);
    NVIC_SetPriority(dev->hw->irqn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetVector(dev->hw->irqn, (uint32_t)dev->hw->isr);

    return OS_OK;
}

int
da1469x_uart_dev_create(struct da1469x_uart_dev *dev, const char *name, uint8_t priority,
                        const struct da1469x_uart_cfg *da1469x_cfg)
{
    size_t uart_num_idx;
    int uart_num;

    if (dev == NULL || name == NULL || da1469x_cfg == NULL) {
        return OS_EINVAL;
    }

    /* Get UART number from device name last character. */
    uart_num_idx = strlen(name) - 1;
    uart_num = name[uart_num_idx] - '0';

    if (MYNEWT_VAL(UART_0) && uart_num == 0) {
        dev->hw = &da1469x_uart0_hw;
        assert(da1469x_dev0 == NULL);
        da1469x_dev0 = dev;
    } else if (MYNEWT_VAL(UART_1) && uart_num == 1) {
        dev->hw = &da1469x_uart1_hw;
        assert(da1469x_dev1 == NULL);
        da1469x_dev1 = dev;
    } else if (MYNEWT_VAL(UART_2) && uart_num == 2) {
        dev->hw = &da1469x_uart2_hw;
        assert(da1469x_dev2 == NULL);
        da1469x_dev2 = dev;
    } else {
        return OS_ENOENT;
    }

    return os_dev_create((struct os_dev *)dev, name, OS_DEV_INIT_PRIMARY, priority, da1469x_uart_init,
                         (void *)da1469x_cfg);
}
