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

#include <os/mynewt.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_uart.h>

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include <bus/drivers/spi_common.h>
#endif
#include <max3107/max3107.h>

#include <uart/uart.h>
#include <stats/stats.h>

#if MYNEWT_VAL(MAX3107_STATS)
STATS_NAME_START(max3107_stats_section)
        STATS_NAME(max3107_stats_section, lock_timeouts)
        STATS_NAME(max3107_stats_section, uart_read_ops)
        STATS_NAME(max3107_stats_section, uart_read_errors)
        STATS_NAME(max3107_stats_section, uart_breaks)
        STATS_NAME(max3107_stats_section, uart_read_bytes)
        STATS_NAME(max3107_stats_section, uart_write_ops)
        STATS_NAME(max3107_stats_section, uart_write_errors)
        STATS_NAME(max3107_stats_section, uart_write_bytes)
STATS_NAME_END(bus_stats_section)
#endif

static inline int
max3107_lock(struct max3107_dev *dev)
{
    return os_error_to_sys(os_mutex_pend(&dev->lock,
                                         os_time_ms_to_ticks32(MYNEWT_VAL(MAX3107_LOCK_TIMEOUT))));
}

static inline void
max3107_unlock(struct max3107_dev *dev)
{
    int rc = os_mutex_release(&dev->lock);
    assert(rc == 0);
}

void
max3107_cs_activate(struct max3107_dev *dev)
{
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_spi_node *node = (struct bus_spi_node *)&dev->dev;
    hal_gpio_write(node->pin_cs, 0);
#else
    hal_gpio_write(dev->cfg.ss_pin, 0);
#endif
}

void
max3107_cs_deactivate(struct max3107_dev *dev)
{
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_spi_node *node = &dev->dev;
    hal_gpio_write(node->pin_cs, 1);
#else
    hal_gpio_write(dev->cfg.ss_pin, 1);
#endif
}

int
max3107_read_regs(struct max3107_dev *dev, uint8_t addr, void *buf, uint32_t size)
{
    int rc;
    bool locked;
#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    uint8_t fast_buf[8];
#endif

    rc = max3107_lock(dev);
    locked = rc == 0;

    if (size > 0 && locked) {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
        rc = bus_node_simple_write_read_transact((struct os_dev *)&dev->dev,
                                                 &addr, 1, buf, size);
#else
        max3107_cs_activate(dev);

        if (size < sizeof(fast_buf)) {
            fast_buf[0] = addr;
            memset(fast_buf + 1, 0xFF, size);
            /* Send command + address */
            rc = hal_spi_txrx(dev->cfg.spi_num, fast_buf, fast_buf, size + 1);
            if (rc == 0) {
                memcpy(buf, fast_buf + 1, size);
            }
        } else {
            /* Send command + address */
            rc = hal_spi_txrx(dev->cfg.spi_num, &addr, NULL, 1);
            if (rc == 0) {
                /* For security mostly, do not output random data, fill it with FF */
                memset(buf, 0xFF, size);
                /* Tx buf does not matter, for simplicity pass read buffer */
                rc = hal_spi_txrx(dev->cfg.spi_num, buf, buf, (int)size);
            }
        }

        max3107_cs_deactivate(dev);
#endif
    }

    if (locked) {
        max3107_unlock(dev);
    }

    return rc;
}

int
max3107_write_regs(struct max3107_dev *dev, uint8_t addr, const uint8_t *buf, uint32_t size)
{
    int rc;
    bool locked;
    uint8_t fast_buf[17];

    rc = max3107_lock(dev);
    locked = rc == 0;

    if (!locked) {
        MAX3107_STATS_INC(dev->stats, lock_timeouts);
    }

    if (size && locked) {
        /* For writes set MSB */
        addr |= 0x80;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
        rc = bus_node_lock((struct os_dev *)&dev->dev,
                           BUS_NODE_LOCK_DEFAULT_TIMEOUT);
        if (size < sizeof(fast_buf)) {
            fast_buf[0] = addr;
            memcpy(fast_buf + 1, buf, size);
            rc = bus_node_simple_write((struct os_dev *)&dev->dev, fast_buf, size + 1);
        } else {
            if (rc == 0) {
                rc = bus_node_write((struct os_dev *)&dev->dev,
                                    &addr, 1,
                                    BUS_NODE_LOCK_DEFAULT_TIMEOUT, BUS_F_NOSTOP);
                if (rc == 0) {
                    rc = bus_node_simple_write((struct os_dev *)&dev->dev, buf, size);
                }
            }
        }
        (void)bus_node_unlock((struct os_dev *)&dev->dev);
#else
        max3107_cs_activate(dev);
        if (size < sizeof(fast_buf)) {
            fast_buf[0] = addr;
            memcpy(fast_buf + 1, buf, size);
            rc = hal_spi_txrx(dev->cfg.spi_num, fast_buf, NULL, size + 1);
        } else {
            rc = hal_spi_txrx(dev->cfg.spi_num, &addr, NULL, 1);
            if (rc == 0) {
                rc = hal_spi_txrx(dev->cfg.spi_num, (void *)buf, NULL, (int)size);
            }
        }
        max3107_cs_deactivate(dev);
#endif
    }
    if (locked) {
        max3107_unlock(dev);
    }

    return rc;
}

static int
max3107_write_reg(struct max3107_dev *dev, uint8_t addr, uint8_t val)
{
    return max3107_write_regs(dev, addr, &val, 1);
}

static int
max3107_write_fifo(struct max3107_dev *dev, const uint8_t *buf, uint32_t size)
{
    return max3107_write_regs(dev, 0, buf, size);
}

static int
max3107_read_fifo(struct max3107_dev *dev, uint8_t *buf, uint32_t size)
{
    return max3107_read_regs(dev, 0, buf, size);
}

static const uint8_t factors[5] = { 1, 6, 48, 96, 144 };
/* "F PLL in" from datasheet: Table 4. PLLFactor[1:0] Selection Guide */
static const uint32_t fpllin_min[5] = { 1, 500000,  850000,  425000, 390000 };
static const uint32_t fpllin_max[5] = { 1, 800000, 1200000, 1000000, 666666 };

static uint32_t
max3107_compute_clock_config(uint32_t clockf, uint32_t br, struct max3107_clock_cfg *cfg)
{
    uint32_t div;
    uint32_t pre_div;
    uint32_t pre_div_min;
    uint32_t pre_div_max;
    uint32_t mul;
    uint32_t actual_br;
    uint32_t best_br = 1;
    uint32_t fref;
    int factor_ix;
    int max_factor = (cfg->clk_source & CLCSOURCE_PLLBYPASS) ? 1 : 5;
    uint32_t mode_mul = (cfg->brg_config & BRGCONFIG_4XMODE) ? 4 : (cfg->brg_config & BRGCONFIG_2XMODE) ? 2 : 1;

    /* Clock will be needed. */
    cfg->clk_source |= CLCSOURCE_CLOCKEN;

    /* If bypass was not disable at the start try finding more accurate clock settings. */
    for (factor_ix = 0; factor_ix < max_factor; ++factor_ix) {
        /*
         * Pre divider does not apply when PLL is not used. Set lower and upper
         * limits to 1 to have same code for PLL/non PLL.
         */
        if (factor_ix == 0) {
            pre_div_min = 1;
            pre_div_max = 1;
        } else {
            /* Lower and upper frequency limits used to get pre divider range */
            pre_div_max = clockf / fpllin_min[factor_ix];
            pre_div_min = (clockf + fpllin_max[factor_ix] - 1) / fpllin_max[factor_ix];
            /* Make sure divider is in correct range. */
            pre_div_min = min(63, pre_div_min);
            pre_div_min = max(1, pre_div_min);
            pre_div_max = min(63, pre_div_max);
            pre_div_max = max(1, pre_div_max);
        }
        /* Loop for x1, x2 and x4 modes. */
        for (mul = 1; mul <= mode_mul; mul <<= 1) {
            for (pre_div = pre_div_min; pre_div <= pre_div_max; ++pre_div) {
                fref = (clockf / pre_div) * factors[factor_ix];
                div = (fref * mul + (br / 2)) / br;
                div = max(div, 16);

                actual_br = mul * fref / div;
                if (abs((int)(actual_br - br)) < abs((int)(best_br - br))) {
                    best_br = actual_br;
                    cfg->div_lsb = (uint8_t)(div >> 4);
                    cfg->div_msb = (uint8_t)(div >> 12);
                    cfg->brg_config = (div & 0xF) | ((mul == 4) ? 0x20 : ((mul == 2) ? 0x10 : 0));
                    /* If best choice is factor_ix == 0, no need for PLL */
                    if (factor_ix == 0) {
                        cfg->clk_source |= CLCSOURCE_PLLBYPASS;
                        cfg->clk_source &= ~CLCSOURCE_PLLEN;
                    } else {
                        cfg->pll_config = pre_div | ((factor_ix - 1) << 6);
                        cfg->clk_source &= ~CLCSOURCE_PLLBYPASS;
                        cfg->clk_source |= CLCSOURCE_PLLEN;
                    }
                }
            }
        }
    }

    return best_br;
}

uint32_t
max3107_get_real_baudrate(struct max3107_dev *dev)
{
    return dev->real_baudrate;
}

int
max3107_config_uart(struct max3107_dev *dev, const struct uart_conf_port *conf)
{
    int rc;

    if (dev->cfg.crystal_en) {
        dev->regs.clock.clk_source |= CLCSOURCE_CRYSTALEN;
    } else {
        dev->regs.clock.clk_source &= ~CLCSOURCE_CRYSTALEN;
    }
    if (dev->cfg.no_pll) {
        dev->regs.clock.clk_source |= CLCSOURCE_PLLBYPASS;
    } else {
        dev->regs.clock.clk_source &= ~CLCSOURCE_PLLBYPASS;
    }
    if (dev->cfg.allow_mul_4) {
        dev->regs.clock.brg_config = BRGCONFIG_4XMODE;
    } else if (dev->cfg.allow_mul_2) {
        dev->regs.clock.brg_config = BRGCONFIG_2XMODE;
    } else {
        dev->regs.clock.brg_config = 0;
    }
    dev->real_baudrate = max3107_compute_clock_config(dev->cfg.osc_freq, conf->uc_speed, &dev->regs.clock);

    rc = max3107_write_regs(dev, MAX3107_REG_PLLCONFIG, &dev->regs.clock.pll_config, 5);
    if (rc) {
        goto end;
    }

    dev->regs.modes.mode1 = MODE1_IRQSEL | (conf->uc_flow_ctl ? 0 : MODE1_RTSHIZ);
    dev->regs.modes.mode2 = 0;
    dev->regs.modes.lcr = (dev->uart.ud_conf_port.uc_stopbits == 1 ? 0 : LCR_STOPBITS) |
                          (dev->uart.ud_conf_port.uc_databits - 5) |
                          ((dev->uart.ud_conf_port.uc_parity != UART_PARITY_NONE) ? LCR_PARITYEN : 0 ) |
                          ((dev->uart.ud_conf_port.uc_parity == UART_PARITY_EVEN) ? LCR_EVENPARITY : 0);
    dev->regs.modes.rxtimeout = 2;
    dev->regs.modes.hdplxdelay = 0;
    dev->regs.modes.irda = 0;

    rc = max3107_write_regs(dev, MAX3107_REG_MODE1, &dev->regs.modes.mode1, 6);
    if (rc) {
        goto end;
    }

    /* RTS activated at FIFO level 120. */
    dev->regs.fifo.flow_lvl = 0xFF;
    dev->regs.fifo.fifo_trg_lvl = (dev->cfg.rx_trigger_level >> 3) << 4 | (dev->cfg.tx_trigger_level >> 3);
    rc = max3107_write_regs(dev, MAX3107_REG_FLOWLVL, &dev->regs.fifo.flow_lvl, 2);
    if (rc) {
        goto end;
    }

    dev->regs.flow.flow_ctrl = conf->uc_flow_ctl ? (FLOWCTRL_AUTOCTS | FLOWCTRL_AUTORTS) : 0;
    rc = max3107_write_reg(dev, MAX3107_REG_FLOWCTRL, dev->regs.flow.flow_ctrl);
    if (rc) {
        goto end;
    }

    dev->regs.ints.irq_en = IRQEN_LSRERRIEN | IRQEN_RXTRGIEN;
    dev->regs.ints.lsr_int_en = LSRINTEN_FRAMEERRIEN | LSRINTEN_PARITYIEN |
                                LSRINTEN_RBREAKIEN | LSRINTEN_ROVERRIEN | LSRINTEN_RTIMEOUTIEN;
    rc = max3107_write_regs(dev, MAX3107_REG_IRQEN, &dev->regs.ints.irq_en, 3);
end:
    return rc;
}

int
max3107_rx_available(struct max3107_dev *dev)
{
    int rc = 0;

    if (dev->regs.fifo.rx_fifo_lvl == 0) {
        rc = max3107_read_regs(dev, MAX3107_REG_RXFIFOLVL, &dev->regs.fifo.rx_fifo_lvl, 1);
    }
    if (rc == 0) {
        rc = dev->regs.fifo.rx_fifo_lvl;
    }

    return rc;
}

int
max3107_tx_available(struct max3107_dev *dev)
{
    int rc = 0;

    if (dev->regs.fifo.tx_fifo_lvl >= 128) {
        rc = max3107_read_regs(dev, MAX3107_REG_TXFIFOLVL, &dev->regs.fifo.tx_fifo_lvl, 1);
    }
    if (rc == 0) {
        rc = 128 - dev->regs.fifo.tx_fifo_lvl;
    }

    return rc;
}

static int
max3107_irqen_set(struct max3107_dev *dev, uint8_t enabled_bits)
{
    int rc = 0;

    uint8_t irq_en = dev->regs.ints.irq_en | enabled_bits;

    if (irq_en != dev->regs.ints.irq_en) {
        dev->regs.ints.irq_en = irq_en;
        rc = max3107_write_reg(dev, MAX3107_REG_IRQEN, dev->regs.ints.irq_en);
    }

    return rc;
}

static int
max3107_irqen_clear(struct max3107_dev *dev, uint8_t cleared_bits)
{
    int rc = 0;

    uint8_t irq_en = dev->regs.ints.irq_en & ~cleared_bits;

    if (irq_en != dev->regs.ints.irq_en) {
        dev->regs.ints.irq_en = irq_en;
        rc = max3107_write_reg(dev, MAX3107_REG_IRQEN, dev->regs.ints.irq_en);
    }

    return rc;
}

int
max3107_read(struct max3107_dev *dev, void *buf, size_t size)
{
    int rc = 0;

    MAX3107_STATS_INC(dev->stats, uart_read_ops);

    if (dev->regs.fifo.rx_fifo_lvl == 0) {
        /* Read number of bytes currently in RX FIFO first. */
        rc = max3107_read_regs(dev, MAX3107_REG_RXFIFOLVL, &dev->regs.fifo.rx_fifo_lvl, 1);
    }

    if (rc == 0) {
        size = min(dev->regs.fifo.rx_fifo_lvl, size);
        if (size > 0) {
            rc = max3107_read_fifo(dev, buf, size);
        }
        if (rc == 0) {
            MAX3107_STATS_INCN(dev->stats, uart_read_bytes, size);
            dev->readable_notified = false;
            /* Update FIFO level for potential future use. */
            dev->regs.fifo.rx_fifo_lvl -= size;
            /*
             * If FIFO was possible emptied, read FIFO level in next interrupt,
             * in case data was coming while FIFO was being read.
             */
            dev->recheck_rx_fifo_level = dev->regs.fifo.rx_fifo_lvl == 0;
            rc = (int)size;
        } else {
            MAX3107_STATS_INC(dev->stats, uart_read_errors);
        }
    }
    return rc;
}

int
max3107_write(struct max3107_dev *dev, const void *buf, size_t size)
{
    size_t fifo_space = 128 - dev->regs.fifo.tx_fifo_lvl;
    int rc = 0;

    MAX3107_STATS_INC(dev->stats, uart_write_ops);
    /*
     * If FIFO level was read before and there is enough data, there is
     * no need to check FIFO level again.
     */
    if (size > fifo_space) {
        /* Read number of bytes currently in TX FIFO */
        rc = max3107_read_regs(dev, MAX3107_REG_TXFIFOLVL, &dev->regs.fifo.tx_fifo_lvl, 1);
        fifo_space = 128 - dev->regs.fifo.tx_fifo_lvl;
    }
    if (rc == 0) {
        size = min(size, fifo_space);
        if (size) {
            MAX3107_STATS_INCN(dev->stats, uart_write_bytes, size);
            rc = max3107_write_fifo(dev, buf, size);
            dev->regs.fifo.tx_fifo_lvl += size;
            dev->writable_notified = false;
            if (rc) {
                MAX3107_STATS_INC(dev->stats, uart_write_errors);
            }
        }
        if (rc == 0) {
            rc = (int)size;
        }
    }

    return rc;
}

static void
max3107_isr_cb(struct max3107_dev *dev)
{
    int rc;
    int sr;
    bool read_irq;
    uint8_t isr = dev->regs.ints.isr;

    OS_ENTER_CRITICAL(sr);
    read_irq = dev->irq_pending;
    dev->irq_pending = false;
    OS_EXIT_CRITICAL(sr);

    if (read_irq) {
        /* Read ISR, LSR, (and also LSRIntEn which is between them) */
        rc = max3107_read_regs(dev, MAX3107_REG_ISR, &dev->regs.ints.isr, 3);
        if (rc) {
            dev->irq_pending = true;
            goto end;
        } else {
            /*
             * In usual case reading ISR clears interrupt status bits on the device.
             * However it's possible that interrupt raised during read of interrupt registers.
             * In that case irq pin will remain low. MCU interrupt is set to be triggered on
             * falling edge, read irq pin, if it stays low, new interrupt just arrived.
             */
            if (hal_gpio_read(dev->cfg.irq_pin) == 0) {
                dev->irq_pending = true;
            }
        }
    }

    if (dev->regs.ints.lsr & LSR_RXBREAK) {
        MAX3107_STATS_INC(dev->stats, uart_breaks);
        if (dev->client && dev->client->break_detected) {
            dev->client->break_detected(dev);
        }
    }
    if (dev->regs.ints.lsr & (LSR_RXERROOR)) {
        MAX3107_STATS_INC(dev->stats, uart_read_errors);
        if (dev->client && dev->client->error) {
            dev->client->error(dev, (max3107_error_t)(dev->regs.ints.lsr & LSR_RXERROOR));
        }
    }
    /* TX Empty interrupt, no need to read TX FIFO level */
    if ((0 != (dev->regs.ints.isr & ISR_TXEMPTYINT) &&
        (0 == (dev->regs.ints.isr & ISR_TFIFOTRIGINT)))) {
        dev->regs.fifo.tx_fifo_lvl = 0;
    } else if (0 != (isr & ISR_TFIFOTRIGINT) &&
               0 == (dev->regs.ints.isr & ISR_TFIFOTRIGINT) &&
               (dev->regs.fifo.fifo_trg_lvl & FIFOTRGLVL_TXTRIG) < (dev->regs.fifo.tx_fifo_lvl >> 3)) {
        dev->regs.fifo.tx_fifo_lvl = (dev->regs.fifo.fifo_trg_lvl & FIFOTRGLVL_TXTRIG) << 3;
    }
    /* RX Empty interrupt, no need to read RX FIFO level */
    if ((0 != (dev->regs.ints.isr & ISR_RXEMPTYINT)) &&
        (0 == (dev->regs.ints.isr & ISR_RFIFOTRIGINT))) {
        dev->regs.fifo.rx_fifo_lvl = 0;
        dev->recheck_rx_fifo_level = false;
    } else if (dev->regs.ints.isr & ISR_RFIFOTRIGINT) {
        /*
         * RX Trigger level interrupt, there are some bytes in FIFO. Update RX FIFO level from
         * known trigger level.
         */
        if (dev->regs.fifo.rx_fifo_lvl < ((dev->regs.fifo.fifo_trg_lvl & FIFOTRGLVL_RXTRIG) >> 1)) {
            dev->regs.fifo.rx_fifo_lvl = (dev->regs.fifo.fifo_trg_lvl & FIFOTRGLVL_RXTRIG) >> 1;
        }
    }
    /*
     * Read both FIFO levels when:
     * - RX timeout interrupt arrived, more RX related interrupts may never come
     * - TX FIFO level just went above trigger level, if transmission is ongoing
     *   tx_fifi_lvl is not updated by software so periodically read TX FIFO level
     *   instead waiting for FIFO empty interrupt.
     * - All know data from RX FIFO was read, there may be more bytes that arrived
     *   after FIFO level was checked, to avoid data overwrite (or timeout with
     *   flow control enabled) check FIFO level now.
     */
    if ((dev->regs.ints.lsr & LSR_RTIMEOUT) ||
        (dev->regs.fifo.tx_fifo_lvl > 64) ||
        dev->recheck_rx_fifo_level) {
        rc = max3107_read_regs(dev, MAX3107_REG_TXFIFOLVL, &dev->regs.fifo.tx_fifo_lvl, 2);
        if (rc == 0) {
            dev->recheck_rx_fifo_level = false;
        }
    }
    while (dev->regs.fifo.rx_fifo_lvl > 0 && dev->client && !dev->readable_notified) {
        dev->readable_notified = true;
        dev->client->readable(dev);
    }
    if (dev->regs.fifo.tx_fifo_lvl < 128) {
        if (dev->client && !dev->writable_notified) {
            dev->writable_notified = true;
            dev->client->writable(dev);
        }
    }
end:
    if (dev->irq_pending) {
        os_eventq_put(dev->event_queue, &dev->irq_event);
    }
}

static void
max3107_disable_rx_int(struct max3107_dev *dev)
{
    int rc = max3107_irqen_clear(dev, IRQEN_RXTRGIEN | IRQEN_LSRERRIEN);

    if ((rc != 0) && (dev->client != NULL) && (NULL != dev->client->error)) {
        dev->client->error(dev, MAX3107_ERROR_IO_FAILURE);
    }
}

static void
max3107_enable_rx_int(struct max3107_dev *dev)
{
    int rc = max3107_irqen_set(dev, IRQEN_RXTRGIEN | IRQEN_LSRERRIEN);

    if ((rc != 0) && (dev->client != NULL) && (NULL != dev->client->error)) {
        dev->client->error(dev, MAX3107_ERROR_IO_FAILURE);
    }
}

static void
max3107_disable_tx_int(struct max3107_dev *dev)
{
    int rc = max3107_irqen_clear(dev, IRQEN_TXEMTYIEN | IRQEN_TXTRGIEN);

    if ((rc != 0) && (dev->client != NULL) && (NULL != dev->client->error)) {
        dev->client->error(dev, MAX3107_ERROR_IO_FAILURE);
    }
}

static void
max3107_enable_tx_int(struct max3107_dev *dev)
{
    int rc = max3107_irqen_set(dev, IRQEN_TXEMTYIEN | IRQEN_TXTRGIEN);

    if ((rc != 0) && (dev->client != NULL) && (NULL != dev->client->error)) {
        dev->client->error(dev, MAX3107_ERROR_IO_FAILURE);
    }
}

static int
max3107_drain_rx_buffer(struct max3107_dev *dev)
{
    int i;

    for (i = 0; i < dev->rx_buf_count; ++i) {
        if (dev->uc_rx_char(dev->uc_cb_arg, dev->rx_buf[i]) < 0) {
            dev->readable_notified = true;
            if (i) {
                memmove(dev->rx_buf, dev->rx_buf + i, dev->rx_buf_count - i);
            }
            break;
        }
    }
    dev->rx_buf_count -= i;

    return i;
}

static int
max3107_drain_tx_cache(struct max3107_dev *dev)
{
    int rc = 0;

    if (dev->tx_buf_count) {
        rc = max3107_write(dev, dev->tx_buf, dev->tx_buf_count);
        if (rc > 0) {
            dev->tx_buf_count -= rc;
            if (dev->tx_buf_count) {
                memmove(dev->tx_buf, dev->tx_buf + rc, dev->tx_buf_count);
            }
        }
    }

    return rc;
}

int
max3107_set_client(struct max3107_dev *dev, const struct max3107_client *client)
{
    hal_gpio_irq_disable(dev->cfg.irq_pin);

    dev->client = client;

    if (client) {
        hal_gpio_irq_enable(dev->cfg.irq_pin);
        max3107_enable_rx_int(dev);
        max3107_enable_tx_int(dev);
        max3107_isr_cb(dev);
    } else {
        max3107_disable_rx_int(dev);
        max3107_disable_tx_int(dev);
    }

    return 0;
}

static void
max3107_isr_event_cb(struct os_event *event)
{
    max3107_isr_cb((struct max3107_dev *)event->ev_arg);
}

static void
max3107_isr(void *arg)
{
    struct max3107_dev *dev = arg;

    dev->irq_pending = true;
    os_eventq_put(dev->event_queue, &dev->irq_event);
}

static void
max3107_init_int(struct max3107_dev *dev)
{
    if (dev->cfg.irq_pin >= 0) {
        dev->event_queue = os_eventq_dflt_get();
        dev->irq_event.ev_cb = max3107_isr_event_cb;
        dev->irq_event.ev_arg = dev;

        hal_gpio_irq_init(dev->cfg.irq_pin, max3107_isr, dev, HAL_GPIO_TRIG_FALLING, HAL_GPIO_PULL_UP);
    }
}

static int
max3107_dev_open(struct max3107_dev *dev)
{
    uint8_t rev_id;
    int rc;

    rc = max3107_read_regs(dev, MAX3107_REG_REVID, &rev_id, 1);
    if (rc) {
        goto end;
    }
    if (rev_id != 0xA1) {
        rc = SYS_ENODEV;
        goto end;
    }

    rc = max3107_read_regs(dev, MAX3107_REG_IRQEN, &dev->regs.ints.irq_en, 30);
    if (rc == 0) {
        rc = max3107_config_uart(dev, &dev->uart.ud_conf_port);
    }
end:
    return rc;
}

static void
max3107_dev_close(struct max3107_dev *dev)
{
    max3107_set_client(dev, NULL);
}

/*
 * Mynewt UART driver layer.
 * Functions needed to use max3107 as any other UART
 */

static void
max3107_uart_readable(struct max3107_dev *dev)
{
    int rc;
    int size;

    if (dev->uc_rx_char == NULL) {
        max3107_disable_rx_int(dev);
    } else {
        dev->readable_notified = false;
        max3107_drain_rx_buffer(dev);
        if (!dev->readable_notified) {
            size = min(MYNEWT_VAL(MAX3107_UART_RX_BUFFER_SIZE), dev->regs.fifo.rx_fifo_lvl);
            if (size) {
                rc = max3107_read(dev, dev->rx_buf, size);
                if (rc > 0) {
                    dev->rx_buf_count = (uint8_t)rc;
                    max3107_drain_rx_buffer(dev);
                }
            }
        }
    }
}

static bool
max3107_tx_cache_full(struct max3107_dev *dev)
{
    return dev->tx_buf_count >= sizeof(dev->tx_buf);
}

static void
max3107_uart_writable(struct max3107_dev *dev)
{
    int c;
    int sr;

    if (max3107_tx_cache_full(dev)) {
        max3107_drain_tx_cache(dev);
    }
    while (!max3107_tx_cache_full(dev)) {
        OS_ENTER_CRITICAL(sr);
        c = dev->uc_tx_char(dev->uc_cb_arg);
        OS_EXIT_CRITICAL(sr);
        if (c >= 0) {
            dev->tx_buf[dev->tx_buf_count++] = (uint8_t)c;
            if (max3107_tx_cache_full(dev)) {
                max3107_drain_tx_cache(dev);
            }
        } else {
            max3107_drain_tx_cache(dev);
            if (dev->tx_buf_count == 0 && 0) {
                max3107_irqen_clear(dev, IRQEN_TXEMTYIEN | IRQEN_TXTRGIEN);
            }
            dev->writable_notified = false;
            break;
        }
    }
}

static void
max3107_uart_start_rx(struct uart_dev *uart)
{
    struct max3107_dev *dev = (struct max3107_dev *)uart->ud_priv;
    bool resume_after_stall = dev->readable_notified;
    int rc;

    dev->readable_notified = false;
    max3107_drain_rx_buffer(dev);
    if (!dev->readable_notified) {
        if (resume_after_stall) {
            rc = max3107_read_regs(dev, MAX3107_REG_TXFIFOLVL, &dev->regs.fifo.tx_fifo_lvl, 2);
            if (rc) {
                /* Lets be pessimistic about FIFO level */
                dev->regs.fifo.rx_fifo_lvl = 0;
            }
        }
        max3107_enable_rx_int((struct max3107_dev *)uart->ud_priv);
        max3107_isr_cb(dev);
    }
}

static void
max3107_uart_start_tx(struct uart_dev *uart)
{
    struct max3107_dev *dev = (struct max3107_dev *)uart->ud_priv;

    max3107_enable_tx_int(dev);
    max3107_isr_cb(dev);
}

static void
max3107_uart_blocking_tx(struct uart_dev *uart, uint8_t c)
{
    int rc;
    struct max3107_dev *dev = (struct max3107_dev *)uart->ud_priv;

    while (1) {
        rc = max3107_read_regs(dev, MAX3107_REG_TXFIFOLVL, &dev->regs.fifo.tx_fifo_lvl, 1);
        if (rc != 0 || dev->regs.fifo.tx_fifo_lvl == 128) {
            os_time_delay(1);
        } else {
            rc = max3107_write_fifo(dev, &c, 1);
            if (rc == 0) {
                break;
            }
        }
    }
}

static const struct max3107_client max3107_uart_client = {
    .writable = max3107_uart_writable,
    .readable = max3107_uart_readable,
};

static int
max3107_uart_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct uart_dev *uart = (struct uart_dev *)odev;
    struct max3107_dev *dev = uart->ud_priv;
    struct max3107_dev *opened_dev;
    struct uart_conf *uc = (struct uart_conf *)arg;
    int rc;

    if (!uc) {
        rc = OS_EINVAL;
        goto end;
    }

    if (odev->od_flags & OS_DEV_F_STATUS_OPEN) {
        rc = OS_EBUSY;
        goto end;
    }

    if ((uc->uc_databits < 5) || (uc->uc_databits > 8) ||
        (uc->uc_stopbits < 1) || (uc->uc_stopbits > 2)) {
        rc = OS_INVALID_PARM;
        goto end;
    }

    if ((uc->uc_parity != UART_PARITY_NONE) &&
        (uc->uc_parity != UART_PARITY_ODD) &&
        (uc->uc_parity != UART_PARITY_EVEN)) {
        rc = OS_INVALID_PARM;
        goto end;
    }

    uart->ud_conf_port = *(struct uart_conf_port *)uc;

    /* Open max3107 device */
    opened_dev = (struct max3107_dev *)os_dev_open(((struct os_dev *)dev)->od_name, wait, NULL);
    if (opened_dev) {
        assert(opened_dev == dev);
        dev->uc_rx_char = uc->uc_rx_char;
        dev->uc_tx_char = uc->uc_tx_char;
        dev->uc_tx_done = uc->uc_tx_done;
        dev->uc_cb_arg = uc->uc_cb_arg;
        max3107_set_client(opened_dev, &max3107_uart_client);
        rc = SYS_EOK;
    } else {
        rc = SYS_ENODEV;
    }
end:
    return rc;
}

static int
max3107_uart_close(struct os_dev *odev)
{
    struct uart_dev *uart = (struct uart_dev *)odev;

    return os_dev_close(uart->ud_priv);
}

static int
max3107_uart_init_func(struct os_dev *odev, void *arg)
{
    struct uart_dev *dev = (struct uart_dev *)odev;

    (void)arg;

    OS_DEV_SETHANDLERS(odev, max3107_uart_open, max3107_uart_close);

    odev->od_handlers.od_suspend = NULL;
    odev->od_handlers.od_resume = NULL;

    dev->ud_funcs.uf_start_tx = max3107_uart_start_tx;
    dev->ud_funcs.uf_start_rx = max3107_uart_start_rx;
    dev->ud_funcs.uf_blocking_tx = max3107_uart_blocking_tx;

    return 0;
}

struct max3107_dev *
max3107_get_dev_from_uart(struct uart_dev *uart)
{
    return (struct max3107_dev *)uart->ud_priv;
}

struct max3107_dev *
max3107_open(const char *name, const struct max3107_client *client)
{
    struct max3107_dev *dev = (struct max3107_dev *)os_dev_open(name, 1000, NULL);
    if (dev && client) {
        max3107_set_client(dev, client);
    }

    return dev;
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void
max3107_init_node_cb(struct bus_node *node, void *arg)
{
    struct max3107_dev *dev = (struct max3107_dev *)node;

    (void)arg;

    max3107_init_int(dev);
}

void max3107_node_open(struct bus_node *node)
{
    max3107_dev_open((struct max3107_dev *)node);
}

void max3107_node_close(struct bus_node *node)
{
    max3107_dev_close((struct max3107_dev *)node);
}

int
max3107_dev_create_spi(struct max3107_dev *max3107_dev, const char *name, const char *uart_name,
                       const struct max3107_cfg *cfg, const struct uart_conf_port *uart_cfg)
{
    struct bus_node_callbacks cbs = {
        .init = max3107_init_node_cb,
        .open = max3107_node_open,
        .close = max3107_node_close,
    };
    int rc;

    max3107_dev->cfg = *cfg;
    max3107_dev->uart.ud_conf_port = *uart_cfg;
    max3107_dev->uart.ud_priv = max3107_dev;
    if (cfg->ldoen_pin >= 0) {
        hal_gpio_init_out(cfg->ldoen_pin, 0);
    }
#if MYNEWT_VAL(MAX3107_STATS)
    rc = stats_init_and_reg(STATS_HDR(max3107_dev->stats),
                            STATS_SIZE_INIT_PARMS(max3107_dev->stats, STATS_SIZE_32),
                            STATS_NAME_INIT_PARMS(max3107_stats_section),
                            name);
    assert(rc == 0);
#endif

    bus_node_set_callbacks((struct os_dev *)max3107_dev, &cbs);

    rc = bus_spi_node_create(name, &max3107_dev->dev, &cfg->node_cfg, NULL);
    if (rc == 0) {
        os_dev_create(&max3107_dev->uart.ud_dev, uart_name, OS_DEV_INIT_SECONDARY, 0,
                      max3107_uart_init_func, max3107_dev);
    }

    return rc;
}

#else

static int
max3107_open_handler(struct os_dev *odev, uint32_t timeout, void *arg)
{
    (void)timeout;
    (void)arg;

    return max3107_dev_open((struct max3107_dev *)odev);
}

static int
max3107_close_handler(struct os_dev *odev)
{
    max3107_dev_close((struct max3107_dev *)odev);

    return 0;
}

static int
max3107_init_func(struct os_dev *odev, void *arg)
{
    int rc;
    struct max3107_dev *dev = (struct max3107_dev *)odev;

    (void)odev;
    (void)arg;

    OS_DEV_SETHANDLERS(odev, max3107_open_handler, max3107_close_handler);

    hal_gpio_init_out(dev->cfg.ss_pin, 1);

    (void)hal_spi_disable(dev->cfg.spi_num);

    rc = hal_spi_config(dev->cfg.spi_num, &dev->cfg.spi_settings);
    if (rc) {
        goto end;
    }

    hal_spi_set_txrx_cb(dev->cfg.spi_num, NULL, NULL);
    rc = hal_spi_enable(dev->cfg.spi_num);

    max3107_init_int(dev);
end:
    return rc;
}

int
max3107_dev_create_spi(struct max3107_dev *max3107_dev, const char *name, const char *uart_name,
                       const struct max3107_cfg *cfg, const struct uart_conf_port *uart_cfg)
{
    int rc;

    max3107_dev->cfg = *cfg;
    max3107_dev->uart.ud_conf_port = *uart_cfg;
    max3107_dev->uart.ud_priv = max3107_dev;
    if (cfg->ldoen_pin >= 0) {
        hal_gpio_init_out(cfg->ldoen_pin, 0);
    }
#if MYNEWT_VAL(MAX3107_STATS)
    rc = stats_init_and_reg(STATS_HDR(max3107_dev->stats),
                            STATS_SIZE_INIT_PARMS(max3107_dev->stats, STATS_SIZE_32),
                            STATS_NAME_INIT_PARMS(max3107_stats_section),
                            name);
    assert(rc == 0);
#endif

    rc = os_dev_create(&max3107_dev->dev, name, OS_DEV_INIT_SECONDARY, 0, max3107_init_func, NULL);
    if (rc == 0) {
        os_dev_create(&max3107_dev->uart.ud_dev, uart_name, OS_DEV_INIT_SECONDARY, 0,
                      max3107_uart_init_func, max3107_dev);
    }

    return rc;
}

#endif
