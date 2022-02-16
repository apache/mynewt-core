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
#include <stdbool.h>
#include <stdint.h>
#include "syscfg/syscfg.h"
#include "mcu/da1469x_pd.h"
#include "mcu/da1469x_hal.h"
#include "mcu/mcu.h"
#include "hal/hal_uart.h"
#include "hal/hal_gpio.h"
#include "defs/error.h"
#include "os/os_trace_api.h"
#include "os/util.h"

#define DA1469X_UART_COUNT      3

struct da1469x_uart {
    /* Use UART2 as common type since UART does not have flow control */
    UART2_Type *regs;
    IRQn_Type irqn;

    volatile uint8_t rx_stalled : 1;
    volatile uint8_t tx_started : 1;
    /* RX temporarily switched to GPIO with interrupt */
    volatile uint8_t rx_disconnected : 1;
    volatile uint8_t rx_data;

    /*
     * Stores rx pin func and pointer to cfg. Needed to enable/disable pullup
     * when the uart is opened/closed
     */
    mcu_gpio_func rx_pin_func;
    struct da1469x_uart_cfg *cfg;

    hal_uart_rx_char rx_func;
    hal_uart_tx_char tx_func;
    hal_uart_tx_done tx_done;
    void *func_arg;
};

#if MYNEWT_VAL(UART_0)
static struct da1469x_uart da1469x_uart_0;
#endif
#if MYNEWT_VAL(UART_1)
static struct da1469x_uart da1469x_uart_1;
#endif
#if MYNEWT_VAL(UART_2)
static struct da1469x_uart da1469x_uart_2;
#endif

static struct da1469x_uart * const da1469x_uarts[DA1469X_UART_COUNT] = {
#if MYNEWT_VAL(UART_0)
    &da1469x_uart_0,
#else
    NULL,
#endif
#if MYNEWT_VAL(UART_1)
    &da1469x_uart_1,
#else
    NULL,
#endif
#if MYNEWT_VAL(UART_2)
    &da1469x_uart_2,
#else
    NULL,
#endif
};

struct da1469x_uart_baudrate {
    uint32_t baudrate;
    uint32_t cfg; /* DLH=cfg[23:16] DLL=cfg[15:8] DLF=cfg[7:0] */
};

static const struct da1469x_uart_baudrate da1469x_uart_baudrates[] = {
#if MYNEWT_VAL_CHOICE(MCU_SYSCLK_SOURCE, PLL96)
#if MYNEWT_VAL(UART_1) || MYNEWT_VAL(UART_2)
    { 6000000, 0x00000100 },
    { 3000000, 0x00000200 },
#endif
#endif
    { 2000000, 0x00000100 },
    { 1000000, 0x00000200 },
    {  921600, 0x00000203 },
    {  500000, 0x00000400 },
    {  230400, 0x0000080b },
    {  115200, 0x00001106 },
    {   57600, 0x0000220c },
    {   38400, 0x00003401 },
    {   28800, 0x00004507 },
    {   19200, 0x00006803 },
    {   14400, 0x00008a0e },
    {    9600, 0x0000d005 },
    {    4800, 0x0001a00b },
};

static inline struct da1469x_uart *
da1469x_uart_resolve(unsigned uart_num)
{
    if (uart_num >= DA1469X_UART_COUNT) {
        return NULL;
    }

    return da1469x_uarts[uart_num];
}

static inline uint32_t
da1469x_uart_find_baudrate_cfg(uint32_t baudrate)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(da1469x_uart_baudrates); i++) {
        if (da1469x_uart_baudrates[i].baudrate == baudrate) {
            return da1469x_uart_baudrates[i].cfg;
        }
    }

    return 0;
}

static inline void
da1469x_uart_tx_intr_enable(struct da1469x_uart *uart)
{
    uart->regs->UART2_IER_DLH_REG |= UART2_UART2_IER_DLH_REG_PTIME_DLH7_Msk |
                                     UART2_UART2_IER_DLH_REG_ETBEI_DLH1_Msk;
}

static inline void
da1469x_uart_tx_intr_disable(struct da1469x_uart *uart)
{
    uart->regs->UART2_IER_DLH_REG &= ~(UART2_UART2_IER_DLH_REG_PTIME_DLH7_Msk |
                                       UART2_UART2_IER_DLH_REG_ETBEI_DLH1_Msk);
}

static inline void
da1469x_uart_tx_pthre_intr_disable(struct da1469x_uart *uart)
{
    uart->regs->UART2_IER_DLH_REG &= ~UART2_UART2_IER_DLH_REG_PTIME_DLH7_Msk;
}

static inline void
da1469x_uart_rx_intr_enable(struct da1469x_uart *uart)
{
    uart->regs->UART2_IER_DLH_REG |= UART2_UART2_IER_DLH_REG_ERBFI_DLH0_Msk;
}

static inline void
da1469x_uart_rx_intr_disable(struct da1469x_uart *uart)
{
    uart->regs->UART2_IER_DLH_REG &= ~UART2_UART2_IER_DLH_REG_ERBFI_DLH0_Msk;
}

static void
da1469x_uart_set_rx_pin(struct da1469x_uart *uart)
{
    int mode = uart->cfg->rx_pullup ? MCU_GPIO_MODE_INPUT_PULLUP : MCU_GPIO_MODE_INPUT;

    if (uart->cfg->pin_rx >= 0) {
        mcu_gpio_set_pin_function(uart->cfg->pin_rx, mode, uart->rx_pin_func);
    }
}

static void
da1469x_uart_set_rx_pin_as_gpio(struct da1469x_uart *uart)
{
    int mode = uart->cfg->rx_pullup ? MCU_GPIO_MODE_INPUT_PULLUP : MCU_GPIO_MODE_INPUT;

    if (uart->cfg->pin_rx >= 0) {
        mcu_gpio_set_pin_function(uart->cfg->pin_rx, mode, MCU_GPIO_FUNC_GPIO);
    }
}

#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1) || MYNEWT_VAL(UART_2)
static void
da1469x_uart_isr_thr_empty(struct da1469x_uart *uart)
{
    UART2_Type *regs;
    int ch;

    regs = uart->regs;

    if (!uart->tx_started) {
        da1469x_uart_tx_intr_disable(uart);
        if (uart->tx_done) {
            uart->tx_done(uart->func_arg);
        }
        return;
    }

    while (regs->UART2_USR_REG & UART2_UART2_USR_REG_UART_TFNF_Msk) {
        ch = uart->tx_func(uart->func_arg);
        if (ch < 0) {
            da1469x_uart_tx_pthre_intr_disable(uart);
            uart->tx_started = 0;
            break;
        }

        regs->UART2_RBR_THR_DLL_REG = ch;
    }
}

static void
da1469x_uart_isr_recv_data(struct da1469x_uart *uart)
{
    UART2_Type *regs;
    int ch;

    regs = uart->regs;

    uart->rx_data = regs->UART2_RBR_THR_DLL_REG;

    ch = uart->rx_func(uart->func_arg, uart->rx_data);
    if (ch < 0) {
        da1469x_uart_rx_intr_disable(uart);
        uart->rx_stalled = 1;
    }

}

static void
da1469x_uart_reconnect_rx(void *arg)
{
    struct da1469x_uart *uart = (struct da1469x_uart *)arg;

    /* RX pin high, reconfigure RX pin for UART */
    if (hal_gpio_read(uart->cfg->pin_rx) == 1) {
        hal_gpio_irq_release(uart->cfg->pin_rx);
        uart->rx_disconnected = 0;
        da1469x_uart_set_rx_pin(uart);
    }
}

void
da1469x_uart_busy(struct da1469x_uart *uart)
{
    UART2_Type *regs = uart->regs;
    int8_t pin_rx = uart->cfg->pin_rx;
    hal_gpio_pull_t mode = uart->cfg->rx_pullup ? HAL_GPIO_PULL_UP : HAL_GPIO_PULL_NONE;

    (void)regs->UART2_USR_REG;
    (void)regs->UART2_LSR_REG;
    /* Busy due to low RX */
    if (MYNEWT_VAL(MCU_UART_DICONNECT_RX_ON_BUSY) && hal_gpio_read(pin_rx) == 0) {
        hal_gpio_irq_init(pin_rx, da1469x_uart_reconnect_rx, uart,
                          HAL_GPIO_TRIG_RISING, mode);
        uart->rx_disconnected = 1;
        hal_gpio_irq_enable(pin_rx);
        return;
    }
    assert(0);
}

static void
da1469x_uart_common_isr(struct da1469x_uart *uart)
{
    UART2_Type *regs;
    bool no_intr = false;

    os_trace_isr_enter();

    regs = uart->regs;

    while (!no_intr) {
        /*
         * XXX should be UART2_UART2_IIR_FCR_REG_IIR_FCR_Msk below but this is
         *     defined (incorrectly) as 0xFF so incorrect value is read.
         */
        switch (regs->UART2_IIR_FCR_REG & 0x0F) {
        case 0x01: /* no interrupt pending */
            no_intr = true;
            break;
        case 0x02: /* THR empty */
            da1469x_uart_isr_thr_empty(uart);
            break;
        case 0x04: /* received data avaialable */
            da1469x_uart_isr_recv_data(uart);
            break;
        case 0x06: /* receiver line status */
            break;
        case 0x07: /* busy detect */
            da1469x_uart_busy(uart);
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
#endif

#if MYNEWT_VAL(UART_0)
static void
da1469x_uart_isr(void)
{
    da1469x_uart_common_isr(&da1469x_uart_0);
}
#endif

#if MYNEWT_VAL(UART_1)
static void
da1469x_uart2_isr(void)
{
    da1469x_uart_common_isr(&da1469x_uart_1);
}
#endif

#if MYNEWT_VAL(UART_2)
static void
da1469x_uart3_isr(void)
{
    da1469x_uart_common_isr(&da1469x_uart_2);
}
#endif

void
hal_uart_start_rx(int port)
{
    struct da1469x_uart *uart;
    uint32_t primask;
    int ch;

    uart = da1469x_uart_resolve(port);
    if (!uart) {
        return;
    }

    if (uart->cfg->pin_rx < 0) {
        return;
    }

    __HAL_DISABLE_INTERRUPTS(primask);

    if (uart->rx_stalled) {
        ch = uart->rx_func(uart->func_arg, uart->rx_data);
        if (ch >= 0) {
            uart->rx_stalled = 0;
            da1469x_uart_rx_intr_enable(uart);
        }
    }

    __HAL_ENABLE_INTERRUPTS(primask);
}

void
hal_uart_start_tx(int port)
{
    struct da1469x_uart *uart;
    uint32_t primask;

    uart = da1469x_uart_resolve(port);
    if (!uart) {
        return;
    }

    __HAL_DISABLE_INTERRUPTS(primask);

    if (uart->tx_started == 0) {
        uart->tx_started = 1;
        da1469x_uart_tx_intr_enable(uart);
    }

    __HAL_ENABLE_INTERRUPTS(primask);
}

void
hal_uart_blocking_tx(int port, uint8_t data)
{
    struct da1469x_uart *uart;
    UART2_Type *regs;

    uart = da1469x_uart_resolve(port);
    if (!uart) {
        return;
    }

    regs = uart->regs;

    while (!(regs->UART2_USR_REG & UART2_UART2_USR_REG_UART_TFNF_Msk)) {
        /* Wait until FIFO has free space */
    }

    regs->UART2_RBR_THR_DLL_REG = data;

    while (!(regs->UART2_USR_REG & UART2_UART2_USR_REG_UART_TFE_Msk) ||
            (regs->UART2_USR_REG & UART2_UART2_USR_REG_UART_BUSY_Msk)) {
        /* Wait until FIFO is empty and UART finished tx */
    }
}

int
hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
                  hal_uart_rx_char rx_func, void *func_arg)
{
    struct da1469x_uart *uart;

    uart = da1469x_uart_resolve(port);
    if (!uart) {
        return SYS_EINVAL;
    }

    uart->rx_func = rx_func;
    uart->tx_func = tx_func;
    uart->tx_done = tx_done;
    uart->func_arg = func_arg;

    return 0;
}

int
hal_uart_init(int port, void *arg)
{
    struct da1469x_uart_cfg *cfg = arg;
    struct da1469x_uart *uart;
    UART2_Type *regs;
    IRQn_Type irqn;
    void (* isr)(void);
    mcu_gpio_func gpiofunc[4]; /* RX, TX, RTS, CTS */

    uart = da1469x_uart_resolve(port);
    if (!uart) {
        return SYS_EINVAL;
    }

    switch (port) {
#if MYNEWT_VAL(UART_0)
    case 0:
        regs = (void *)UART_BASE;
        irqn = UART_IRQn;
        isr = da1469x_uart_isr;
        gpiofunc[0] = MCU_GPIO_FUNC_UART_TX;
        gpiofunc[1] = MCU_GPIO_FUNC_UART_RX;
        gpiofunc[2] = 0;
        gpiofunc[3] = 0;
        break;
#endif
#if MYNEWT_VAL(UART_1)
    case 1:
        regs = (void *)UART2_BASE;
        irqn = UART2_IRQn;
        isr = da1469x_uart2_isr;
        gpiofunc[0] = MCU_GPIO_FUNC_UART2_TX;
        gpiofunc[1] = MCU_GPIO_FUNC_UART2_RX;
        gpiofunc[2] = MCU_GPIO_FUNC_UART2_RTSN;
        gpiofunc[3] = MCU_GPIO_FUNC_UART2_CTSN;
        break;
#endif
#if MYNEWT_VAL(UART_2)
    case 2:
        regs = (void *)UART3_BASE;
        irqn = UART3_IRQn;
        isr = da1469x_uart3_isr;
        gpiofunc[0] = MCU_GPIO_FUNC_UART3_TX;
        gpiofunc[1] = MCU_GPIO_FUNC_UART3_RX;
        gpiofunc[2] = MCU_GPIO_FUNC_UART3_RTSN;
        gpiofunc[3] = MCU_GPIO_FUNC_UART3_CTSN;
        break;
#endif
    default:
        return SYS_EINVAL;
    }

    if (((cfg->pin_rts >= 0) && (gpiofunc[2] == 0)) ||
        ((cfg->pin_cts >= 0) && (gpiofunc[3] == 0))) {
        return SYS_ENOTSUP;
    }

    uart->regs = regs;
    uart->irqn = irqn;

    /* These are for uart open/close to enable a pullup on the rx line */
    uart->rx_pin_func = gpiofunc[1];
    uart->cfg = cfg;

    mcu_gpio_set_pin_function(cfg->pin_tx, MCU_GPIO_MODE_OUTPUT, gpiofunc[0]);

    da1469x_uart_set_rx_pin(uart);

    if (cfg->pin_rts >= 0) {
        mcu_gpio_set_pin_function(cfg->pin_rts, MCU_GPIO_MODE_OUTPUT, gpiofunc[2]);
    }
    if (cfg->pin_cts >= 0) {
        mcu_gpio_set_pin_function(cfg->pin_cts, MCU_GPIO_MODE_INPUT, gpiofunc[3]);
    }

    NVIC_DisableIRQ(irqn);
    NVIC_SetPriority(irqn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetVector(irqn, (uint32_t)isr);

    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
                enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct da1469x_uart *uart;
    UART2_Type *regs;
    uint32_t reg;
    uint32_t baudrate_cfg;

    uart = da1469x_uart_resolve(port);
    if (!uart) {
        return SYS_EINVAL;
    }

    regs = uart->regs;

    if ((databits < 5) || (databits > 8) || (stopbits < 1) || (stopbits > 2)) {
        return SYS_EINVAL;
    }

    /* Check baudrate */
    baudrate_cfg = da1469x_uart_find_baudrate_cfg(baudrate);
    if (!baudrate_cfg || ((port == 0) && (baudrate > 2000000))) {
        return SYS_ENOTSUP;
    }

    switch (port) {
    case 0:
        CRG_COM->SET_CLK_COM_REG = CRG_COM_SET_CLK_COM_REG_UART_ENABLE_Msk;
        break;
    case 1:
        CRG_COM->SET_CLK_COM_REG = CRG_COM_SET_CLK_COM_REG_UART2_ENABLE_Msk;
        if (baudrate <= 2000000) {
            /* Use DIVN as clock source */
            CRG_COM->RESET_CLK_COM_REG = CRG_COM_SET_CLK_COM_REG_UART2_CLK_SEL_Msk;
        } else {
            /* Use DIV1 as clock source */
            CRG_COM->SET_CLK_COM_REG = CRG_COM_SET_CLK_COM_REG_UART2_CLK_SEL_Msk;
        }
        break;
    case 2:
        CRG_COM->SET_CLK_COM_REG = CRG_COM_SET_CLK_COM_REG_UART3_ENABLE_Msk;
        if (baudrate <= 2000000) {
            /* Use DIVN as clock source */
            CRG_COM->RESET_CLK_COM_REG = CRG_COM_SET_CLK_COM_REG_UART3_CLK_SEL_Msk;
        } else {
            /* Use DIV1 as clock source */
            CRG_COM->SET_CLK_COM_REG = CRG_COM_SET_CLK_COM_REG_UART3_CLK_SEL_Msk;
        }
        break;
    default:
        assert(0);
        break;
    }

    if (uart->cfg->pin_rx >= 0) {
        /*
         * Switch to GPIO during configuration to prevent landing up in
         * busy state if RX line is low.
         */
        da1469x_uart_set_rx_pin_as_gpio(uart);
    }
    /* Reset UART before setting up new baudrate */
    regs->UART2_SRR_REG = UART2_UART2_SRR_REG_UART_UR_Msk |
                          UART2_UART2_SRR_REG_UART_RFR_Msk |
                          UART2_UART2_SRR_REG_UART_XFR_Msk;

    regs->UART2_LCR_REG |= UART2_UART2_LCR_REG_UART_DLAB_Msk;
    regs->UART2_IER_DLH_REG = (baudrate_cfg >> 16) & 0xff;
    regs->UART2_RBR_THR_DLL_REG = (baudrate_cfg >> 8) & 0xff;
    regs->UART2_DLF_REG = baudrate_cfg & 0xff;
    regs->UART2_LCR_REG &= ~UART2_UART2_LCR_REG_UART_DLAB_Msk;

    /* Configure frame */
    reg = 0;
    switch (parity) {
    case HAL_UART_PARITY_NONE:
        break;
    case HAL_UART_PARITY_EVEN:
        reg |= UART2_UART2_LCR_REG_UART_EPS_Msk;
        /* no break */
    case HAL_UART_PARITY_ODD:
        reg |= UART2_UART2_LCR_REG_UART_PEN_Msk;
        break;
    default:
        return SYS_EINVAL;
    }
    reg |= (stopbits - 1) << UART2_UART2_LCR_REG_UART_STOP_Pos;
    reg |= (databits - 5) << UART2_UART2_LCR_REG_UART_DLS_Pos;
    regs->UART2_LCR_REG = reg;

    /* Enable hardware FIFO */
    regs->UART2_SFE_REG = UART2_UART2_SFE_REG_UART_SHADOW_FIFO_ENABLE_Msk;
    regs->UART2_SRT_REG = 0;
    regs->UART2_STET_REG = 3;

    /* Enable flow-control if requested and supported */
    if (flow_ctl == HAL_UART_FLOW_CTL_RTS_CTS) {
#if MYNEWT_VAL(UART_0)
        if (uart == &da1469x_uart_0) {
            return SYS_ENOTSUP;
        }
#endif

        regs->UART2_MCR_REG |= UART2_UART2_MCR_REG_UART_AFCE_Msk |
                               UART2_UART2_MCR_REG_UART_RTS_Msk;
    }

    uart->rx_stalled = 0;
    uart->tx_started = 0;

    /* Setup interrupt */
    NVIC_DisableIRQ(uart->irqn);
    NVIC_ClearPendingIRQ(uart->irqn);
    NVIC_EnableIRQ(uart->irqn);

    /*
     * We can acquire PD_COM here to be sure it's acquired only if everything is
     * set properly. It's ok to setup UART without acquiring that domain earlier
     * explicitly because hal_uart_config shall only be called after hal_uart_init
     * and the latter configures GPIOs. Since at least one GPIO is configured, we
     * can assume PD_COM is enabled while in active mode.
     */
    da1469x_pd_acquire(MCU_PD_DOMAIN_COM);

    /* Reconnect RX pin to UART and enable RX interrupt. */
    if (uart->cfg->pin_rx >= 0) {
        da1469x_uart_rx_intr_enable(uart);
        da1469x_uart_set_rx_pin(uart);
    }

    return 0;
}

int
hal_uart_close(int port)
{
    struct da1469x_uart *uart;
    int sr;

    uart = da1469x_uart_resolve(port);
    if (!uart) {
        return SYS_EINVAL;
    }

    da1469x_uart_tx_intr_disable(uart);
    da1469x_uart_rx_intr_disable(uart);

    switch (port) {
    case 0:
        CRG_COM->RESET_CLK_COM_REG = CRG_COM_SET_CLK_COM_REG_UART_ENABLE_Msk;
        break;
    case 1:
        CRG_COM->RESET_CLK_COM_REG = CRG_COM_SET_CLK_COM_REG_UART2_ENABLE_Msk;
        break;
    case 2:
        CRG_COM->RESET_CLK_COM_REG = CRG_COM_SET_CLK_COM_REG_UART3_ENABLE_Msk;
        break;
    default:
        assert(0);
        break;
    }

    /*
     * If RX pin was changed to GPIO due busy state, change it back to UART RX
     * releasing interrupt that was setup.
     */
    if (uart->cfg->pin_rx >= 0) {
        OS_ENTER_CRITICAL(sr);
        if (uart->rx_disconnected) {
            hal_gpio_irq_release(uart->cfg->pin_rx);
            uart->rx_disconnected = 0;
            da1469x_uart_set_rx_pin(uart);
        }
        OS_EXIT_CRITICAL(sr);
    }

    da1469x_pd_release(MCU_PD_DOMAIN_COM);

    return 0;
}
