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

#include <os/mynewt.h>
#include <bsp/bsp.h>
#include <i2s/i2s.h>
#include <i2s/i2s_driver.h>
#include <i2s_da1469x/i2s_da1469x.h>
#include <mcu/da1469x_dma.h>
#include <mcu/da1469x_pd.h>

struct da1469x_i2s {
    struct i2s_cfg cfg;

    /* DMA register pairs 0,2 RX, 1,3 TX */
    struct da1469x_dma_regs *dma_regs[4];
    /* Currently active DMA buffer half. */
    uint8_t active_half;
    /* Number of buffers that driver has data from. */
    uint8_t full_buffer_count;

    bool running;

    struct i2s *i2s;
};

static struct da1469x_i2s da1469x_i2s;

static int
da1469x_i2s_init(struct i2s *i2s, const struct i2s_cfg *cfg)
{
    int rc;

    da1469x_i2s.i2s = i2s;
    da1469x_i2s.cfg = *cfg;

    mcu_gpio_set_pin_function(cfg->bclk_pin, MCU_GPIO_MODE_OUTPUT, MCU_GPIO_FUNC_PCM_CLK);
    mcu_gpio_set_pin_function(cfg->lrcl_pin, MCU_GPIO_MODE_OUTPUT, MCU_GPIO_FUNC_PCM_FSC);
    i2s->direction = I2S_INVALID;
    if (cfg->sdout_pin >= 0) {
        mcu_gpio_set_pin_function(cfg->sdout_pin, MCU_GPIO_MODE_OUTPUT, MCU_GPIO_FUNC_PCM_DO);
        i2s->direction = I2S_OUT;
    }
    if (cfg->sdin_pin >= 0) {
        mcu_gpio_set_pin_function(cfg->sdin_pin, MCU_GPIO_MODE_INPUT, MCU_GPIO_FUNC_PCM_DI);
        i2s->direction |= I2S_IN;
    }
    i2s->sample_size_in_bytes = cfg->sample_bits / 8;

    rc = i2s_init(i2s, cfg->pool);

    if (rc != OS_OK) {
        goto end;
    }

    i2s->sample_rate = cfg->sample_rate;
    i2s->driver_data = &da1469x_i2s;
end:
    return rc;
}

int
i2s_create(struct i2s *i2s, const char *name, const struct i2s_cfg *cfg)
{
    return os_dev_create(&i2s->dev, name, OS_DEV_INIT_PRIMARY,
                         100, (os_dev_init_func_t)da1469x_i2s_init, (void *)cfg);
}

int
i2s_driver_stop(struct i2s *i2s)
{
    struct da1469x_i2s *i2s_data = (struct da1469x_i2s *)i2s->driver_data;
    struct i2s_sample_buffer *buffer;

    if (i2s_data->running) {
        da1469x_i2s.dma_regs[0]->DMA_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DMA_ON_Msk;
        da1469x_i2s.dma_regs[1]->DMA_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DMA_ON_Msk;

        i2s_data->full_buffer_count = 0;
        while ((buffer = i2s_driver_buffer_get(i2s)) != NULL) {
            i2s_driver_buffer_put(i2s, buffer);
        }
        da1469x_pd_release_nowait(MCU_PD_DOMAIN_PER);
    }

    return 0;
}


static void
copy_and_swap_channels_16(const int16_t *lr, int16_t *rl, uint32_t count)
{
    const int16_t *limit = lr + count;
    while (lr < limit) {
        *rl++ = lr[1];
        *rl++ = lr[0];
        lr += 2;
    }
}

/* Split 16 bit samples to left and right data for DMA. */
static void
split_channels_16(const int16_t *lr, int16_t *l, int16_t *r, uint32_t count)
{
    const int16_t *limit = lr + count;

    while (lr < limit) {
        *l++ = *lr++;
        *r++ = *lr++;
    }
}

/* Split 16 bit samples to left and right data for DMA, extend to 32 bits left aligned. */
static void
split_channels_16_32(const int16_t *lr, int32_t *l, int32_t *r, uint32_t count)
{
    const int16_t *limit = lr + count;

    while (lr < limit) {
        *l++ = (*lr++) << 16;
        *r++ = (*lr++) << 16;
    }
}

/* Split 32 bit samples to left and right data for DMA. */
static void
split_channels_32_32(const int32_t *lr, int32_t *l, int32_t *r, uint32_t count)
{
    const int32_t *limit = lr + count;

    while (lr < limit) {
        *l++ = *lr++;
        *r++ = *lr++;
    }
}

static inline bool
tx_dma_is_active(void)
{
    return (da1469x_i2s.dma_regs[1]->DMA_CTRL_REG & DMA_DMA0_CTRL_REG_DMA_ON_Msk) != 0;
}

static void
da1469x_i2s_fill_dma_buffer(const void *buffer, uint32_t sample_count, uint32_t sample_size_in_bytes)
{
    uint32_t offset = 0;
    const int16_t *src = (const int16_t *)buffer;
    struct da1469x_dma_buffer *dma_mem = da1469x_i2s.cfg.dma_memory;
    uint8_t inactive_half = tx_dma_is_active() ? (da1469x_i2s.active_half ^ 1) : da1469x_i2s.full_buffer_count;

    if (da1469x_i2s.cfg.data_format == I2S_DATA_FRAME_16_16) {

        assert(sample_size_in_bytes == 2);
        assert(sample_count * sample_size_in_bytes == dma_mem->size);

        if (inactive_half != 0) {
            offset = dma_mem->size;
        }
        copy_and_swap_channels_16(src, (int16_t *)(dma_mem->buffer + offset), sample_count);
    } else if (da1469x_i2s.cfg.data_format == I2S_DATA_FRAME_16_32) {

        assert(sample_size_in_bytes == 2);
        assert(sample_count * sample_size_in_bytes == dma_mem->size);

        if (inactive_half != 0) {
            offset = dma_mem->size / 2;
        }
        split_channels_16(src, (int16_t *)(dma_mem->buffer + offset),
                          (int16_t *)(dma_mem->buffer + offset + dma_mem->size),
                          sample_count);
    } else if (da1469x_i2s.cfg.data_format == I2S_DATA_FRAME_32_32) {

        if (inactive_half != 0) {
            offset = dma_mem->size / 2;
        }
        if (sample_size_in_bytes == 2) {
            assert(sample_count * 4 == dma_mem->size);
            split_channels_16_32((const int16_t *)buffer, (int32_t *)(dma_mem->buffer + offset),
                                 (int32_t *)(dma_mem->buffer + offset + dma_mem->size),
                                 sample_count);
        } else {
            assert(sample_count * sample_size_in_bytes == dma_mem->size);

            split_channels_32_32((const int32_t *)buffer, (int32_t *)(dma_mem->buffer + offset),
                                 (int32_t *)(dma_mem->buffer + offset + dma_mem->size),
                                 sample_count);
        }
    }
    da1469x_i2s.full_buffer_count++;
}

static void
da1469x_i2s_dma_tx_start(void)
{
    uint32_t number_of_transfers;
    struct da1469x_dma_regs *lregs = da1469x_i2s.dma_regs[1];
    struct da1469x_dma_regs *rregs = da1469x_i2s.dma_regs[3];
    struct da1469x_dma_buffer *dma_mem = da1469x_i2s.cfg.dma_memory;

    da1469x_i2s.active_half = 0;

    if (da1469x_i2s.cfg.data_format == I2S_DATA_FRAME_32_32) {
        /* Left and right channels data are not interleaved, DMA transfer size 4B */
        number_of_transfers = (dma_mem->size >> 2) - 1;
    } else {
        /* Number of transferred valid for I2S_DATA_FRAME_16_16 and I2S_DATA_FRAME_16_32 */
        number_of_transfers = (dma_mem->size >> 1) - 1;
    }

    /* First interrupt at half buffer */
    lregs->DMA_INT_REG = number_of_transfers / 2;
    lregs->DMA_LEN_REG = number_of_transfers;
    if (rregs) {
        /* DMA is handled in one interrupt, not need for right channel int */
        rregs->DMA_INT_REG = 0xFFFF;
        rregs->DMA_LEN_REG = number_of_transfers;
        rregs->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_DMA_ON_Msk;
    }
    lregs->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_DMA_ON_Msk;
}

struct pcm_div {
    uint16_t div;
    uint16_t fdiv;
};

static void
da1469x_i2s_compute_pcm_div(uint32_t system_clock, uint32_t bit_rate, struct pcm_div *div)
{
    int32_t denominator = 0;

    uint32_t initial_deviation;
    int32_t deviation;
    int32_t minimum_deviation = INT32_MAX;
    uint32_t bit_accumulator;
    uint32_t frac_accumulator;
    uint16_t fdiv = 1;

    div->div = system_clock / bit_rate;
    div->fdiv = 0;
    bit_accumulator = bit_rate;

    initial_deviation = system_clock - (bit_rate * div->div);
    deviation = initial_deviation;
    frac_accumulator = initial_deviation;

    while (deviation != 0) {
        if (deviation > 0) {
            if (denominator == 16) {
                break;
            }
            ++denominator;
            fdiv <<= 1;
            frac_accumulator += initial_deviation;
        } else if (deviation < 0) {
            fdiv |= 1;
            bit_accumulator += bit_rate;
        }
        deviation = bit_accumulator - frac_accumulator;
        if (abs(deviation) < minimum_deviation) {
            minimum_deviation = abs(deviation);
            div->fdiv = fdiv;
        }
    }
}

static void
da1469x_i2s_dma_tx_isr(void *arg)
{
    struct i2s *i2s = da1469x_i2s.i2s;
    struct i2s_sample_buffer *buffer;
    (void)arg;

    /* Change interrupt time to the end or half the circular buffer size, depending on which half is active. */
    da1469x_i2s.dma_regs[1]->DMA_INT_REG = da1469x_i2s.dma_regs[1]->DMA_LEN_REG >> (da1469x_i2s.active_half);

    /* DMA is already in other half, let active_half be consistent. */
    da1469x_i2s.active_half ^= 1;

    assert(da1469x_i2s.full_buffer_count > 0 && da1469x_i2s.full_buffer_count < 3);
    da1469x_i2s.full_buffer_count--;
    buffer = i2s_driver_buffer_get(i2s);
    if (buffer) {
        da1469x_i2s_fill_dma_buffer(buffer->sample_data, buffer->sample_count, i2s->sample_size_in_bytes);
        i2s_driver_buffer_put(i2s, buffer);
    } else {
        da1469x_i2s.dma_regs[1]->DMA_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DMA_ON_Msk;
        if (da1469x_i2s.cfg.data_format != I2S_DATA_FRAME_16_16) {
            da1469x_i2s.dma_regs[3]->DMA_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DMA_ON_Msk;
        }
        i2s_driver_state_changed(i2s, I2S_STATE_OUT_OF_BUFFERS);
    }
}

int
i2s_driver_start(struct i2s *i2s)
{
    int rc = 0;
    uint32_t bit_rate;
    uint32_t bits_per_frame;
    struct i2s_sample_buffer *buffer;
    struct da1469x_dma_buffer *dma_mem = da1469x_i2s.cfg.dma_memory;
    struct pcm_div div;
    const i2s_data_format_t data_format = da1469x_i2s.cfg.data_format;
    struct da1469x_dma_config tx_cfg = {
        .src_inc = true,
        .dst_inc = false,
        .priority = 0,
        .burst_mode = 0,
        .bus_width = data_format == I2S_DATA_FRAME_16_32 ? MCU_DMA_BUS_WIDTH_2B : MCU_DMA_BUS_WIDTH_4B,
    };

    if (!da1469x_i2s.running) {
        da1469x_pd_acquire(MCU_PD_DOMAIN_PER);
        if (da1469x_i2s.dma_regs[0] == NULL && da1469x_i2s.dma_regs[1] == NULL) {
            rc = da1469x_dma_acquire_periph(-1, MCU_DMA_PERIPH_PCM, da1469x_i2s.dma_regs);
            if (rc) {
                return rc;
            }

            da1469x_dma_configure(da1469x_i2s.dma_regs[1], &tx_cfg,
                                  (da1469x_dma_interrupt_cb)da1469x_i2s_dma_tx_isr, NULL);
            da1469x_i2s.dma_regs[1]->DMA_A_START_REG = (uint32_t)dma_mem->buffer;
            da1469x_i2s.dma_regs[1]->DMA_B_START_REG = (uint32_t)&APU->PCM1_OUT1_REG +
                                                       ((data_format == I2S_DATA_FRAME_16_32) ? 2 : 0);
            da1469x_i2s.dma_regs[1]->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_CIRCULAR_Msk;
        }
        if (data_format != I2S_DATA_FRAME_16_16) {
            rc = da1469x_dma_acquire_periph(-1, MCU_DMA_PERIPH_PCM, &da1469x_i2s.dma_regs[2]);
            if (rc) {
                da1469x_dma_release_channel(da1469x_i2s.dma_regs[0]);
                da1469x_i2s.dma_regs[1] = NULL;
                return rc;
            }

            da1469x_dma_configure(da1469x_i2s.dma_regs[3], &tx_cfg,
                                  (da1469x_dma_interrupt_cb)da1469x_i2s_dma_tx_isr, NULL);
            da1469x_i2s.dma_regs[3]->DMA_A_START_REG = (uint32_t)dma_mem->buffer + da1469x_i2s.cfg.dma_memory->size;
            da1469x_i2s.dma_regs[3]->DMA_B_START_REG = (uint32_t)&APU->PCM1_OUT2_REG +
                                                       ((data_format == I2S_DATA_FRAME_16_32) ? 2 : 0);
            da1469x_i2s.dma_regs[3]->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_CIRCULAR_Msk;
        }
        bits_per_frame = data_format == I2S_DATA_FRAME_16_16 ? 32 : 64;
        bit_rate = i2s->sample_rate * bits_per_frame;
        da1469x_i2s_compute_pcm_div(SystemCoreClock, bit_rate, &div);
        CRG_PER->PCM_DIV_REG = (1 << CRG_PER_PCM_DIV_REG_PCM_SRC_SEL_Pos) |
                               (1 << CRG_PER_PCM_DIV_REG_CLK_PCM_EN_Pos) |
                               (div.div << CRG_PER_PCM_DIV_REG_PCM_DIV_Pos);
        CRG_PER->PCM_FDIV_REG = div.fdiv;

        APU->PCM1_CTRL_REG = ((bits_per_frame - 1) << APU_PCM1_CTRL_REG_PCM_FSC_DIV_Pos) |
                             (((data_format == I2S_DATA_FRAME_16_16) ? 0 : 1) << APU_PCM1_CTRL_REG_PCM_FSC_EDGE_Pos) |
                             ((bits_per_frame / 16) << APU_PCM1_CTRL_REG_PCM_FSCLEN_Pos) |
                             (0 << APU_PCM1_CTRL_REG_PCM_FSCDEL_Pos) |
                             (1 << APU_PCM1_CTRL_REG_PCM_CLKINV_Pos) |
                             (0 << APU_PCM1_CTRL_REG_PCM_FSCINV_Pos) |
                             (0 << APU_PCM1_CTRL_REG_PCM_CH_DEL_Pos) |
                             (0 << APU_PCM1_CTRL_REG_PCM_PPOD_Pos) |
                             (1 << APU_PCM1_CTRL_REG_PCM_MASTER_Pos);
        APU->PCM1_CTRL_REG |= (1 << APU_PCM1_CTRL_REG_PCM_EN_Pos);

        if (i2s->direction == I2S_OUT) {
            APU->APU_MUX_REG = 2u << APU_APU_MUX_REG_PCM1_MUX_IN_Pos;
        }

        da1469x_i2s.running = true;

        if (i2s->direction == I2S_OUT) {
            while (da1469x_i2s.full_buffer_count < 2 && (buffer = i2s_driver_buffer_get(i2s)) != NULL) {
                da1469x_i2s_fill_dma_buffer(buffer->sample_data, buffer->sample_count, i2s->sample_size_in_bytes);
                i2s_driver_buffer_put(i2s, buffer);
            }
        } else {
            /* TODO: Implement handling of I2S_IN */
        }
        if (da1469x_i2s.full_buffer_count == 0) {
            i2s->state = I2S_STATE_OUT_OF_BUFFERS;
            rc = I2S_ERR_NO_BUFFER;
        } else {
            i2s->state = I2S_STATE_RUNNING;
            if (i2s->direction == I2S_OUT) {
                da1469x_i2s_dma_tx_start();
            }
        }
    }
    return rc;
}

void
i2s_driver_buffer_queued(struct i2s *i2s)
{
    struct i2s_sample_buffer *buffer;

    while (da1469x_i2s.full_buffer_count < 2 && (buffer = i2s_driver_buffer_get(i2s)) != NULL) {
        da1469x_i2s_fill_dma_buffer(buffer->sample_data, buffer->sample_count, i2s->sample_size_in_bytes);
        i2s_driver_buffer_put(i2s, buffer);
    }
}

int
i2s_driver_suspend(struct i2s *i2s, os_time_t timeout, int arg)
{
    return OS_OK;
}

int
i2s_driver_resume(struct i2s *i2s)
{
    return OS_OK;
}
