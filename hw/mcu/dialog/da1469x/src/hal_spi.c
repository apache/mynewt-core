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

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <os/mynewt.h>
#include <mcu/da1469x_pd.h>
#include <mcu/da1469x_hal.h>
#include <hal/hal_spi.h>
#include <mcu/mcu.h>
#include "DA1469xAB.h"

/* The maximum number of SPI interfaces we will allow */
#define DA1469X_HAL_SPI_MAX (2)

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_1_MASTER)
#define SPI_MASTER_CODE 1
#else
#define SPI_MASTER_CODE 0
#endif
#if MYNEWT_VAL(SPI_0_SLAVE) || MYNEWT_VAL(SPI_1_SLAVE)
#defien SPI_SLAVE_CODE  1
#else
#define SPI_SLAVE_CODE  0
#endif

#if SPI_MASTER_CODE || SPI_SLAVE_CODE

struct da1469x_hal_spi_controller {
    SPI_Type *regs;
    uint8_t spi_num;
    uint8_t spi_clk_func;
    uint8_t spi_do_func;
    uint8_t spi_di_func;
    uint8_t spi_ss_func;
    uint8_t irq_num;
    void (* irq_handler)(void);
};

struct da1469x_hal_spi {
    uint8_t spi_type;
    struct hal_spi_settings spi_cfg; /* Slave and master */

    /* SPI controller data  */
    const struct da1469x_hal_spi_controller *hw;

    const uint8_t *txbuf;   /* Pointer to TX buffer */
    uint8_t *rxbuf;         /* Pointer to RX buffer */

    uint16_t len;           /* Length of buffer(s) */
    uint16_t txlen;         /* Number of bytes sent so far */
    uint16_t rxlen;         /* Number of bytes received so far */

    /* Callback and arguments */
    hal_spi_txrx_cb txrx_cb_func;
    void           *txrx_cb_arg;
};

void SPI_Handler(void);
void SPI2_Handler(void);

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
static const struct da1469x_hal_spi_controller hal_spi0_controller = {
    .regs = (SPI_Type *)SPI,
    .spi_num = 0,
    .spi_clk_func = MCU_GPIO_FUNC_SPI_CLK,
    .spi_do_func = MCU_GPIO_FUNC_SPI_DO,
    .spi_di_func = MCU_GPIO_FUNC_SPI_DI,
    .spi_ss_func = MCU_GPIO_FUNC_SPI_EN,
    .irq_num = SPI_IRQn,
    .irq_handler = SPI_Handler,
};
struct da1469x_hal_spi hal_spi0 = {
    .hw = &hal_spi0_controller,
};
#endif
#if MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
static const struct da1469x_hal_spi_controller hal_spi1_controller = {
    .regs = (SPI_Type *)SPI2,
    .spi_num = 1,
    .spi_clk_func = MCU_GPIO_FUNC_SPI2_CLK,
    .spi_do_func = MCU_GPIO_FUNC_SPI2_DO,
    .spi_di_func = MCU_GPIO_FUNC_SPI2_DI,
    .spi_ss_func = MCU_GPIO_FUNC_SPI2_EN,
    .irq_num = SPI2_IRQn,
    .irq_handler = SPI2_Handler,
};
struct da1469x_hal_spi hal_spi1 = {
    .hw = &hal_spi1_controller,
};
#endif

static struct da1469x_hal_spi *da1469x_hal_spis[DA1469X_HAL_SPI_MAX] = {
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
    &hal_spi0,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
    &hal_spi1
#else
    NULL
#endif
};

static struct da1469x_hal_spi *
hal_spi_resolve(int spi_num)
{
    if (spi_num >= DA1469X_HAL_SPI_MAX) {
        return NULL;
    }

    return da1469x_hal_spis[spi_num];
}

static bool
da1469x_hal_spi_do_transfer(struct da1469x_hal_spi *spi)
{
    SPI_Type *regs = spi->hw->regs;
    uint32_t ctrl_reg;

    while (spi->rxlen < spi->len || spi->txlen < spi->len) {
        ctrl_reg = regs->SPI_CTRL_REG;
        if (0 == (ctrl_reg & SPI_SPI_CTRL_REG_SPI_RX_FIFO_EMPTY_Msk)) {
            if (spi->rxlen < spi->len) {
                spi->rxbuf[spi->rxlen++] = (uint8_t)regs->SPI_RX_TX_REG;
            }
        } else if (0 != (ctrl_reg & SPI_SPI_CTRL_REG_SPI_TXH_Msk)) {
            if (spi->txlen < spi->len) {
                regs->SPI_RX_TX_REG = spi->txbuf[spi->txlen++];
            }
        } else {
            /* Transfer not finished yet, but there is nothing more to send
             * or TX FIFO is full for now */
            return false;
        }
    }
    return true;
}

static void
da1469x_hal_spi_irq_handler(struct da1469x_hal_spi *spi)
{
    SPI_Type *regs = spi->hw->regs;

    if (!da1469x_hal_spi_do_transfer(spi)) {
        if (spi->txlen >= spi->len) {
            /* No need for interrupt from TX fifo */
            regs->SPI_CTRL_REG &= ~SPI_SPI_CTRL_REG_SPI_TX_FIFO_NOTFULL_MASK_Msk;
        }
        regs->SPI_CLEAR_INT_REG = 1;
    } else {
        regs->SPI_CTRL_REG &= ~(SPI_SPI_CTRL_REG_SPI_TX_FIFO_NOTFULL_MASK_Msk |
                                SPI_SPI_CTRL_REG_SPI_MINT_Msk);
        if (spi->txrx_cb_func) {
            spi->txrx_cb_func(spi->txrx_cb_arg, spi->len);
        }
    }
}

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
void
SPI_Handler(void)
{
    da1469x_hal_spi_irq_handler(&hal_spi0);
}
#endif

#if MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
void
SPI2_Handler(void)
{
    da1469x_hal_spi_irq_handler(&hal_spi1);
}
#endif

static int
hal_spi_init_master(const struct da1469x_hal_spi *spi,
                    const struct da1469x_hal_spi_cfg *cfg)
{
    int irq_num = spi->hw->irq_num;

    /* Configure pins */
    mcu_gpio_set_pin_function(cfg->pin_sck, MCU_GPIO_MODE_OUTPUT,
                              spi->hw->spi_clk_func);
    if (cfg->pin_do >= 0) {
        mcu_gpio_set_pin_function(cfg->pin_do, MCU_GPIO_MODE_OUTPUT,
                                  spi->hw->spi_do_func);
    }
    if (cfg->pin_di >= 0) {
        mcu_gpio_set_pin_function(cfg->pin_di, MCU_GPIO_MODE_INPUT,
                                  spi->hw->spi_di_func);
    }
    if (cfg->pin_ss >= 0) {
        mcu_gpio_set_pin_function(cfg->pin_ss, MCU_GPIO_MODE_INPUT,
                                  spi->hw->spi_ss_func);
    }

    spi->hw->regs->SPI_CLEAR_INT_REG = 0;
    spi->hw->regs->SPI_CTRL_REG = 0;

    if (spi->hw->spi_num == 0 && MYNEWT_VAL(SPI_0_MASTER)) {
        CRG_COM->RESET_CLK_COM_REG = CRG_COM_RESET_CLK_COM_REG_SPI_CLK_SEL_Msk;
        CRG_COM->SET_CLK_COM_REG = CRG_COM_RESET_CLK_COM_REG_SPI_ENABLE_Msk;
    } else if (spi->hw->spi_num == 1 && MYNEWT_VAL(SPI_1_MASTER)) {
        CRG_COM->RESET_CLK_COM_REG = CRG_COM_RESET_CLK_COM_REG_SPI2_CLK_SEL_Msk;
        CRG_COM->SET_CLK_COM_REG = CRG_COM_RESET_CLK_COM_REG_SPI2_ENABLE_Msk;
    }

    NVIC_SetVector(irq_num, (uint32_t)spi->hw->irq_handler);
    NVIC_SetPriority(irq_num, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_ClearPendingIRQ(irq_num);
    NVIC_EnableIRQ(irq_num);

    return 0;
}

static int
hal_spi_init_slave(const struct da1469x_hal_spi *spi,
                   const struct da1469x_hal_spi_cfg *cfg)
{
    return -1;
}

int
hal_spi_init(int spi_num, void *cfg, uint8_t spi_type)
{
    int rc;
    struct da1469x_hal_spi *spi;

    if (cfg == NULL) {
        return SYS_EINVAL;
    }

    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return SYS_EINVAL;
    }

    if ((spi_type != HAL_SPI_TYPE_MASTER) && (spi_type != HAL_SPI_TYPE_SLAVE)) {
        return SYS_EINVAL;
    }

    da1469x_pd_acquire(MCU_PD_DOMAIN_COM);

    spi->spi_type  = spi_type;

    hal_spi_disable(spi_num);

    if (spi_type == HAL_SPI_TYPE_MASTER && SPI_MASTER_CODE) {
        rc = hal_spi_init_master(spi, (struct da1469x_hal_spi_cfg *)cfg);
    } else if (SPI_SLAVE_CODE) {
        rc = hal_spi_init_slave(spi, (struct da1469x_hal_spi_cfg *)cfg);
    } else {
        rc = SYS_EINVAL;
    }

    return rc;
}

int
hal_spi_config(int spi_num, struct hal_spi_settings *settings)
{
    SPI_Type *regs;
    uint32_t ctrl_reg;
    struct da1469x_hal_spi *spi;

    if (settings == NULL) {
        return SYS_EINVAL;
    }

    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return SYS_EINVAL;
    }
    regs = spi->hw->regs;

    regs->SPI_CTRL_REG &= ~SPI_SPI_CTRL_REG_SPI_ON_Msk;
    regs->SPI_CTRL_REG |= SPI_SPI_CTRL_REG_SPI_RST_Msk;

    /* Preserve some register fields only */
    ctrl_reg = regs->SPI_CTRL_REG & (SPI_SPI_CTRL_REG_SPI_TX_FIFO_NOTFULL_MASK_Msk |
                                     SPI_SPI_CTRL_REG_SPI_DMA_TXREQ_MODE_Msk |
                                     SPI_SPI_CTRL_REG_SPI_PRIORITY_Msk |
                                     SPI_SPI_CTRL_REG_SPI_EN_CTRL_Msk |
                                     SPI_SPI_CTRL_REG_SPI_SMN_Msk |
                                     SPI_SPI_CTRL_REG_SPI_DO_Msk |
                                     SPI_SPI_CTRL_REG_SPI_RST_Msk);

    assert(settings->data_order == HAL_SPI_MSB_FIRST);

    switch (settings->baudrate) {
    case 16000:
        ctrl_reg |= (2U << SPI_SPI_CTRL_REG_SPI_CLK_Pos);
        break;
    case 8000:
        ctrl_reg |= (1U << SPI_SPI_CTRL_REG_SPI_CLK_Pos);
        break;
    case 4000:
        ctrl_reg |= (0U << SPI_SPI_CTRL_REG_SPI_CLK_Pos);
        break;
    default:
        /* Slowest possibly clock, divider 14, 2.28MHz */
        ctrl_reg |= (3U << SPI_SPI_CTRL_REG_SPI_CLK_Pos);
        break;
    }

    switch (settings->data_mode) {
    case HAL_SPI_MODE0:
        /* Bits already zeroed */
        break;
    case HAL_SPI_MODE1:
        ctrl_reg |= (1U << SPI_SPI_CTRL_REG_SPI_PHA_Pos);
        break;
    case HAL_SPI_MODE2:
        ctrl_reg |= (1U << SPI_SPI_CTRL_REG_SPI_POL_Pos);
        break;
    case HAL_SPI_MODE3:
        ctrl_reg |= (1U << SPI_SPI_CTRL_REG_SPI_PHA_Pos) |
                    (1U << SPI_SPI_CTRL_REG_SPI_POL_Pos);
        break;
    default:
        assert(0);
        break;
    }
    if (settings->word_size == HAL_SPI_WORD_SIZE_9BIT) {
        ctrl_reg |= (1U << SPI_SPI_CTRL_REG_SPI_WORD_Pos);
    }
    regs->SPI_CTRL_REG = ctrl_reg;
    /* At this point interrupt is cleared, FIFO is enabled, controller is disabled */

    return 0;
}

int
hal_spi_enable(int spi_num)
{
    SPI_Type *regs;
    struct da1469x_hal_spi *spi;

    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return SYS_EINVAL;
    }
    regs = spi->hw->regs;

    if (regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_BUSY_Msk) {
        return SYS_EBUSY;
    }

    regs->SPI_CTRL_REG |= SPI_SPI_CTRL_REG_SPI_ON_Msk;
    regs->SPI_CTRL_REG &= ~SPI_SPI_CTRL_REG_SPI_RST_Msk;

    return 0;
}

int
hal_spi_disable(int spi_num)
{
    SPI_Type *regs;
    struct da1469x_hal_spi *spi;

    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return SYS_EINVAL;
    }
    regs = spi->hw->regs;

    while (regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_BUSY_Msk) {
    }

    regs->SPI_CTRL_REG &= ~(SPI_SPI_CTRL_REG_SPI_ON_Msk |
                            SPI_SPI_CTRL_REG_SPI_INT_BIT_Msk);
    regs->SPI_CTRL_REG |= SPI_SPI_CTRL_REG_SPI_RST_Msk;

    return 0;
}

uint16_t
hal_spi_tx_val(int spi_num, uint16_t val)
{
    SPI_Type *regs;
    struct da1469x_hal_spi *spi;
    uint16_t dummy;
    uint32_t ctrl_reg;
    bool nine_bits = false;

    spi = hal_spi_resolve(spi_num);
    if (!spi || spi->spi_type == HAL_SPI_TYPE_SLAVE) {
        return 0xFFFF;
    }
    regs = spi->hw->regs;

    /* Get rid of old data if any */
    while (!(regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_TX_FIFO_EMPTY_Msk)) {
    }

    while (!(regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_RX_FIFO_EMPTY_Msk)) {
        dummy = regs->SPI_RX_TX_REG;
        (void)dummy;
    }

    ctrl_reg = regs->SPI_CTRL_REG;
    /* 9-bit word */
    nine_bits = (ctrl_reg & SPI_SPI_CTRL_REG_SPI_WORD_Msk) ==
        (3U << SPI_SPI_CTRL_REG_SPI_WORD_Pos);
    if (nine_bits) {
        ctrl_reg &= ~SPI_SPI_CTRL_REG_SPI_9BIT_VAL_Msk;
        ctrl_reg |= (val << (SPI_SPI_CTRL_REG_SPI_9BIT_VAL_Pos - 8) ) &
            SPI_SPI_CTRL_REG_SPI_9BIT_VAL_Msk;
    }
    regs->SPI_RX_TX_REG = (uint8_t)val;
    while (regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_RX_FIFO_EMPTY_Msk) {
    }

    ctrl_reg = regs->SPI_CTRL_REG;
    val = (uint8_t)regs->SPI_RX_TX_REG;
    if (nine_bits) {
        val |= (ctrl_reg & SPI_SPI_CTRL_REG_SPI_9BIT_VAL_Msk) >>
                (SPI_SPI_CTRL_REG_SPI_9BIT_VAL_Pos - 8);
    }

    return val;
}

int
hal_spi_set_txrx_cb(int spi_num, hal_spi_txrx_cb txrx_cb, void *arg)
{
    SPI_Type *regs;
    struct da1469x_hal_spi *spi;
    int rc = 0;

    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        rc = SYS_EINVAL;
        goto err;
    }
    regs = spi->hw->regs;


    if ((regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_ON_Msk) != 0) {
        rc = SYS_EINVAL;
    } else {
        spi->txrx_cb_func = txrx_cb;
        spi->txrx_cb_arg = arg;
    }

err:
    return rc;
}

int
hal_spi_txrx(int spi_num, void *txbuf, void *rxbuf, int len)
{
    SPI_Type *regs;
    struct da1469x_hal_spi *spi;
    int rc = 0;
    const uint8_t *tx;
    uint8_t *rx;
    int rxlen = 0;
    uint8_t val;

    spi = hal_spi_resolve(spi_num);
    if (spi == NULL || txbuf == NULL || spi->spi_type != HAL_SPI_TYPE_MASTER) {
        rc = SYS_EINVAL;
        goto err;
    }
    regs = spi->hw->regs;
    tx = txbuf;
    rx = rxbuf;

    /* Flush RX FIFO */
    while (!(regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_RX_FIFO_EMPTY_Msk)) {
        val = (uint8_t)regs->SPI_RX_TX_REG;
    }

    rxlen = len;

    while (len) {
        if (0 == (regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_TXH_Msk)) {
            regs->SPI_RX_TX_REG = *tx++;
            len--;
        }
        if (0 == (regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_RX_FIFO_EMPTY_Msk)) {
            val = (uint8_t)regs->SPI_RX_TX_REG;
            if (rx) {
                *rx++ = val;
            }
            rxlen--;
        }
    }
    while (rxlen) {
        if (0 == (regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_RX_FIFO_EMPTY_Msk)) {
            val = (uint8_t)regs->SPI_RX_TX_REG;
            if (rx) {
                *rx++ = val;
            }
            rxlen--;
        }
    }
err:
    return rc;
}

int
hal_spi_txrx_noblock(int spi_num, void *txbuf, void *rxbuf, int len)
{
    SPI_Type *regs;
    struct da1469x_hal_spi *spi;
    uint32_t ctrl_reg;
    int rc = 0;

    spi = hal_spi_resolve(spi_num);
    if (spi == NULL) {
        rc = SYS_EINVAL;
        goto err;
    }
    if (SPI_MASTER_CODE && txbuf == NULL && spi->spi_type != HAL_SPI_TYPE_MASTER) {
        rc = SYS_EINVAL;
        goto err;
    } else if (SPI_SLAVE_CODE && txbuf == NULL && rxbuf == NULL &&
               spi->spi_type == HAL_SPI_TYPE_SLAVE) {
        rc = SYS_EINVAL;
        goto err;
    }
    regs = spi->hw->regs;
    spi->txbuf = txbuf;
    spi->rxbuf = rxbuf;
    spi->len = (uint16_t)len;
    spi->txlen = 0;
    spi->rxlen = 0;

    if (da1469x_hal_spi_do_transfer(spi)) {
        if (spi->txrx_cb_func) {
            spi->txrx_cb_func(spi->txrx_cb_arg, spi->len);
        }
    } else {
        ctrl_reg = regs->SPI_CTRL_REG | SPI_SPI_CTRL_REG_SPI_MINT_Msk;
        if (spi->txlen < spi->len) {
            ctrl_reg |= SPI_SPI_CTRL_REG_SPI_TX_FIFO_NOTFULL_MASK_Msk;
        }
        regs->SPI_CTRL_REG = ctrl_reg;
    }
err:
    return rc;
}

int
hal_spi_slave_set_def_tx_val(int spi_num, uint16_t val)
{
    return 0;
}

int
hal_spi_abort(int spi_num)
{
    SPI_Type *regs;
    struct da1469x_hal_spi *spi;
    int rc = 0;

    spi = hal_spi_resolve(spi_num);
    if (spi == NULL) {
        rc = SYS_EINVAL;
        goto err;
    }
    regs = spi->hw->regs;
    regs->SPI_CTRL_REG &= ~(SPI_SPI_CTRL_REG_SPI_MINT_Msk |
        SPI_SPI_CTRL_REG_SPI_TX_FIFO_NOTFULL_MASK_Msk);

    spi->len = 0;
    spi->txlen = 0;
    spi->rxlen = 0;

err:
    return rc;
}

int
hal_spi_init_hw(uint8_t spi_num, uint8_t spi_type,
                const struct hal_spi_hw_settings *cfg)
{
    struct da1469x_hal_spi_cfg hal_cfg;

    hal_cfg.pin_sck = (uint8_t)cfg->pin_sck;
    if (spi_type == HAL_SPI_TYPE_MASTER) {
        hal_cfg.pin_do = (uint8_t)cfg->pin_mosi;
        hal_cfg.pin_di = (uint8_t)cfg->pin_miso;
    } else {
        hal_cfg.pin_di = (uint8_t)cfg->pin_mosi;
        hal_cfg.pin_do = (uint8_t)cfg->pin_miso;
    }
    hal_cfg.pin_ss = (uint8_t)cfg->pin_ss;

    return hal_spi_init(spi_num, &hal_cfg, spi_type);
}

#endif
