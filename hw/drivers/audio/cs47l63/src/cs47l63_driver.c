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
#include <stdint.h>
#include <stdlib.h>

#include <os/os_cputime.h>
#include <hal/hal_gpio.h>
#include <modlog/modlog.h>

#include <cs47l63/cs47l63_driver.h>
#include <bsp_driver_if.h>
#include <bus/drivers/spi_hal.h>
#include <cs47l63.h>

static void irq_event_cb(struct os_event *);

/*
 * Empty table, table is required by Cirrus driver but initialization
 * is done separately so table is left empty
 */
uint32_t cs47l63_syscfg_regs[] = {};

/*
 * SDK can call bsp_disable_irq() several times.
 * There is no parameter so global sr value will be kept
 * to be used on last call to bsp_enable_irq()
 */
static os_sr_t cs47l63_sr;
static uint8_t cs47l63_sr_irq_nest_cnt;

struct cs47l63_dev cs47l63_0_dev = {
    .irq_event = {
        .ev_cb = irq_event_cb,
        .ev_arg = NULL,
    }
};

static uint32_t
bsp_disable_irq(void)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    if (cs47l63_sr_irq_nest_cnt++ == 0) {
        cs47l63_sr = sr;
    }

    return BSP_STATUS_OK;
}

static uint32_t
bsp_enable_irq(void)
{
    if (--cs47l63_sr_irq_nest_cnt == 0) {
        OS_EXIT_CRITICAL(cs47l63_sr);
    }

    return BSP_STATUS_OK;
}

static uint32_t
bsp_i2c_db_write(uint32_t bsp_dev_id,
                 uint8_t *write_buffer_0,
                 uint32_t write_length_0,
                 uint8_t *write_buffer_1,
                 uint32_t write_length_1,
                 bsp_callback_t cb,
                 void *cb_ar)
{
    return BSP_STATUS_FAIL;
}

static uint32_t
bsp_i2c_write(uint32_t bsp_dev_id,
              uint8_t *write_buffer,
              uint32_t write_length,
              bsp_callback_t cb,
              void *cb_arg)
{
    return BSP_STATUS_FAIL;
}

static uint32_t
bsp_i2c_read_repeated_start(uint32_t bsp_dev_id,
                            uint8_t *write_buffer,
                            uint32_t write_length,
                            uint8_t *read_buffer,
                            uint32_t read_length,
                            bsp_callback_t cb,
                            void *cb_arg)
{
    return BSP_STATUS_FAIL;
}

static uint32_t
bsp_i2c_reset(uint32_t bsp_dev_id, bool *was_i2c_busy)
{
    return BSP_STATUS_FAIL;
}

static void
irq_event_cb(struct os_event *event)
{
    int ret;

    while (cs47l63_0_dev.pending_irq_cnt) {
        atomic_fetch_sub(&cs47l63_0_dev.pending_irq_cnt, 1);
        ret = cs47l63_process((cs47l63_t *)&cs47l63_0_dev.cs47l63);
        (void)ret;
    }
}

static bsp_callback_t bsp_irq_cb;

static void
bsp_irq_wrapper(void *arg)
{
    atomic_fetch_add(&cs47l63_0_dev.pending_irq_cnt, 1);
    bsp_irq_cb(BSP_STATUS_OK, arg);
    os_eventq_put(os_eventq_dflt_get(), &cs47l63_0_dev.irq_event);
}

static uint32_t
bsp_register_gpio_cb(uint32_t gpio_id, bsp_callback_t cb, void *cb_arg)
{
    hal_gpio_irq_init(gpio_id, bsp_irq_wrapper, cb_arg, HAL_GPIO_TRIG_FALLING, HAL_GPIO_PULL_NONE);
    bsp_irq_cb = cb;

    return BSP_STATUS_OK;
}

static uint32_t
bsp_set_gpio(uint32_t gpio_id, uint8_t gpio_state)
{
    hal_gpio_write(gpio_id, gpio_state == BSP_GPIO_HIGH ? 1 : 0);

    return BSP_STATUS_OK;
}

static uint32_t
bsp_set_supply(uint32_t supply_id, uint8_t supply_state)
{
    return BSP_STATUS_OK;
}

static struct hal_timer bsp_timer;
static bsp_callback_t bsp_timer_cb;

static void
bsp_timer_wrapper(void *arg)
{
    bsp_timer.cb_func = NULL;
    bsp_timer_cb(BSP_STATUS_OK, arg);
}


static uint32_t
bsp_set_timer(uint32_t duration_ms, bsp_callback_t cb, void *cb_arg)
{
    if (cb == NULL) {
        os_cputime_delay_usecs(duration_ms * 1000);
    } else {
        /* If not NULL timer is already running */
        assert(bsp_timer.cb_func == NULL);

        bsp_timer_cb = cb;

        os_cputime_timer_init(&bsp_timer, bsp_timer_wrapper, cb_arg);
        os_cputime_timer_relative(&bsp_timer, duration_ms * 1000);
    }
    return BSP_STATUS_OK;
}

static uint32_t
bsp_spi_write(uint32_t bsp_dev_id,
              uint8_t *addr_buffer,
              uint32_t addr_length,
              uint8_t *data_buffer,
              uint32_t data_length,
              uint32_t pad_len)
{
    int rc;
    uint8_t pad[pad_len];

    memset(pad, 0, pad_len);

    (void)bus_node_write((struct os_dev *)bsp_dev_id, addr_buffer, addr_length, 100, BUS_F_NOSTOP);
    (void)bus_node_write((struct os_dev *)bsp_dev_id, pad, pad_len, 100, BUS_F_NOSTOP);
    rc = bus_node_simple_write((struct os_dev *)bsp_dev_id, data_buffer, data_length);

    if (rc) {
        return BSP_STATUS_FAIL;
    } else {
        return BSP_STATUS_OK;
    }
}

static uint32_t
bsp_spi_read(uint32_t bsp_dev_id,
             uint8_t *addr_buffer,
             uint32_t addr_length,
             uint8_t *data_buffer,
             uint32_t data_length,
             uint32_t pad_len)
{
    int rc;
    uint8_t write_buf[addr_length + pad_len];

    memcpy(write_buf, addr_buffer, addr_length);
    memset(write_buf + addr_length, 0, pad_len);
    rc = bus_node_simple_write_read_transact((struct os_dev *)bsp_dev_id,
                                             write_buf, addr_length + pad_len,
                                             data_buffer, data_length);

    if (rc) {
        return BSP_STATUS_FAIL;
    } else {
        return BSP_STATUS_OK;
    }
}

static uint32_t
bsp_spi_throttle_speed(uint32_t speed_hz)
{
    return BSP_STATUS_OK;
}

static uint32_t
bsp_spi_restore_speed(void)
{
    return BSP_STATUS_OK;
}

static bsp_driver_if_t bsp_driver_if = {
    .set_gpio = bsp_set_gpio,
    .set_supply = bsp_set_supply,
    .register_gpio_cb = bsp_register_gpio_cb,
    .set_timer = bsp_set_timer,
    .i2c_reset = bsp_i2c_reset,
    .i2c_read_repeated_start = bsp_i2c_read_repeated_start,
    .i2c_write = bsp_i2c_write,
    .i2c_db_write = bsp_i2c_db_write,
    .spi_read = bsp_spi_read,
    .spi_write = bsp_spi_write,
    .disable_irq = bsp_disable_irq,
    .enable_irq = bsp_enable_irq,
    .spi_throttle_speed = bsp_spi_throttle_speed,
    .spi_restore_speed = bsp_spi_restore_speed,
};

bsp_driver_if_t *bsp_driver_if_g = &bsp_driver_if;

void
cs47l63_notification_callback(uint32_t event_flags, void *arg)
{
}

uint32_t
cs47l63_write_regs(struct cs47l63_dev *dev, const struct reg_val_pair regs[])
{
    int i;
    uint32_t rc = CS47L63_STATUS_OK;

    for (i = 0; rc == CS47L63_STATUS_OK && regs[i].reg != 0; ++i) {
        rc = cs47l63_write_reg(&dev->cs47l63, regs[i].reg, regs[i].val);
    }

    return rc;
}

uint32_t
cs47l63_reg_read(struct cs47l63_dev *dev, uint32_t reg, uint32_t *val)
{
    return cs47l63_read_reg(&dev->cs47l63, reg, val);
}

uint32_t
cs47l63_reg_write(struct cs47l63_dev *dev, uint32_t reg, uint32_t val)
{
    return cs47l63_write_reg(&dev->cs47l63, reg, val);
}

static void
cs47l63_init(struct bus_node *node, void *arg)
{
}

static void
cs47l63_open(struct bus_node *node)
{
    int rc;
    struct cs47l63_dev *dev = (struct cs47l63_dev *)node;

    /* Will pin reset the device and wait until boot done */
    rc = cs47l63_reset(&dev->cs47l63);
    if (rc != CS47L63_STATUS_OK) {
        assert(0);
        return;
    }

    if (dev->cs47l63.devid != CS47L63_DEVID_VAL) {
        CS47L63_LOG_ERROR("Wrong device id: 0x%02x, should be 0x%02x", (unsigned)dev->cs47l63.devid,
                          CS47L63_DEVID_VAL);
    }
}

static void
cs47l63_close(struct bus_node *node)
{
    struct cs47l63_dev *dev = (struct cs47l63_dev *)node;

    cs47l63_fll_disable(&dev->cs47l63, false);
}

int
cs47l63_create_dev(struct cs47l63_dev *dev, const char *name, const struct cs47l63_create_cfg *cfg)
{
    int rc;
    struct bus_node_callbacks cbs = {
        .init = cs47l63_init,
        .open = cs47l63_open,
        .close = cs47l63_close,
    };

    bus_node_set_callbacks((struct os_dev *)dev, &cbs);

    rc = bus_spi_node_create(name, &dev->spi_node, &cfg->spi_cfg, NULL);

    return rc;
}

#define FLD(n, v) ((v) << (n ## _SHIFT))

/* Set up GPIOs */
const struct reg_val_pair gpio_config[] = {
    /* Enable CODEC LED */
    { CS47L63_GPIO10_CTRL1, FLD(CS47L63_GP10_PU, 1) | FLD(CS47L63_GP10_DRV_STR, 1) | FLD(CS47L63_GP10_LVL, 1) |
      FLD(CS47L63_GP10_FN, 1) },
    /* FLL1 to GPIO9 */
    { CS47L63_GPIO9_CTRL1, FLD(CS47L63_GP9_DRV_STR, 0) | FLD(CS47L63_GP9_LVL, 1) | FLD(CS47L63_GP9_FN, 16) },
    { 0 },
};

/* Set up output */
const struct reg_val_pair output_config[] = {
    { CS47L63_OUTPUT_ENABLE_1, FLD(CS47L63_OUT1L_EN, 1) },

    /* OUT1L SRC - ASP1_RX1 at 0 dB */
    { CS47L63_OUT1L_INPUT1, FLD(CS47L63_OUT1LMIX_VOL1, 0x40) | FLD(CS47L63_OUT1L_SRC1, 0x20) },
    /* OUT1L SRC - ASP1_RX2 at 0 dB */
    { CS47L63_OUT1L_INPUT2, FLD(CS47L63_OUT1LMIX_VOL2, 0x40) | FLD(CS47L63_OUT1L_SRC2, 0x21) },
    { 0 },
};

uint32_t
cs47l63_fll_control(struct cs47l63_dev *dev, bool enable)
{
    return cs47l63_write_reg(&dev->cs47l63, CS47L63_FLL1_CONTROL1, FLD(CS47L63_FLL1_EN, enable ? 1 : 0));
}

uint32_t
cs47l63_mute_control(struct cs47l63_dev *dev, bool mute)
{
    if (mute) {
        dev->out1l_volume_1 |= FLD(CS47L63_OUT1L_MUTE, 1);
    } else {
        dev->out1l_volume_1 &= ~FLD(CS47L63_OUT1L_MUTE, 1);
    }
    return cs47l63_write_reg(&dev->cs47l63, CS47L63_OUT1L_VOLUME_1, dev->out1l_volume_1 | CS47L63_OUT_VU_MASK);
}

#define MAX_VOLUME_REG_VAL 0xBF

int
cs47l63_volume_get(struct cs47l63_dev *dev, int8_t *vol)
{
    *vol = (int8_t)((((int8_t)dev->out1l_volume_1) / 2) - 64);

    return 0;
}

int
cs47l63_volume_set(struct cs47l63_dev *dev, int8_t vol)
{
    int rc;
    int volume_val;

    CS47L63_LOG_DEBUG("Volume set %d\n", vol);

    /* Get adjustment in dB, 1 bit is 0.5 dB,
     * so multiply by 2 to get increments of 1 dB
     */
    volume_val = ((int)vol + 64) * 2;
    if (volume_val < 0) {
        volume_val = 0;
    } else if (volume_val > MAX_VOLUME_REG_VAL) {
        volume_val = MAX_VOLUME_REG_VAL;
    }

    rc = cs47l63_write_reg(&dev->cs47l63, CS47L63_OUT1L_VOLUME_1,
                           volume_val | CS47L63_OUT_VU);
    if (rc == 0) {
        dev->out1l_volume_1 = (int16_t)volume_val;
    }

    return rc;
}

int
cs47l63_volume_modify(struct cs47l63_dev *dev, int8_t vol_adj)
{
    int rc;
    int16_t out1l_volume_1 = dev->out1l_volume_1;
    int volume_val = out1l_volume_1 & CS47L63_OUT1L_VOL;

    /* Unmute */
    out1l_volume_1 &= ~CS47L63_OUT1L_MUTE;

    /* Get adjustment in dB, 1 bit is 0.5 dB,
     * so multiply by 2 to get increments of 1 dB
     */
    volume_val += 2 * vol_adj;
    if (volume_val < 0) {
        volume_val = 0;
    } else if (volume_val > MAX_VOLUME_REG_VAL) {
        volume_val = MAX_VOLUME_REG_VAL;
    }
    out1l_volume_1 &= ~(CS47L63_OUT1L_VOL_MASK | CS47L63_OUT1L_MUTE_MASK);
    out1l_volume_1 |= volume_val;

    rc = cs47l63_write_reg(&dev->cs47l63, CS47L63_OUT1L_VOLUME_1,
                           out1l_volume_1 | CS47L63_OUT_VU);
    if (rc == 0) {
        dev->out1l_volume_1 = out1l_volume_1;
    }
    CS47L63_LOG_DEBUG("Volume modify %+d dB (volume %d dB)\n", vol_adj, volume_val / 2 - 64);

    return rc;
}

/* Sample rate mapping for SAMPLE_RATE_n registers */
static const uint32_t sample_rates[][2] = {
    {  12000, 0x01 },
    {  24000, 0x02 },
    {  48000, 0x03 },
    {  96000, 0x04 },
    { 192000, 0x05 },
    {  11025, 0x09 },
    {  22050, 0x0A },
    {  44100, 0x0B },
    {  88200, 0x0C },
    { 176400, 0x0D },
    {   8000, 0x11 },
    {  16000, 0x12 },
    {  32000, 0x13 },
    {      0, 0x00 }
};

int
cs47l63_config_fll1_from_bclk(struct cs47l63_dev *dev, uint32_t sample_freq, uint32_t slot_bits, uint32_t slots)
{
    uint32_t src_freq = sample_freq * slot_bits * slots;
    uint32_t f_ref = src_freq;
    uint32_t ref_clk_div = 0;
    uint32_t fll_f = 49152000;
    uint32_t fll_fb_div = 1;
    uint32_t int_mode;
    uint32_t n_k = 1;
    uint32_t fll_control_fields;
    struct reg_val_pair clock_config[9];
    struct reg_val_pair *cfg = clock_config;
    int i;

    if (176400 / sample_freq * sample_freq == 176400) {
        fll_f = 45158400;
    }

    while (f_ref >= 13000000) {
        f_ref >>= 1;
        ref_clk_div++;
    }
    assert(ref_clk_div <= 3);
    int_mode = (fll_f / f_ref * f_ref == fll_f) ? 1 : 3;
    if (f_ref < 192000) {
        /* Low frequency, TODO: Implement if needed */
        fll_control_fields = FLD(CS47L63_FLL1_PD_GAIN_FINE, 2) | FLD(CS47L63_FLL1_PD_GAIN_COARSE, 3) |
                             FLD(CS47L63_FLL1_FD_GAIN_FINE, 0xF) | FLD(CS47L63_FLL1_FD_GAIN_COARSE, 0) |
                             FLD(CS47L63_FLL1_INTEG_DLY_MODE, 1);
        assert(0);
    } else if (f_ref < 1152000) {
        /* Mid frequency */
        fll_control_fields = FLD(CS47L63_FLL1_PD_GAIN_FINE, 2) | FLD(CS47L63_FLL1_PD_GAIN_COARSE, 2) |
                             FLD(CS47L63_FLL1_FD_GAIN_FINE, 0xF) | FLD(CS47L63_FLL1_FD_GAIN_COARSE, 2) |
                             FLD(CS47L63_FLL1_INTEG_DLY_MODE, 1);
        fll_fb_div = int_mode ? 2 : 16;
    } else {
        fll_control_fields = FLD(CS47L63_FLL1_PD_GAIN_FINE, 2) | FLD(CS47L63_FLL1_PD_GAIN_COARSE, 1) |
                             FLD(CS47L63_FLL1_FD_GAIN_FINE, 0xF) | FLD(CS47L63_FLL1_FD_GAIN_COARSE, 0) |
                             FLD(CS47L63_FLL1_INTEG_DLY_MODE, 1);
    }
    if (int_mode == 1) {
        for (;;) {
            n_k = fll_f / (fll_fb_div * f_ref);
            assert(n_k > 0);
            if (n_k > 1023) {
                fll_fb_div *= 2;
                continue;
            }
            break;
        }
    } else {
        /* TODO: fractional mode */
    }

    for (i = 0; i < ARRAY_SIZE(sample_rates); ++i) {
        if (sample_rates[i][0] == sample_freq) {
            break;
        }
    }
    if (i >= ARRAY_SIZE(sample_rates)) {
        assert(0);
        return -1;
    }
    *cfg++ = (struct reg_val_pair) { CS47L63_SYSTEM_CLOCK1,
                                     FLD(CS47L63_SYSCLK_FRAC, (fll_f != 49152000 ? 1 : 0)) |
                                     FLD(CS47L63_SYSCLK_FREQ, 3) | FLD(CS47L63_SYSCLK_EN, 1) |
                                     FLD(CS47L63_SYSCLK_SRC, 12) };
    *cfg++ = (struct reg_val_pair) { CS47L63_ASYNC_CLOCK1,
                                     FLD(CS47L63_ASYNC_CLK_FREQ, 3) | FLD(CS47L63_ASYNC_CLK_EN, 1) |
                                     FLD(CS47L63_ASYNC_CLK_SRC, 12) };
    *cfg++ = (struct reg_val_pair) { CS47L63_FLL1_CONTROL2,
                                     FLD(CS47L63_FLL1_LOCKDET_THR, 8) | FLD(CS47L63_FLL1_LOCKDET, 1) |
                                     FLD(CS47L63_FLL1_REFDET, 1) | FLD(CS47L63_FLL1_REFCLK_SRC, 8) |
                                     FLD(CS47L63_FLL1_REFCLK_DIV, ref_clk_div) | FLD(CS47L63_FLL1_N, n_k) };
    *cfg++ = (struct reg_val_pair) { CS47L63_FLL1_CONTROL3, FLD(CS47L63_FLL1_LAMBDA, 0) };
    *cfg++ = (struct reg_val_pair) { CS47L63_FLL1_CONTROL4,
                                     fll_control_fields | FLD(CS47L63_FLL1_FB_DIV, fll_fb_div) |
                                     FLD(CS47L63_FLL1_HP, int_mode) };
    *cfg++ = (struct reg_val_pair) { CS47L63_SAMPLE_RATE1,
                                     FLD(CS47L63_SAMPLE_RATE_1, sample_rates[i][1]) };
    /* Output FLL1 / 10 clock on GPIO */
    *cfg++ = (struct reg_val_pair) { CS47L63_FLL1_GPIO_CLOCK,
                                     FLD(CS47L63_FLL1_GPCLK_SRC, 0) | FLD(CS47L63_FLL1_GPCLK_DIV, 10) |
                                     FLD(CS47L63_FLL1_GPCLK_EN, 1) };
    *cfg++ = (struct reg_val_pair) { CS47L63_FLL1_CONTROL1,
                                     FLD(CS47L63_FLL1_CTRL_UPD, 1) | FLD(CS47L63_FLL1_EN, 1) };
    cfg->reg = 0;

    return cs47l63_write_regs(dev, clock_config);
}

static const uint32_t bclk_freq_for_asp[][2] = {
    {   128000, 0x0C },
    {   176400, 0x0D },
    {   192000, 0x0E },
    {   256000, 0x0F },
    {   352800, 0x10 },
    {   384000, 0x11 },
    {   512000, 0x12 },
    {   705600, 0x13 },
    {   768000, 0x15 },
    {  1024000, 0x17 },
    {  1411200, 0x19 },
    {  1536000, 0x1B },
    {  2048000, 0x1D },
    {  2882400, 0x1F },
    {  3072000, 0x21 },
    {  4096000, 0x24 },
    {  5644800, 0x26 },
    {  6144000, 0x28 },
    {  8192000, 0x2F },
    { 11289600, 0x31 },
    { 12288000, 0x33 },
    { 22579200, 0x39 },
    { 24576000, 0x3B },
};

static int
cs47l63_asp1_config(struct cs47l63_dev *dev, uint32_t sample_freq, uint32_t slot_bits, uint32_t slots)
{
    int i;
    uint32_t bclk = sample_freq * slot_bits * slots;
    struct reg_val_pair asp1_config[] = {
        { CS47L63_ASP1_CONTROL1, FLD(CS47L63_ASP1_RATE, 0) /* _BCLK_FREQ compute later */ },

        /* Enable ASP1 GPIOs */
        /* ASP1 DOUT */
        { CS47L63_GPIO1_CTRL1,
          FLD(CS47L63_GP1_PU, 1) | FLD(CS47L63_GP1_PD, 1) | FLD(CS47L63_GP10_DRV_STR, 1) },
        /* ASP1 DIN */
        { CS47L63_GPIO2_CTRL1,
          FLD(CS47L63_GP2_PU, 1) | FLD(CS47L63_GP2_PD, 1) },
        /* ASP1 BCLK */
        { CS47L63_GPIO3_CTRL1,
          FLD(CS47L63_GP3_PU, 1) | FLD(CS47L63_GP3_PD, 1) },
        /* ASP1 FSYNC/RLCLK */
        { CS47L63_GPIO4_CTRL1,
          FLD(CS47L63_GP4_PU, 1) | FLD(CS47L63_GP4_PD, 1) },

        /* FMT = I2S */
        { CS47L63_ASP1_CONTROL2,
          FLD(CS47L63_ASP1_RX_WIDTH, slot_bits) |
          FLD(CS47L63_ASP1_TX_WIDTH, slot_bits) |
          FLD(CS47L63_ASP1_FMT, 2) },
        /* DOUT 0 on DOUT in unused slots */
        { CS47L63_ASP1_CONTROL3, 0x0000},
        /* Valid bits per TX slot */
        { CS47L63_ASP1_DATA_CONTROL1,
          FLD(CS47L63_ASP1_TX_WL, slot_bits) },
        /* Valid bits per RX slot */
        { CS47L63_ASP1_DATA_CONTROL5,
          FLD(CS47L63_ASP1_RX_WL, slot_bits) },
        /* Chanel enable  */
        { CS47L63_ASP1_ENABLES1,
          FLD(CS47L63_ASP1_TX1_EN, 1) | FLD(CS47L63_ASP1_TX2_EN, 1) |
          FLD(CS47L63_ASP1_RX1_EN, 1) | FLD(CS47L63_ASP1_RX2_EN, 1) },
        {0},
    };
    /* Search for requested BCLK */
    for (i = 0; i < ARRAY_SIZE(bclk_freq_for_asp); ++i) {
        if (bclk_freq_for_asp[i][0] == bclk) {
            break;
        }
    }
    if (i >= ARRAY_SIZE(bclk_freq_for_asp)) {
        /* BCLK value not supported */
        assert(0);
        return -1;
    }
    CS47L63_LOG_INFO("ASP1 LRCLK=%d BCLK=%d\n", sample_freq, bclk);
    asp1_config[0].val |= FLD(CS47L63_ASP1_BCLK_FREQ, bclk_freq_for_asp[i][1]);
    return cs47l63_write_regs(dev, asp1_config);
}

int
cs47l63_start_i2s(struct cs47l63_dev *dev, uint32_t sample_rate, uint32_t slot_bits)
{
    uint32_t rc;

    rc = cs47l63_config_fll1_from_bclk(dev, sample_rate, slot_bits, 2);
    if (rc) {
        goto end;
    }

    rc = cs47l63_write_regs(dev, gpio_config);
    if (rc) {
        goto end;
    }

    rc = cs47l63_asp1_config(dev, sample_rate, slot_bits, 2);
    if (rc) {
        goto end;
    }

    rc = cs47l63_write_regs(dev, output_config);
    if (rc) {
        goto end;
    }

    /* Toggle FLL to start up CS47L63 */
    rc = cs47l63_fll_control(dev, false);
    if (rc) {
        goto end;
    }
    os_cputime_delay_usecs(1000);
    rc = cs47l63_fll_control(dev, true);

    /* Unmute */
    rc = cs47l63_mute_control(dev, false);
    if (rc) {
        goto end;
    }
end:
    return (int)rc;
}

#if MYNEWT_VAL(CS47L63_0)
static cs47l63_config_t cs47l63_0_config = {
    .bsp_config = {
        .bsp_reset_gpio_id = MYNEWT_VAL(CS47L63_0_RESET_PIN),
        .bsp_int_gpio_id = MYNEWT_VAL(CS47L63_0_INT_PIN),
        .bsp_dcvdd_supply_id = 0,
        .cp_config.bus_type = REGMAP_BUS_TYPE_SPI,
        .cp_config.dev_id = 0,
        .cp_config.spi_pad_len = 4,
        .cp_config.receive_max = 10,
        .notification_cb = cs47l63_notification_callback,
    },
    .syscfg_regs = cs47l63_syscfg_regs,
    .syscfg_regs_total = CS47L63_SYSCFG_REGS_TOTAL * 0,
};

static struct cs47l63_create_cfg cs47l63_0_cfg = {
    .spi_cfg = {
        .node_cfg.bus_name = MYNEWT_VAL(CS47L63_0_BUS),
        .pin_cs = MYNEWT_VAL(CS47L63_0_SPI_CS_PIN),
        .mode = BUS_SPI_MODE_0,
        .data_order = HAL_SPI_MSB_FIRST,
        .freq = MYNEWT_VAL(CS47L63_0_SPI_CLK_KHZ),
    }
};

void
cs4763_0_dev_init(void)
{
    int rc;

    rc = cs47l63_create_dev(&cs47l63_0_dev, MYNEWT_VAL(CS47L63_0_NAME), &cs47l63_0_cfg);
    SYSINIT_PANIC_ASSERT(rc == 0);

    /* Initialize CS47L63 driver */
    rc = cs47l63_initialize(&cs47l63_0_dev.cs47l63);
    if (rc != CS47L63_STATUS_OK) {
        SYSINIT_PANIC_ASSERT(0);
        return;
    }

    cs47l63_0_config.bsp_config.cp_config.dev_id = (uint32_t)&cs47l63_0_dev;
    cs47l63_0_dev.out1l_volume_1 = FLD(CS47L63_OUT1L_VOL, 128 + (MYNEWT_VAL(CS47L63_0_VOLUME) * 2));

    rc = cs47l63_configure(&cs47l63_0_dev.cs47l63, &cs47l63_0_config);
    if (rc != CS47L63_STATUS_OK) {
        SYSINIT_PANIC_ASSERT(0);
        return;
    }
}

#endif
