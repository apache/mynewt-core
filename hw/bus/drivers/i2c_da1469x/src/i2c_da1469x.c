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
#include "bus/drivers/i2c_common.h"
#include "mcu/mcu.h"

#if MYNEWT_VAL(I2C_DA1469X_BUS_DRIVER)
#include "mcu/da1469x_dma.h"
#endif

#if MYNEWT_VAL(I2C_DA1469X_STAT)
#include "stats/stats.h"
#endif

#if MYNEWT_VAL(I2C_DA1469X_STAT)
STATS_SECT_START(i2c_da1469x_stats_section)
    STATS_SECT_ENTRY(dma_read_bytes)
    STATS_SECT_ENTRY(dma_written_bytes)
    STATS_SECT_ENTRY(i2c_errors)
STATS_SECT_END

STATS_NAME_START(i2c_da1469x_stats_section)
    STATS_NAME(i2c_da1469x_stats_section, dma_read_bytes)
    STATS_NAME(i2c_da1469x_stats_section, dma_written_bytes)
    STATS_NAME(i2c_da1469x_stats_section, i2c_errors)
STATS_NAME_END(i2c_da1469x_stats_section)
#endif

#define I2C_COUNT 2
/* Minimum transfer size when DMA should be used, for shorter transfers
 * interrupts are used
 */
#define MIN_DMA_SIZE 2

static void i2c_da1469x_i2c0_isr(void);
static void i2c_da1469x_i2c1_isr(void);

struct da1469x_i2c_hw {
    I2C_Type *regs;
    uint8_t scl_func;
    uint8_t sda_func;
    IRQn_Type irqn;
    /* DMA peripheral index */
    uint8_t dma_periph;
    /* Bit number for clock selection in CLK_COM_REG */
    uint8_t clk_src_bit;
    /* Bit number for clock enable in CLK_COM_REG */
    uint8_t clk_ena_bit;
    void (* isr)(void);
};

static const struct da1469x_i2c_hw da1469x_i2c[2] = {
    {
        .regs = (void *)I2C_BASE,
        .scl_func = MCU_GPIO_FUNC_I2C_SCL,
        .sda_func = MCU_GPIO_FUNC_I2C_SDA,
        .irqn = I2C_IRQn,
        .dma_periph = MCU_DMA_PERIPH_I2C,
        .clk_src_bit = CRG_COM_CLK_COM_REG_I2C_CLK_SEL_Pos,
        .clk_ena_bit = CRG_COM_CLK_COM_REG_I2C_ENABLE_Pos,
        .isr = i2c_da1469x_i2c0_isr,
    },
    {
        .regs = (void *)I2C2_BASE,
        .scl_func = MCU_GPIO_FUNC_I2C2_SCL,
        .sda_func = MCU_GPIO_FUNC_I2C2_SDA,
        .irqn = I2C2_IRQn,
        .dma_periph = MCU_DMA_PERIPH_I2C2,
        .clk_src_bit = CRG_COM_CLK_COM_REG_I2C2_CLK_SEL_Pos,
        .clk_ena_bit = CRG_COM_CLK_COM_REG_I2C2_ENABLE_Pos,
        .isr = i2c_da1469x_i2c1_isr,
    }
};

struct da1469x_transfer {
    uint8_t *data;
    uint16_t wlen;
    uint16_t rlen;
    uint8_t nostop:1;
    uint8_t write:1;
    uint8_t dma:1;
    uint8_t started:1;
};

_Static_assert(MYNEWT_VAL(I2C_DA1469X_TX_DMA_BUFFER_SIZE) > 0, "Value must be 1 or more");

struct i2c_da1469x_dev_data {
    struct bus_i2c_dev *dev;
    struct os_sem sem;
    uint32_t errorsrc;
    struct da1469x_dma_regs *dma_chans[2];
    struct da1469x_transfer transfer;
    void (*i2c_isr)(I2C_Type *i2c_regs, struct i2c_da1469x_dev_data *dd);

#if MYNEWT_VAL(I2C_DA1469X_STAT)
    STATS_SECT_DECL(i2c_da1469x_stats_section) stats;
#endif
    uint16_t tx_dma_buf[MYNEWT_VAL(I2C_DA1469X_TX_DMA_BUFFER_SIZE)];
};

static struct i2c_da1469x_dev_data i2c_dev_data[I2C_COUNT];

static void
i2c_da1469x_clear_int(I2C_Type *i2c_regs)
{
    (void)i2c_regs->I2C_CLR_INTR_REG;
}

static void
i2c_da1469x_dma_rx_isr(struct bus_i2c_dev *dev)
{
    struct i2c_da1469x_dev_data *dd;
    int transferred;
    struct da1469x_transfer *transfer;

    dd = &i2c_dev_data[dev->cfg.i2c_num];
    transferred = (int)(dd->dma_chans[0]->DMA_IDX_REG + 1);
    transfer = &dd->transfer;

    transfer->data += transferred;
    transfer->rlen -= transferred;

#if MYNEWT_VAL(I2C_DA1469X_STAT)
    STATS_INCN(dd->stats, dma_read_bytes, transferred);
#endif
    assert(transfer->rlen == 0);

    transfer->started = 0;

    os_sem_release(&dd->sem);
}

void
i2c_da1469x_fill_tx_dma_buffer(struct i2c_da1469x_dev_data *dd)
{
    uint16_t i;
    uint16_t length = dd->transfer.wlen;

    if (dd->transfer.write) {
        if (length > MYNEWT_VAL(I2C_DA1469X_TX_DMA_BUFFER_SIZE)) {
            length = MYNEWT_VAL(I2C_DA1469X_TX_DMA_BUFFER_SIZE);
        }
        /* Byte array converted to word array */
        for (i = 0; i < length; ++i) {
            dd->tx_dma_buf[i] = dd->transfer.data[i];
        }
        /* Add stop request is required */
        if (!dd->transfer.nostop && i == dd->transfer.wlen) {
            dd->tx_dma_buf[i - 1] |= I2C_I2C_DATA_CMD_REG_I2C_STOP_Msk;
        }
    } else {
        if (dd->transfer.nostop) {
            /* No stop, all can be transmitted with one DMA request */
            dd->tx_dma_buf[0] = I2C_I2C_DATA_CMD_REG_I2C_CMD_Msk;
        } else if (length == 1) {
            /* Stop requested and only one byte left, add stop */
            dd->tx_dma_buf[0] = I2C_I2C_DATA_CMD_REG_I2C_CMD_Msk |
                                I2C_I2C_DATA_CMD_REG_I2C_STOP_Msk;
        } else {
            /* Stop requested more send lentght - 1 without stop */
            dd->tx_dma_buf[0] = I2C_I2C_DATA_CMD_REG_I2C_CMD_Msk;
            --length;
        }
    }

    dd->dma_chans[1]->DMA_A_START_REG = (uint32_t)dd->tx_dma_buf;
    dd->dma_chans[1]->DMA_LEN_REG = (uint32_t)(length - 1);
    dd->dma_chans[1]->DMA_INT_REG = (uint32_t)(length - 1);
    dd->dma_chans[1]->DMA_CTRL_REG = (0U << DMA_DMA0_CTRL_REG_DMA_INIT_Pos) |
                                     (3U << DMA_DMA0_CTRL_REG_DMA_PRIO_Pos) |
        /* Increment source for writes, for read read same data */
                                     (dd->transfer.write << DMA_DMA0_CTRL_REG_AINC_Pos) |
                                     (0U << DMA_DMA0_CTRL_REG_BINC_Pos) |
                                     (1U << DMA_DMA0_CTRL_REG_DREQ_MODE_Pos) |
                                     (1U << DMA_DMA0_CTRL_REG_BW_Pos) |
                                     (1U << DMA_DMA0_CTRL_REG_DMA_ON_Pos);
}

static void
i2c_da1469x_dma_tx_isr(struct bus_i2c_dev *dev)
{
    struct i2c_da1469x_dev_data *dd;
    int transferred;
    struct da1469x_transfer *transfer;

    dd = &i2c_dev_data[dev->cfg.i2c_num];
    transferred = (int)(dd->dma_chans[1]->DMA_IDX_REG + 1);
    transfer = &dd->transfer;

    /* Update transfer data */
    if (transfer->write) {
        transfer->data += transferred;
    }
    transfer->wlen -= transferred;
#if MYNEWT_VAL(I2C_DA1469X_STAT)
    STATS_INCN(dd->stats, dma_written_bytes, transferred);
#endif

    /* More data to transfer */
    if (transfer->wlen) {
        i2c_da1469x_fill_tx_dma_buffer(dd);
    } else if (transfer->write && transfer->started && transfer->nostop) {
        transfer->started = 0;
        os_sem_release(&dd->sem);
    }
}

static void
i2c_da1469x_fill_fifo_for_rx(I2C_Type *i2c_regs, struct i2c_da1469x_dev_data *dd)
{
    uint32_t intr_stat;
    struct da1469x_transfer *transfer;

    transfer = &dd->transfer;

    intr_stat = i2c_regs->I2C_INTR_STAT_REG;

    if (intr_stat & I2C_I2C_INTR_MASK_REG_M_TX_ABRT_Msk) {
        dd->errorsrc = i2c_regs->I2C_TX_ABRT_SOURCE_REG;
        (void)i2c_regs->I2C_CLR_TX_ABRT_REG;
#if MYNEWT_VAL(I2C_DA1469X_STAT)
        STATS_INC(dd->stats, i2c_errors);
#endif
        i2c_da1469x_clear_int(i2c_regs);
        /* Stop DMA */
        dd->dma_chans[1]->DMA_CTRL_REG &= ~(1U << DMA_DMA0_CTRL_REG_DMA_ON_Pos);
        dd->dma_chans[0]->DMA_CTRL_REG &= ~(1U << DMA_DMA0_CTRL_REG_DMA_ON_Pos);
        transfer->started = 0;
        i2c_regs->I2C_INTR_MASK_REG = 0;

        /* Finish transaction */
        os_sem_release(&dd->sem);
        return;
    }

    i2c_da1469x_clear_int(i2c_regs);

    while ((transfer->wlen > 0) &&
           i2c_regs->I2C_STATUS_REG & I2C_I2C_STATUS_REG_TFNF_Msk) {
        transfer->wlen--;
        if (transfer->wlen) {
            i2c_regs->I2C_DATA_CMD_REG = (1U << I2C_I2C_DATA_CMD_REG_I2C_CMD_Pos);
        } else {
            i2c_regs->I2C_DATA_CMD_REG = (1U << I2C_I2C_DATA_CMD_REG_I2C_CMD_Pos) |
                                         (1U << I2C_I2C_DATA_CMD_REG_I2C_STOP_Pos);
        }
    }

    if (transfer->dma == 0) {
        while ((transfer->rlen > 0) &&
               i2c_regs->I2C_STATUS_REG & I2C_I2C_STATUS_REG_RFNE_Msk) {
            *transfer->data++ = (uint8_t)i2c_regs->I2C_DATA_CMD_REG;
            transfer->rlen--;
            if (transfer->rlen == 0) {
                i2c_regs->I2C_INTR_MASK_REG = 0;
                i2c_da1469x_clear_int(i2c_regs);
                transfer->started = 0;
                os_sem_release(&dd->sem);
            }
        }
    }

    if (intr_stat & I2C_I2C_INTR_MASK_REG_M_STOP_DET_Msk) {
        i2c_regs->I2C_INTR_MASK_REG = 0;
        assert(transfer->started == 0 && transfer->rlen == 0);
        if (transfer->started || transfer->rlen) {
            transfer->started = 0;
            transfer->rlen = 0;
            dd->errorsrc = I2C_I2C_TX_ABRT_SOURCE_REG_ABRT_USER_ABRT_Msk;
            os_sem_release(&dd->sem);
        }
        return;
    }

    /* All data in tx buffer, no need for TX interrupt any more */
    if (transfer->wlen == 0) {
        i2c_regs->I2C_INTR_MASK_REG &= ~I2C_I2C_INTR_MASK_REG_M_TX_EMPTY_Msk;
    }
}

static void
i2c_da1469x_fill_fifo_for_tx(I2C_Type *i2c_regs, struct i2c_da1469x_dev_data *dd)
{
    uint32_t intr_stat;
    struct da1469x_transfer *transfer = &dd->transfer;

    intr_stat = i2c_regs->I2C_INTR_STAT_REG;

    /* If RX FIFO is not empty read all since this is write only stage */
    if (intr_stat & I2C_I2C_INTR_MASK_REG_M_RX_FULL_Msk) {
        while (i2c_regs->I2C_STATUS_REG & I2C2_I2C2_STATUS_REG_RFNE_Msk) {
            /* Read and ignore from RX FIFO */
            if (i2c_regs->I2C_DATA_CMD_REG) {}
        }
    }

    if (intr_stat & I2C_I2C_INTR_MASK_REG_M_TX_ABRT_Msk) {
        dd->errorsrc = i2c_regs->I2C_TX_ABRT_SOURCE_REG;
        (void)i2c_regs->I2C_CLR_TX_ABRT_REG;
#if MYNEWT_VAL(I2C_DA1469X_STAT)
        STATS_INC(dd->stats, i2c_errors);
#endif
        i2c_da1469x_clear_int(i2c_regs);
        if (transfer->dma) {
            dd->dma_chans[1]->DMA_CTRL_REG &= ~(1U << DMA_DMA0_CTRL_REG_DMA_ON_Pos);
        }
        i2c_regs->I2C_INTR_MASK_REG = 0;
        os_sem_release(&dd->sem);
        return;
    }

    i2c_da1469x_clear_int(i2c_regs);

    while ((transfer->wlen > 0) &&
           i2c_regs->I2C_STATUS_REG & I2C_I2C_STATUS_REG_TFNF_Msk) {
        transfer->wlen--;
        if (transfer->wlen != 0 || transfer->nostop) {
            i2c_regs->I2C_DATA_CMD_REG = *transfer->data++;
        } else {
            i2c_regs->I2C_DATA_CMD_REG = *transfer->data++ |
                                         (1U << I2C_I2C_DATA_CMD_REG_I2C_STOP_Pos);
        }
        if (transfer->wlen == 0) {
            if (transfer->nostop) {
                i2c_regs->I2C_INTR_MASK_REG = 0;
                i2c_da1469x_clear_int(i2c_regs);
                transfer->started = 0;
                os_sem_release(&dd->sem);
            }
        }
    }
    if (transfer->wlen == 0) {
        i2c_regs->I2C_INTR_MASK_REG &= ~I2C_I2C_INTR_MASK_REG_M_TX_EMPTY_Msk;
    }

    if (intr_stat & I2C_I2C_INTR_MASK_REG_M_STOP_DET_Msk) {
        i2c_regs->I2C_INTR_MASK_REG = 0;
        if (transfer->started) {
            transfer->started = 0;
            os_sem_release(&dd->sem);
        }
    }
}

static void
i2c_da1469x_i2c0_isr(void)
{
    os_trace_isr_enter();

    i2c_dev_data[0].i2c_isr((I2C_Type *)I2C_BASE, &i2c_dev_data[0]);

    os_trace_isr_exit();
}

static void
i2c_da1469x_i2c1_isr(void)
{
    os_trace_isr_enter();

    i2c_dev_data[1].i2c_isr((I2C_Type *)I2C2_BASE, &i2c_dev_data[1]);

    os_trace_isr_exit();
}

static int
i2c_da1469x_init_node(struct bus_dev *bdev, struct bus_node *bnode, void *arg)
{
    struct bus_i2c_node *node = (struct bus_i2c_node *)bnode;
    struct bus_i2c_node_cfg *cfg = arg;

    BUS_DEBUG_POISON_NODE(node);

    node->freq = cfg->freq;
    node->addr = cfg->addr;
    node->quirks = cfg->quirks;

    return 0;
}

static int
i2c_da1469x_enable(struct bus_dev *bdev)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    I2C_Type *i2c_regs;

    BUS_DEBUG_VERIFY_DEV(dev);

    i2c_regs = da1469x_i2c[dev->cfg.i2c_num].regs;

    /* Disable I2C */
    i2c_regs->I2C_ENABLE_REG |= (1U << I2C_I2C_ENABLE_REG_I2C_EN_Pos);

    return 0;
}

static int
i2c_da1469x_configure(struct bus_dev *bdev, struct bus_node *bnode)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    struct bus_i2c_node *node = (struct bus_i2c_node *)bnode;
    struct bus_i2c_node *current_node = (struct bus_i2c_node *)bdev->configured_for;
    I2C_Type *i2c_regs;
    uint32_t speed;
    int rc = 0;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    i2c_regs = da1469x_i2c[dev->cfg.i2c_num].regs;

    if (i2c_regs->I2C_ENABLE_REG & I2C_I2C_ENABLE_REG_I2C_EN_Msk) {
        i2c_regs->I2C_ENABLE_REG &= ~(1U << I2C_I2C_ENABLE_REG_I2C_EN_Pos);
    }

    i2c_regs->I2C_TAR_REG = node->addr & I2C_I2C_TAR_REG_IC_TAR_Msk;

    if (current_node && (current_node->freq == node->freq)) {
        goto end;
    }

    switch (node->freq) {
    case 100:
        speed = 1;
        break;
    case 400:
        speed = 2;
        break;
    case 3400:
        speed = 3;
        break;
    default:
        rc = SYS_EIO;
        goto end;
    }
    i2c_regs->I2C_CON_REG = (i2c_regs->I2C_CON_REG & ~I2C_I2C_CON_REG_I2C_SPEED_Msk) |
                            (speed << I2C_I2C_CON_REG_I2C_SPEED_Pos);
end:
    return rc;
}

static int
i2c_da1469x_translate_abort(uint32_t abort_code)
{
    if (abort_code &
        (I2C_I2C_TX_ABRT_SOURCE_REG_ABRT_7B_ADDR_NOACK_Msk |
         I2C_I2C_TX_ABRT_SOURCE_REG_ABRT_10ADDR1_NOACK_Msk |
         I2C_I2C_TX_ABRT_SOURCE_REG_ABRT_10ADDR2_NOACK_Msk)) {
        return SYS_ENOENT;
    } else if (abort_code & I2C_I2C_TX_ABRT_SOURCE_REG_ABRT_TXDATA_NOACK_Msk) {
        return SYS_EREMOTEIO;
    } else {
        return SYS_EIO;
    }
}

static int
i2c_da1469x_read(struct bus_dev *bdev, struct bus_node *bnode,
                 uint8_t *buf, uint16_t length, os_time_t timeout,
                 uint16_t flags)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    struct bus_i2c_node *node = (struct bus_i2c_node *)bnode;
    struct i2c_da1469x_dev_data *dd;
    I2C_Type *i2c_regs;
    int rc;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    i2c_regs = da1469x_i2c[dev->cfg.i2c_num].regs;
    dd = &i2c_dev_data[dev->cfg.i2c_num];

    i2c_regs->I2C_INTR_MASK_REG = 0;
    dd->errorsrc = 0;
    dd->transfer.rlen = length;
    dd->transfer.wlen = length;
    dd->transfer.data = buf;
    dd->transfer.nostop = (flags & BUS_F_NOSTOP) != 0;
    dd->transfer.write = 0;
    dd->transfer.started = 1;
    dd->i2c_isr = i2c_da1469x_fill_fifo_for_rx;
    if (length >= MIN_DMA_SIZE) {
        i2c_regs->I2C_DMA_CR_REG = 0;
        /*
         * To read I2C controller with DMA output FIFO must be fed with
         * read requests. In this case only one value is needed if DMA source
         * is not incremented.
         */
        dd->transfer.dma = 1;
        dd->dma_chans[0]->DMA_B_START_REG = (uint32_t)buf;
        /*
         * Read is for all requested bytes, length - 1 means that DMA will
         * perform length transfers.
         */
        dd->dma_chans[0]->DMA_LEN_REG = (uint32_t)length - 1;
        dd->dma_chans[0]->DMA_INT_REG = (uint32_t)length - 1;
        dd->dma_chans[0]->DMA_CTRL_REG |= (1U << DMA_DMA0_CTRL_REG_DMA_ON_Pos);

        i2c_da1469x_fill_tx_dma_buffer(dd);

        i2c_regs->I2C_DMA_CR_REG = (1U << I2C_I2C_DMA_CR_REG_TDMAE_Pos) |
                                   (1U << I2C_I2C_DMA_CR_REG_RDMAE_Pos);
        i2c_regs->I2C_INTR_MASK_REG = I2C_I2C_INTR_MASK_REG_M_TX_ABRT_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_STOP_DET_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_SCL_STUCK_AT_LOW_Msk;
    } else {
        dd->transfer.dma = 0;
        i2c_regs->I2C_INTR_MASK_REG = I2C_I2C_INTR_MASK_REG_M_TX_ABRT_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_STOP_DET_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_TX_EMPTY_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_RX_FULL_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_SCL_STUCK_AT_LOW_Msk;
    }
    i2c_da1469x_clear_int(i2c_regs);
    i2c_regs->I2C_ENABLE_REG = 1;

    rc = os_sem_pend(&dd->sem, timeout);
    if (rc == OS_TIMEOUT) {
        rc = SYS_ETIMEOUT;
    } else if (rc) {
        rc = SYS_EUNKNOWN;
    } else if (dd->errorsrc) {
        rc = i2c_da1469x_translate_abort(dd->errorsrc);
    }

    return rc;
}

static int
i2c_da1469x_write(struct bus_dev *bdev, struct bus_node *bnode,
                  const uint8_t *buf, uint16_t length, os_time_t timeout,
                  uint16_t flags)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    struct bus_i2c_node *node = (struct bus_i2c_node *)bnode;
    struct i2c_da1469x_dev_data *dd;
    I2C_Type *i2c_regs;
    int rc;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);


    i2c_regs = da1469x_i2c[dev->cfg.i2c_num].regs;
    dd = &i2c_dev_data[dev->cfg.i2c_num];

    dd->errorsrc = 0;
    dd->transfer.rlen = 0;
    dd->transfer.wlen = length;
    dd->transfer.data = (uint8_t *)buf;
    dd->transfer.nostop = (flags & BUS_F_NOSTOP) ? 1 : 0;
    dd->transfer.write = 1;
    dd->transfer.started = 1;
    dd->i2c_isr = i2c_da1469x_fill_fifo_for_tx;
    if (length >= MIN_DMA_SIZE) {
        dd->transfer.dma = 1;
        i2c_regs->I2C_DMA_CR_REG = 0;
        i2c_da1469x_fill_tx_dma_buffer(dd);
        i2c_regs->I2C_DMA_CR_REG = (1U << I2C_I2C_DMA_CR_REG_TDMAE_Pos);
        i2c_regs->I2C_INTR_MASK_REG = I2C_I2C_INTR_MASK_REG_M_TX_ABRT_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_STOP_DET_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_SCL_STUCK_AT_LOW_Msk;
    } else {
        dd->transfer.dma = 0;
        i2c_regs->I2C_INTR_MASK_REG = I2C_I2C_INTR_MASK_REG_M_TX_ABRT_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_STOP_DET_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_TX_EMPTY_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_RX_FULL_Msk |
                                      I2C_I2C_INTR_MASK_REG_M_SCL_STUCK_AT_LOW_Msk;
    }
    i2c_da1469x_clear_int(i2c_regs);
    i2c_regs->I2C_ENABLE_REG = 1;

    rc = os_sem_pend(&dd->sem, timeout);
    dd->dma_chans[1]->DMA_CTRL_REG &= ~(1U << DMA_DMA0_CTRL_REG_DMA_ON_Pos);

    if (rc == OS_TIMEOUT) {
        rc = SYS_ETIMEOUT;
    } else if (rc) {
        rc = SYS_EUNKNOWN;
    } else if (dd->errorsrc) {
        rc = i2c_da1469x_translate_abort(dd->errorsrc);
    }

    return rc;
}

static int
i2c_da1469x_disable(struct bus_dev *bdev)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    I2C_Type *i2c_regs;

    BUS_DEBUG_VERIFY_DEV(dev);

    i2c_regs = da1469x_i2c[dev->cfg.i2c_num].regs;

    /* Disable I2C */
    i2c_regs->I2C_ENABLE_REG &= ~(1U << I2C_I2C_ENABLE_REG_I2C_EN_Pos);

    return 0;
}

static const struct bus_dev_ops bus_i2c_da1469x_dma_ops = {
    .init_node = i2c_da1469x_init_node,
    .enable = i2c_da1469x_enable,
    .configure = i2c_da1469x_configure,
    .read = i2c_da1469x_read,
    .write = i2c_da1469x_write,
    .disable = i2c_da1469x_disable,
};

int
bus_i2c_da1469x_dev_init_func(struct os_dev *odev, void *arg)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)odev;
    struct bus_i2c_dev_cfg *cfg = arg;
    struct i2c_da1469x_dev_data *dd;
#if MYNEWT_VAL(I2C_DA1469X_STAT)
    char *stats_name;
#endif
    int rc;
    const struct da1469x_i2c_hw *i2c_hw;
    static const struct da1469x_dma_config rx_cfg = {
        .src_inc = false,
        .dst_inc = true,
        .priority = 0,
        .burst_mode = 0,
        .bus_width = 0,
    };
    static const struct da1469x_dma_config tx_cfg = {
        .src_inc = true,
        .dst_inc = false,
        .priority = 0,
        .burst_mode = 0,
        .bus_width = 0,
    };

    BUS_DEBUG_POISON_DEV(dev);

    if (i2c_dev_data[cfg->i2c_num].dev) {
        return SYS_EALREADY;
    }

    i2c_dev_data[cfg->i2c_num].dev = dev;
    i2c_hw = &da1469x_i2c[cfg->i2c_num];
    dd = &i2c_dev_data[cfg->i2c_num];

    dev->cfg = *cfg;

    da1469x_pd_acquire(MCU_PD_DOMAIN_COM);

    /* Select DIVN clock */
    CRG_COM->RESET_CLK_COM_REG = (1U << i2c_hw->clk_src_bit);
    /* Enable clock */
    CRG_COM->SET_CLK_COM_REG = (1U << i2c_hw->clk_ena_bit);

    /* Abort ongoing transactions and disable I2C */
    i2c_hw->regs->I2C_ENABLE_REG |= (1 << I2C_I2C_ENABLE_REG_I2C_ABORT_Pos);
    i2c_hw->regs->I2C_ENABLE_REG &= ~(1 << I2C_I2C_ENABLE_REG_I2C_EN_Pos);
    while (i2c_hw->regs->I2C_ENABLE_STATUS_REG & I2C_I2C_ENABLE_STATUS_REG_IC_EN_Msk);

    i2c_hw->regs->I2C_CON_REG = (1U << I2C_I2C_CON_REG_I2C_MASTER_MODE_Pos) |
                                (1U << I2C_I2C_CON_REG_I2C_SPEED_Pos) |
                                (1U << I2C_I2C_CON_REG_I2C_RESTART_EN_Pos);

    i2c_hw->regs->I2C_INTR_MASK_REG = 0;
    i2c_da1469x_clear_int(i2c_hw->regs);
    da1469x_dma_acquire_periph(-1, i2c_hw->dma_periph, dd->dma_chans);
    da1469x_dma_configure(dd->dma_chans[0], &rx_cfg,
                          (da1469x_dma_interrupt_cb)i2c_da1469x_dma_rx_isr, dev);
    dd->dma_chans[0]->DMA_A_START_REG = (uint32_t)&i2c_hw->regs->I2C_DATA_CMD_REG;
    da1469x_dma_configure(dd->dma_chans[1], &tx_cfg,
                          (da1469x_dma_interrupt_cb)i2c_da1469x_dma_tx_isr, dev);
    dd->dma_chans[1]->DMA_B_START_REG = (uint32_t)&i2c_hw->regs->I2C_DATA_CMD_REG;

    mcu_gpio_set_pin_function(cfg->pin_scl, MCU_GPIO_MODE_OUTPUT_OPEN_DRAIN,
                              i2c_hw->scl_func);
    mcu_gpio_set_pin_function(cfg->pin_sda, MCU_GPIO_MODE_OUTPUT_OPEN_DRAIN,
                              i2c_hw->sda_func);

    NVIC_DisableIRQ(i2c_hw->irqn);
    NVIC_SetVector(i2c_hw->irqn, (uint32_t)i2c_hw->isr);
    NVIC_ClearPendingIRQ(i2c_hw->irqn);
    NVIC_EnableIRQ(i2c_hw->irqn);

    os_sem_init(&dd->sem, 0);

#if MYNEWT_VAL(I2C_DA1469X_STAT)
    asprintf(&stats_name, "i2c_da1469x_%d", cfg->i2c_num);
    /* XXX should we assert or return error on failure? */
    stats_init_and_reg(STATS_HDR(dd->stats),
                       STATS_SIZE_INIT_PARMS(dd->stats, STATS_SIZE_32),
                       STATS_NAME_INIT_PARMS(i2c_da1469x_stats_section),
                       stats_name);
#endif

    rc = bus_dev_init_func(odev, (void*)&bus_i2c_da1469x_dma_ops);
    assert(rc == 0);

    return 0;
}
