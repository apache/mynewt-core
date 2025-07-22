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
#include <mcu/da1469x_pd.h>
#include "defs/error.h"
#include "hal/hal_gpio.h"
#include "bus/bus.h"
#include "bus/bus_debug.h"
#include "bus/bus_driver.h"
#include "bus/drivers/spi_common.h"
#include "mcu/mcu.h"
#include "mcu/da1469x_dma.h"

#if MYNEWT_VAL(SPI_DA1469X_STAT)
#include "stats/stats.h"
#endif

#if MYNEWT_VAL(SPI_DA1469X_STAT)
STATS_SECT_START(spi_da1469x_stats_section)
    STATS_SECT_ENTRY(read_bytes)
    STATS_SECT_ENTRY(written_bytes)
    STATS_SECT_ENTRY(dma_transferred_bytes)
STATS_SECT_END

STATS_NAME_START(spi_da1469x_stats_section)
    STATS_NAME(spi_da1469x_stats_section, read_bytes)
    STATS_NAME(spi_da1469x_stats_section, written_bytes)
    STATS_NAME(spi_da1469x_stats_section, dma_transferred_bytes)
STATS_NAME_END(spi_da1469x_stats_section)
#endif

#define SPI_COUNT 2
/* Minimum transfer size when DMA should be used, for shorter transfers
 * interrupts are used
 */
#define MIN_DMA_SIZE 8

/* A value of 1 for the word size means 16-bit words */
#define SPI_CTRL_REG_16BIT_WORD (1 << SPI_SPI_CTRL_REG_SPI_WORD_Pos)

static const struct da1469x_dma_config spi_write_rx_dma_cfg = {
    .priority = 0,
    .burst_mode = MCU_DMA_BURST_MODE_DISABLED,
    .bus_width = MCU_DMA_BUS_WIDTH_1B,
    .dst_inc = false,
    .src_inc = false,
};

static const struct da1469x_dma_config spi_write_tx_dma_cfg = {
    .priority = 0,
    .burst_mode = MCU_DMA_BURST_MODE_DISABLED,
    .bus_width = MCU_DMA_BUS_WIDTH_1B,
    .dst_inc = false,
    .src_inc = true,
};

static const struct da1469x_dma_config spi_write_rx_dma_cfg16 = {
    .priority = 0,
    .burst_mode = MCU_DMA_BURST_MODE_DISABLED,
    .bus_width = MCU_DMA_BUS_WIDTH_2B,
    .dst_inc = false,
    .src_inc = false,
};

static const struct da1469x_dma_config spi_write_tx_dma_cfg16 = {
    .priority = 0,
    .burst_mode = MCU_DMA_BURST_MODE_DISABLED,
    .bus_width = MCU_DMA_BUS_WIDTH_2B,
    .dst_inc = false,
    .src_inc = true,
};

static const struct da1469x_dma_config spi_read_rx_dma_cfg = {
    .priority = 0,
    .burst_mode = MCU_DMA_BURST_MODE_DISABLED,
    .bus_width = MCU_DMA_BUS_WIDTH_1B,
    .dst_inc = true,
    .src_inc = false,
};

static const struct da1469x_dma_config spi_read_tx_dma_cfg = {
    .priority = 0,
    .burst_mode = MCU_DMA_BURST_MODE_DISABLED,
    .bus_width = MCU_DMA_BUS_WIDTH_1B,
    .dst_inc = false,
    .src_inc = false,
};

static const struct da1469x_dma_config spi_read_rx_dma_cfg16 = {
    .priority = 0,
    .burst_mode = MCU_DMA_BURST_MODE_DISABLED,
    .bus_width = MCU_DMA_BUS_WIDTH_2B,
    .dst_inc = true,
    .src_inc = false,
};

static const struct da1469x_dma_config spi_read_tx_dma_cfg16 = {
    .priority = 0,
    .burst_mode = MCU_DMA_BURST_MODE_DISABLED,
    .bus_width = MCU_DMA_BUS_WIDTH_2B,
    .dst_inc = false,
    .src_inc = false,
};

static void spi_da1469x_spi0_isr(void);
static void spi_da1469x_spi1_isr(void);

struct da1469x_spi_hw {
    SPI_Type *regs;
    uint8_t sck_func;
    uint8_t mosi_func;
    uint8_t miso_func;
    IRQn_Type irqn;
    /* DMA peripheral index */
    uint8_t dma_periph;
    /* Bit number for clock selection in CLK_COM_REG */
    uint8_t clk_src_bit;
    /* Bit number for clock enable in CLK_COM_REG */
    uint8_t clk_ena_bit;
    void (* isr)(void);
};

static const struct da1469x_spi_hw da1469x_spi = {
    .regs = (void *)SPI_BASE,
    .sck_func = MCU_GPIO_FUNC_SPI_CLK,
    .mosi_func = MCU_GPIO_FUNC_SPI_DO,
    .miso_func = MCU_GPIO_FUNC_SPI_DI,
    .irqn = SPI_IRQn,
    .dma_periph = MCU_DMA_PERIPH_SPI,
    .clk_src_bit = CRG_COM_CLK_COM_REG_SPI_CLK_SEL_Pos,
    .clk_ena_bit = CRG_COM_CLK_COM_REG_SPI_ENABLE_Pos,
    .isr = spi_da1469x_spi0_isr,
};

static const struct da1469x_spi_hw da1469x_spi2 = {
    .regs = (void *)SPI2_BASE,
    .sck_func = MCU_GPIO_FUNC_SPI2_CLK,
    .mosi_func = MCU_GPIO_FUNC_SPI2_DO,
    .miso_func = MCU_GPIO_FUNC_SPI2_DI,
    .irqn = SPI2_IRQn,
    .dma_periph = MCU_DMA_PERIPH_SPI2,
    .clk_src_bit = CRG_COM_CLK_COM_REG_SPI2_CLK_SEL_Pos,
    .clk_ena_bit = CRG_COM_CLK_COM_REG_SPI2_ENABLE_Pos,
    .isr = spi_da1469x_spi1_isr,
};

struct da1469x_transfer {
    /* Transmit or receive buffer */
    void *data;
    /* Transfer length (number of items) */
    uint16_t len;
    /* Number of items written to output FIFO */
    uint16_t wlen;
    /* Number of items read from input FIFO */
    uint16_t rlen;

    uint8_t nostop : 1;
    /* Current transfer is for write */
    uint8_t write : 1;
    /* DMA is used for current transfer */
    uint8_t dma : 1;
    /* Transfer started */
    uint8_t started : 1;
    /* Transfer is 16 bits */
    uint8_t xfr_16 : 1;
};

struct spi_da1469x_driver_data {
    struct bus_spi_dev *dev;
    const struct da1469x_spi_hw *hw;
    /* Semaphore used for end of transfer completion notification */
    struct os_sem sem;
    struct da1469x_dma_regs *dma_chans[2];
    struct da1469x_transfer transfer;

#if MYNEWT_VAL(SPI_DA1469X_STAT)
    STATS_SECT_DECL(spi_da1469x_stats_section) stats;
#endif
};

static struct spi_da1469x_driver_data spi_dev_data_0;
static struct spi_da1469x_driver_data spi_dev_data_1;

static inline struct spi_da1469x_driver_data *
driver_data(struct bus_spi_dev *dev)
{
    if (MYNEWT_VAL(SPI_0_MASTER) && dev->cfg.spi_num == 0) {
        return &spi_dev_data_0;
    } else if (MYNEWT_VAL(SPI_1_MASTER) && dev->cfg.spi_num == 1) {
        return &spi_dev_data_1;
    } else {
        assert(0);
        return NULL;
    }
}

static void
spi_da1469x_int_clear(SPI_Type *regs)
{
    regs->SPI_CLEAR_INT_REG = 0;
}

static void
spi_da1469x_int_enable(SPI_Type *regs)
{
    regs->SPI_CTRL_REG |= SPI_SPI_CTRL_REG_SPI_MINT_Msk;
}

static void
spi_da1469x_int_disable(SPI_Type *regs)
{
    regs->SPI_CTRL_REG &= ~SPI_SPI_CTRL_REG_SPI_MINT_Msk;
}

static int
spi_da1469x_dma_done_cb(void *arg)
{
    struct spi_da1469x_driver_data *dd = (struct spi_da1469x_driver_data *)arg;
    struct da1469x_transfer *transfer;
    int transferred;

    transfer = &dd->transfer;
    if (transfer->started) {
        transfer->started = 0;
        transferred = (int)(dd->dma_chans[0]->DMA_IDX_REG + 1);

        transfer->wlen += transferred;
        transfer->rlen += transferred;

#if MYNEWT_VAL(SPI_DA1469X_STAT)
        if (dd->transfer.xfr_16) {
            transferred *= 2;
        }
        if (transfer->write) {
            STATS_INCN(dd->stats, written_bytes, transferred);
        } else {
            STATS_INCN(dd->stats, read_bytes, transferred);
        }
        STATS_INCN(dd->stats, dma_transferred_bytes, transferred);
#endif

        os_sem_release(&dd->sem);
    }

    return 0;
}

static void
spi_da1469x_isr(SPI_Type *regs, struct spi_da1469x_driver_data *dd)
{
    struct da1469x_transfer *transfer = &dd->transfer;
    uint16_t rxdata;

    while ((transfer->wlen < transfer->len) &&
           !(regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_TXH_Msk)) {
        if (transfer->write) {
            if (transfer->xfr_16) {
                regs->SPI_RX_TX_REG = *(uint16_t *)transfer->data;
#if MYNEWT_VAL(SPI_DA1469X_STAT)
                STATS_INCN(dd->stats, written_bytes, 2);
#endif
                transfer->data = (uint16_t *)transfer->data + 1;
            } else {
                regs->SPI_RX_TX_REG = *(uint8_t *)transfer->data;
#if MYNEWT_VAL(SPI_DA1469X_STAT)
                STATS_INC(dd->stats, written_bytes);
#endif
                transfer->data = (uint8_t *)transfer->data + 1;
            }
        } else {
            /* Write 16 bits in case 16-bit transfer used */
            regs->SPI_RX_TX_REG = 0xFFFF;
        }
        transfer->wlen++;
    }

    if (transfer->wlen == transfer->len) {
        regs->SPI_CTRL_REG &= ~SPI_SPI_CTRL_REG_SPI_TX_FIFO_NOTFULL_MASK_Msk;
    }

    while (0 != (regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_INT_BIT_Msk)) {
        rxdata = regs->SPI_RX_TX_REG;
        if (transfer->rlen < transfer->len) {
            if (!transfer->write) {
                if (transfer->xfr_16) {
                    *(uint16_t *)transfer->data = (uint16_t)rxdata;
                    transfer->data = (uint16_t *)transfer->data + 1;
    #if MYNEWT_VAL(SPI_DA1469X_STAT)
                    STATS_INCN(dd->stats, read_bytes, 2);
    #endif
                } else {
                    *(uint8_t *)transfer->data = (uint8_t)rxdata;
                    transfer->data = (uint8_t *)transfer->data + 1;
    #if MYNEWT_VAL(SPI_DA1469X_STAT)
                    STATS_INC(dd->stats, read_bytes);
    #endif
                }
            }
            transfer->rlen++;
        }
        regs->SPI_CLEAR_INT_REG = 1;
    }

    if (transfer->started && transfer->rlen == transfer->len) {
        spi_da1469x_int_disable(regs);
        transfer->started = 0;
        assert(os_sem_get_count(&dd->sem) == 0);
        os_sem_release(&dd->sem);
    }
}

static void
spi_da1469x_spi0_isr(void)
{
    os_trace_isr_enter();

    spi_da1469x_isr((SPI_Type *)SPI, &spi_dev_data_0);

    os_trace_isr_exit();
}

static void
spi_da1469x_spi1_isr(void)
{
    os_trace_isr_enter();

    spi_da1469x_isr((SPI_Type *)SPI2, &spi_dev_data_1);

    os_trace_isr_exit();
}

static int
spi_da1469x_init_node(struct bus_dev *bdev, struct bus_node *bnode, void *arg)
{
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    struct bus_spi_node_cfg *cfg = arg;
    (void)bdev;

    BUS_DEBUG_POISON_NODE(node);

    node->pin_cs = cfg->pin_cs;
    node->freq = cfg->freq;
    node->quirks = cfg->quirks;
    node->data_order = cfg->data_order;
    node->mode = cfg->mode;

    if (node->pin_cs >= 0) {
        hal_gpio_init_out(node->pin_cs, 1);
    }

    return 0;
}

static int
spi_da1469x_disable(struct bus_dev *bdev)
{
    struct bus_spi_dev *dev = (struct bus_spi_dev *)bdev;
    struct spi_da1469x_driver_data *dd;
    SPI_Type *regs;

    BUS_DEBUG_VERIFY_DEV(dev);

    dd = driver_data(dev);
    regs = dd->hw->regs;

    /* Turn off SPI. */
    regs->SPI_CTRL_REG &= ~(1U << SPI_SPI_CTRL_REG_SPI_ON_Pos);

    if (dd->dma_chans[0] != NULL) {
        da1469x_dma_release_channel(dd->dma_chans[0]);
        dd->dma_chans[0] = NULL;
        dd->dma_chans[1] = NULL;
    }

    /*
     * Domain COM can be powered off, register content can be lost prepare
     * to set it from scratch.
     */
    dev->freq = 0;

    da1469x_pd_release(MCU_PD_DOMAIN_COM);

    return 0;
}

static int
spi_da1469x_enable(struct bus_dev *bdev)
{
    struct bus_spi_dev *dev = (struct bus_spi_dev *)bdev;
    const struct da1469x_spi_hw *spi_hw;
    struct spi_da1469x_driver_data *dd;
    SPI_Type *regs;

    BUS_DEBUG_VERIFY_DEV(dev);

    dd = driver_data(dev);
    spi_hw = dd->hw;
    regs = spi_hw->regs;

    da1469x_pd_acquire(MCU_PD_DOMAIN_COM);

    /* Enable clock */
    CRG_COM->SET_CLK_COM_REG = (1U << spi_hw->clk_ena_bit);

    regs->SPI_CTRL_REG |= (1U << SPI_SPI_CTRL_REG_SPI_ON_Pos);
    spi_da1469x_int_clear(spi_hw->regs);

    da1469x_dma_acquire_periph(-1, spi_hw->dma_periph, dd->dma_chans);

    if (dd->dma_chans[0] != NULL) {
        dd->dma_chans[0]->DMA_A_START_REG = (uint32_t)&regs->SPI_RX_TX_REG;
        dd->dma_chans[1]->DMA_B_START_REG = (uint32_t)&regs->SPI_RX_TX_REG;
    }

    return 0;
}

static int
spi_da1469x_configure(struct bus_dev *bdev, struct bus_node *bnode)
{
    struct bus_spi_dev *dev = (struct bus_spi_dev *)bdev;
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    struct spi_da1469x_driver_data *dd;
    SPI_Type *regs;
    uint32_t ctrl_reg;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    dd = driver_data(dev);
    regs = dd->hw->regs;

    ctrl_reg = regs->SPI_CTRL_REG;
    if (ctrl_reg & SPI_SPI_CTRL_REG_SPI_ON_Msk) {
        regs->SPI_CTRL_REG = ctrl_reg ^ (SPI_SPI_CTRL_REG_SPI_ON_Msk);
    }

    if ((dev->mode == node->mode) && (dev->data_order == node->data_order) &&
        (dev->freq == node->freq)) {
        /* Same configuration, no changes required. */
        goto end;
    }

    dev->freq = node->freq;
    dev->data_order = node->data_order;
    dev->mode = node->mode;

    ctrl_reg &= ~(SPI_SPI_CTRL_REG_SPI_TX_FIFO_NOTFULL_MASK_Msk |
                  SPI_SPI_CTRL_REG_SPI_DMA_TXREQ_MODE_Msk |
                  SPI_SPI_CTRL_REG_SPI_9BIT_VAL_Msk |
                  SPI_SPI_CTRL_REG_SPI_PRIORITY_Msk |
                  SPI_SPI_CTRL_REG_SPI_FIFO_MODE_Msk |
                  SPI_SPI_CTRL_REG_SPI_EN_CTRL_Msk |
                  SPI_SPI_CTRL_REG_SPI_MINT_Msk |
                  SPI_SPI_CTRL_REG_SPI_FORCE_DO_Msk |
                  SPI_SPI_CTRL_REG_SPI_WORD_Msk |
                  SPI_SPI_CTRL_REG_SPI_RST_Msk |
                  SPI_SPI_CTRL_REG_SPI_SMN_Msk |
                  SPI_SPI_CTRL_REG_SPI_DO_Msk |
                  SPI_SPI_CTRL_REG_SPI_CLK_Msk |
                  SPI_SPI_CTRL_REG_SPI_POL_Msk |
                  SPI_SPI_CTRL_REG_SPI_PHA_Msk);

    if (node->freq < 4000) {
        /* Slowest possibly clock, divider 14, 2.28MHz */
        ctrl_reg |= (3U << SPI_SPI_CTRL_REG_SPI_CLK_Pos);
    } else if (node->freq < 8000) {
        ctrl_reg |= (0U << SPI_SPI_CTRL_REG_SPI_CLK_Pos);
    } else if (node->freq < 16000) {
        ctrl_reg |= (1U << SPI_SPI_CTRL_REG_SPI_CLK_Pos);
    } else {
        ctrl_reg |= (2U << SPI_SPI_CTRL_REG_SPI_CLK_Pos);
    }

    switch (node->mode) {
    case BUS_SPI_MODE_0:
        /* Bits already zeroed */
        break;
    case BUS_SPI_MODE_1:
        ctrl_reg |= (1U << SPI_SPI_CTRL_REG_SPI_PHA_Pos);
        break;
    case BUS_SPI_MODE_2:
        ctrl_reg |= (1U << SPI_SPI_CTRL_REG_SPI_POL_Pos);
        break;
    case BUS_SPI_MODE_3:
        ctrl_reg |= (1U << SPI_SPI_CTRL_REG_SPI_PHA_Pos) |
                    (1U << SPI_SPI_CTRL_REG_SPI_POL_Pos);
        break;
    default:
        assert(0);
    }

    regs->SPI_CTRL_REG = ctrl_reg;
    /* At this point interrupt is cleared, FIFO is enabled, controller is disabled */
end:
    return 0;
}

static int
spi_da1469x_read(struct bus_dev *bdev, struct bus_node *bnode,
                 uint8_t *buf, uint16_t length, os_time_t timeout,
                 uint16_t flags)
{
    /* Data to send to MOSI while reading. */
    static uint32_t dma_dummy_src = 0;
    struct bus_spi_dev *dev = (struct bus_spi_dev *)bdev;
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    struct spi_da1469x_driver_data *dd;
    SPI_Type *regs;
    int rc;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    dd = driver_data(dev);
    regs = dd->hw->regs;

    assert(os_sem_get_count(&dd->sem) == 0);

    if (node->pin_cs >= 0) {
        hal_gpio_write(node->pin_cs, 0);
    }
    spi_da1469x_int_disable(regs);
    /* Ignore data that may be in receiver before. */
    while (regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_INT_BIT_Msk) {
        (void)regs->SPI_RX_TX_REG;
        spi_da1469x_int_clear(regs);
    }
    dd->transfer.data = buf;
    dd->transfer.len = length;
    dd->transfer.rlen = 0;
    dd->transfer.wlen = 0;
    dd->transfer.write = 0;
    dd->transfer.started = 1;
    if ((regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_WORD_Msk) == SPI_CTRL_REG_16BIT_WORD) {
        dd->transfer.xfr_16 = 1;
    } else {
        dd->transfer.xfr_16 = 0;
    }

    if (length >= MIN_DMA_SIZE && dd->dma_chans[0] != NULL) {
        dd->transfer.dma = 1;
        /* NOTE: assumes if 16-bit not used; dma transfers are 8-bits */
        if (dd->transfer.xfr_16) {
            da1469x_dma_configure(dd->dma_chans[0], &spi_read_rx_dma_cfg16, spi_da1469x_dma_done_cb, dd);
            da1469x_dma_configure(dd->dma_chans[1], &spi_read_tx_dma_cfg16, NULL, NULL);
        } else {
            da1469x_dma_configure(dd->dma_chans[0], &spi_read_rx_dma_cfg, spi_da1469x_dma_done_cb, dd);
            da1469x_dma_configure(dd->dma_chans[1], &spi_read_tx_dma_cfg, NULL, NULL);
        }
        da1469x_dma_read_peripheral(dd->dma_chans[0], buf, length);
        da1469x_dma_write_peripheral(dd->dma_chans[1], &dma_dummy_src, length);
        regs->SPI_CTRL_REG |= SPI_SPI_CTRL_REG_SPI_ON_Msk;
    } else {
        dd->transfer.dma = 0;
        /* Start with FIFO-not-full enabled interrupt. */
        regs->SPI_CTRL_REG |= SPI_SPI_CTRL_REG_SPI_TX_FIFO_NOTFULL_MASK_Msk | SPI_SPI_CTRL_REG_SPI_ON_Msk;
        spi_da1469x_int_enable(regs);
    }

    rc = os_sem_pend(&dd->sem, timeout);

    spi_da1469x_int_disable(regs);
    if (dd->dma_chans[0] != NULL) {
        dd->dma_chans[0]->DMA_CTRL_REG &= ~(1U << DMA_DMA0_CTRL_REG_DMA_ON_Pos);
        dd->dma_chans[1]->DMA_CTRL_REG &= ~(1U << DMA_DMA0_CTRL_REG_DMA_ON_Pos);
    }

    if (rc == OS_TIMEOUT) {
        rc = SYS_ETIMEOUT;
    } else if (rc) {
        rc = SYS_EUNKNOWN;
    }

    if ((rc != 0 || !(flags & BUS_F_NOSTOP)) && node->pin_cs >= 0) {
        hal_gpio_write(node->pin_cs, 1);
    }

    return rc;
}

static int
spi_da1469x_write(struct bus_dev *bdev, struct bus_node *bnode,
                  const uint8_t *buf, uint16_t length, os_time_t timeout,
                  uint16_t flags)
{
    /*
     * Variable to receive MISO data to be used by DMA,
     * any safe to write address would do.
     */
    static uint32_t dma_dummy_dst;
    struct bus_spi_dev *dev = (struct bus_spi_dev *)bdev;
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    struct spi_da1469x_driver_data *dd;
    SPI_Type *regs;
    int rc;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    dd = driver_data(dev);
    regs = dd->hw->regs;

    assert(os_sem_get_count(&dd->sem) == 0);

    if (node->pin_cs >= 0) {
        hal_gpio_write(node->pin_cs, 0);
    }
    spi_da1469x_int_disable(regs);
    /* Ignore data that may be in receiver before. */
    while (regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_INT_BIT_Msk) {
        (void)regs->SPI_RX_TX_REG;
        spi_da1469x_int_clear(regs);
    }
    dd->transfer.data = (uint8_t *)buf;
    dd->transfer.len = length;
    dd->transfer.rlen = 0;
    dd->transfer.wlen = 0;
    dd->transfer.nostop = (flags & BUS_F_NOSTOP) ? 1 : 0;
    dd->transfer.write = 1;
    dd->transfer.started = 1;
    if ((regs->SPI_CTRL_REG & SPI_SPI_CTRL_REG_SPI_WORD_Msk) == SPI_CTRL_REG_16BIT_WORD) {
        dd->transfer.xfr_16 = 1;
    } else {
        dd->transfer.xfr_16 = 0;
    }

    if (length >= MIN_DMA_SIZE && dd->dma_chans[0] != NULL) {
        dd->transfer.dma = 1;
        regs->SPI_CTRL_REG &= ~SPI_SPI_CTRL_REG_SPI_INT_BIT_Msk;
        spi_da1469x_int_disable(regs);
        /* NOTE: assumes if 16-bit not used; dma transfers are 8-bits */
        if (dd->transfer.xfr_16) {
            da1469x_dma_configure(dd->dma_chans[0], &spi_write_rx_dma_cfg16, spi_da1469x_dma_done_cb, dd);
            da1469x_dma_configure(dd->dma_chans[1], &spi_write_tx_dma_cfg16, NULL, NULL);
        } else {
            da1469x_dma_configure(dd->dma_chans[0], &spi_write_rx_dma_cfg, spi_da1469x_dma_done_cb, dd);
            da1469x_dma_configure(dd->dma_chans[1], &spi_write_tx_dma_cfg, NULL, NULL);
        }

        da1469x_dma_read_peripheral(dd->dma_chans[0], &dma_dummy_dst, length);
        da1469x_dma_write_peripheral(dd->dma_chans[1], buf, length);
        regs->SPI_CTRL_REG |= SPI_SPI_CTRL_REG_SPI_ON_Msk;
        spi_da1469x_int_clear(regs);
    } else {
        dd->transfer.dma = 0;
        /* Start with FIFO not full enabled interrupt. */
        regs->SPI_CTRL_REG |= SPI_SPI_CTRL_REG_SPI_TX_FIFO_NOTFULL_MASK_Msk | SPI_SPI_CTRL_REG_SPI_ON_Msk;
        spi_da1469x_int_clear(regs);
        spi_da1469x_int_enable(regs);
    }

    rc = os_sem_pend(&dd->sem, timeout);

    spi_da1469x_int_disable(regs);
    if (dd->dma_chans[0] != NULL) {
        dd->dma_chans[0]->DMA_CTRL_REG &= ~(1U << DMA_DMA0_CTRL_REG_DMA_ON_Pos);
        dd->dma_chans[1]->DMA_CTRL_REG &= ~(1U << DMA_DMA0_CTRL_REG_DMA_ON_Pos);
    }

    if (rc == OS_TIMEOUT) {
        rc = SYS_ETIMEOUT;
    } else if (rc) {
        rc = SYS_EUNKNOWN;
    }

    if ((rc != 0 || !(flags & BUS_F_NOSTOP)) && node->pin_cs >= 0) {
        hal_gpio_write(node->pin_cs, 1);
    }

    return rc;
}

static const struct bus_dev_ops bus_spi_da1469x_ops = {
    .init_node = spi_da1469x_init_node,
    .enable = spi_da1469x_enable,
    .configure = spi_da1469x_configure,
    .read = spi_da1469x_read,
    .write = spi_da1469x_write,
    .disable = spi_da1469x_disable,
};

int
bus_spi_da1469x_dev_init_func(struct os_dev *odev, void *arg)
{
    struct bus_spi_dev *dev = (struct bus_spi_dev *)odev;
    struct bus_spi_dev_cfg *cfg = arg;
    struct spi_da1469x_driver_data *dd;
#if MYNEWT_VAL(SPI_DA1469X_STAT)
    char *stats_name;
#endif
    int rc;
    const struct da1469x_spi_hw *spi_hw;

    BUS_DEBUG_POISON_DEV(dev);

    if (MYNEWT_VAL(SPI_0_MASTER) && cfg->spi_num == 0) {
        dd = &spi_dev_data_0;
        spi_hw = &da1469x_spi;
    } else if (MYNEWT_VAL(SPI_1_MASTER) && cfg->spi_num == 1) {
        dd = &spi_dev_data_1;
        spi_hw = &da1469x_spi2;
    } else {
        return SYS_EINVAL;
    }
    if (dd->dev) {
        return SYS_EALREADY;
    }

    dd->dev = dev;
    dd->hw = spi_hw;
    dev->cfg = *cfg;

    mcu_gpio_set_pin_function(dev->cfg.pin_sck, MCU_GPIO_MODE_OUTPUT, spi_hw->sck_func);
    mcu_gpio_set_pin_function(dev->cfg.pin_mosi, MCU_GPIO_MODE_OUTPUT, spi_hw->mosi_func);
    mcu_gpio_set_pin_function(dev->cfg.pin_miso, MCU_GPIO_MODE_INPUT, spi_hw->miso_func);

    /* Select DIVN clock for SPI block. */
    CRG_COM->RESET_CLK_COM_REG = (1U << spi_hw->clk_src_bit);

    NVIC_DisableIRQ(spi_hw->irqn);
    NVIC_SetVector(spi_hw->irqn, (uint32_t)spi_hw->isr);
    NVIC_ClearPendingIRQ(spi_hw->irqn);
    NVIC_EnableIRQ(spi_hw->irqn);

    os_sem_init(&dd->sem, 0);

#if MYNEWT_VAL(SPI_DA1469X_STAT)
    asprintf(&stats_name, "spi_da1469x_%d", cfg->spi_num);
    rc = stats_init_and_reg(STATS_HDR(dd->stats),
                            STATS_SIZE_INIT_PARMS(dd->stats, STATS_SIZE_32),
                            STATS_NAME_INIT_PARMS(spi_da1469x_stats_section),
                            stats_name);
    assert(rc == 0);
#endif

    rc = bus_dev_init_func(odev, (void *)&bus_spi_da1469x_ops);
    assert(rc == 0);

    return 0;
}
