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
#include <mcu/mcu.h>
#include <mcu/stm32_hal.h>
#include <i2s/i2s.h>
#include <i2s/i2s_driver.h>
#include <i2s_stm32f1/stm32_pin_cfg.h>
#include <i2s_stm32f1/i2s_stm32f1.h>

static struct stm32_i2s stm32_i2s2;
static struct stm32_i2s stm32_i2s3;

void
i2s2_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_I2S_IRQHandler(&stm32_i2s2.hi2s);

    os_trace_isr_exit();
}

void
i2s3_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_I2S_IRQHandler(&stm32_i2s3.hi2s);

    os_trace_isr_exit();
}

static void
i2s2_clock_enable(bool enable)
{
#ifdef SPI2
    if (enable) {
        __HAL_RCC_SPI2_CLK_ENABLE();
    } else {
        __HAL_RCC_SPI2_CLK_DISABLE();
    }
#endif
}

static void
i2s3_clock_enable(bool enable)
{
#ifdef SPI3
    if (enable) {
        __HAL_RCC_SPI3_CLK_ENABLE();
    } else {
        __HAL_RCC_SPI3_CLK_DISABLE();
    }
#endif
}

void
HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    struct stm32_i2s *i2s_data = (struct stm32_i2s *)hi2s;
    struct i2s *i2s = i2s_data->i2s;
    struct i2s_sample_buffer *processed_buffer = i2s_data->active_buffer;

    i2s_data->active_buffer = i2s_driver_buffer_get(i2s);
    if (i2s_data->active_buffer) {
        HAL_I2S_Transmit_DMA(&i2s_data->hi2s,
                             i2s_data->active_buffer->sample_data,
                             i2s_data->active_buffer->sample_count);
    } else {
        i2s_driver_state_changed(i2s, I2S_STATE_OUT_OF_BUFFERS);
    }
    i2s_driver_buffer_put(i2s, processed_buffer);
}

void
HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    struct stm32_i2s *i2s_data = (struct stm32_i2s *)hi2s;
    struct i2s *i2s = i2s_data->i2s;
    struct i2s_sample_buffer *processed_buffer = i2s_data->active_buffer;

    i2s_data->active_buffer = i2s_driver_buffer_get(i2s);
    if (i2s_data->active_buffer) {
        HAL_I2S_Receive_DMA(&i2s_data->hi2s,
                            i2s_data->active_buffer->sample_data,
                            i2s_data->active_buffer->capacity);
    } else {
        i2s_driver_state_changed(i2s, I2S_STATE_OUT_OF_BUFFERS);
    }
    processed_buffer->sample_count = processed_buffer->capacity;
    i2s_driver_buffer_put(i2s, processed_buffer);
}

static void
i2s2_dma_stream_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(&stm32_i2s2.hdma_spi);

    os_trace_isr_exit();
}

static void
i2s3_dma_stream_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(&stm32_i2s3.hdma_spi);

    os_trace_isr_exit();
}

static void
dma1_enable_clock(bool enable)
{
    if (enable) {
        __HAL_RCC_DMA1_CLK_ENABLE();
    } else {
        __HAL_RCC_DMA1_CLK_DISABLE();
    }
}

static void
dma2_enable_clock(bool enable)
{
#ifdef __HAL_RCC_DMA2_CLK_DISABLE
    if (enable) {
        __HAL_RCC_DMA2_CLK_ENABLE();
    } else {
        __HAL_RCC_DMA2_CLK_DISABLE();
    }
#endif
}

static void
i2s_init_interrupts(const struct i2s_cfg *cfg)
{
    NVIC_SetVector(cfg->hw_cfg->dma_channel_irq, (uint32_t)cfg->hw_cfg->dma_irq_handler);
    HAL_NVIC_SetPriority(cfg->hw_cfg->dma_channel_irq, 5, 0);
    HAL_NVIC_EnableIRQ(cfg->hw_cfg->dma_channel_irq);

    /* I2S interrupt Init */
    NVIC_SetVector(cfg->hw_cfg->i2s_irq, (uint32_t)cfg->hw_cfg->i2s_irq_handler);
    HAL_NVIC_SetPriority(cfg->hw_cfg->i2s_irq, 5, 0);
    HAL_NVIC_EnableIRQ(cfg->hw_cfg->i2s_irq);
}

static void
i2s_init_pins(const struct stm32_i2s_pins *pins)
{
    GPIO_InitTypeDef gpio_init;

    gpio_init = pins->ck_pin->hal_init;
    hal_gpio_init_stm(pins->ck_pin->pin, &gpio_init);
    gpio_init = pins->ws_pin->hal_init;
    hal_gpio_init_stm(pins->ws_pin->pin, &gpio_init);
    gpio_init = pins->sd_pin->hal_init;
    hal_gpio_init_stm(pins->sd_pin->pin, &gpio_init);
}

static int
stm32_i2s_init(struct i2s *i2s, const struct i2s_cfg *cfg)
{
    int rc = 0;
    struct stm32_i2s *stm32_i2s;

    i2s->direction = ((cfg->mode == I2S_MODE_MASTER_TX) ||
                      (cfg->mode == I2S_MODE_SLAVE_TX)) ? I2S_OUT : I2S_IN;

    if (cfg->data_format == I2S_DATAFORMAT_16B_EXTENDED ||
        cfg->data_format == I2S_DATAFORMAT_16B) {
        i2s->sample_size_in_bytes = 2;
    } else {
        i2s->sample_size_in_bytes = 4;
    }

    rc = i2s_init(i2s, cfg->pool);

    if (rc != OS_OK) {
        goto end;
    }

    stm32_i2s = cfg->hw_cfg->driver_data;
    stm32_i2s->i2s = i2s;

    i2s->sample_rate = cfg->sample_rate;
    i2s->driver_data = stm32_i2s;

    i2s_init_pins((struct stm32_i2s_pins *)&cfg->hw_cfg->pins);

    cfg->hw_cfg->i2s_enable_clock(true);

    stm32_i2s->hi2s.Instance = cfg->hw_cfg->i2s_base;
    stm32_i2s->hi2s.Init.Mode = cfg->mode;
    stm32_i2s->hi2s.Init.Standard = cfg->standard;
    stm32_i2s->hi2s.Init.DataFormat = cfg->data_format;
    stm32_i2s->hi2s.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
    stm32_i2s->hi2s.Init.AudioFreq = cfg->sample_rate;
    stm32_i2s->hi2s.Init.CPOL = I2S_CPOL_LOW;

    if (HAL_I2S_Init(&stm32_i2s->hi2s) != HAL_OK) {
        rc = SYS_EUNKNOWN;
        goto end;
    }

    cfg->hw_cfg->dma_enable_clock(true);

    stm32_i2s->hdma_spi.Instance = cfg->hw_cfg->dma_channel_base;
    if (cfg->mode == I2S_MODE_MASTER_TX || cfg->mode == I2S_MODE_SLAVE_TX) {
        stm32_i2s->hdma_spi.Init.Direction = DMA_MEMORY_TO_PERIPH;
    } else {
        stm32_i2s->hdma_spi.Init.Direction = DMA_PERIPH_TO_MEMORY;
    }
    stm32_i2s->hdma_spi.Init.PeriphInc = DMA_PINC_DISABLE;
    stm32_i2s->hdma_spi.Init.MemInc = DMA_MINC_ENABLE;
    stm32_i2s->hdma_spi.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    stm32_i2s->hdma_spi.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    stm32_i2s->hdma_spi.Init.Mode = DMA_NORMAL;
    stm32_i2s->hdma_spi.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&stm32_i2s->hdma_spi) != HAL_OK) {
        rc = SYS_EUNKNOWN;
        goto end;
    }

    if (cfg->mode == I2S_MODE_MASTER_TX || cfg->mode == I2S_MODE_SLAVE_TX) {
        __HAL_LINKDMA(&stm32_i2s->hi2s, hdmatx, stm32_i2s->hdma_spi);
    } else {
        __HAL_LINKDMA(&stm32_i2s->hi2s, hdmarx, stm32_i2s->hdma_spi);
    }

    i2s_init_interrupts(cfg);
end:
    return rc;
}

int
i2s_create(struct i2s *i2s, const char *name, const struct i2s_cfg *cfg)
{
    return os_dev_create(&i2s->dev, name, OS_DEV_INIT_PRIMARY,
                         100, (os_dev_init_func_t)stm32_i2s_init, (void *)cfg);
}

int
i2s_driver_stop(struct i2s *i2s)
{
    struct stm32_i2s *i2s_data = (struct stm32_i2s *)i2s->driver_data;
    struct i2s_sample_buffer *buffer;

    HAL_I2S_DMAStop(&i2s_data->hi2s);
    if (i2s->state == I2S_STATE_RUNNING && i2s->direction == I2S_OUT) {
        /*
         * When DMA is stopped and then I2S peripheral is stopped, it
         * may happen that DMA put some data already in SPI data buffer.
         * In that case single sample may be in the I2S output buffer.
         * If this happens next transmission will swap channels due to
         * one extra sample already present.
         * To avoid this just wait till all samples are gone.
         */
        if (0 == (i2s_data->hi2s.Instance->SR & SPI_SR_TXE_Msk)) {
            __HAL_I2S_ENABLE(&i2s_data->hi2s);
            while (0 == (i2s_data->hi2s.Instance->SR & SPI_SR_TXE_Msk))
                ;
            __HAL_I2S_DISABLE(&i2s_data->hi2s);
        }
    }

    assert(i2s_data->hi2s.State == HAL_I2S_STATE_READY);
    buffer = i2s_data->active_buffer;
    if (buffer) {
        i2s_data->active_buffer = NULL;
        i2s_driver_buffer_put(i2s, buffer);
    }

    return 0;
}

int
i2s_driver_start(struct i2s *i2s)
{
    int rc = 0;
    struct stm32_i2s *i2s_data = (struct stm32_i2s *)i2s->driver_data;

    switch (i2s_data->hi2s.State) {
    case HAL_I2S_STATE_READY:
        assert(i2s_data->active_buffer == NULL);
        i2s_data->active_buffer = i2s_driver_buffer_get(i2s);
        assert(i2s_data->active_buffer);
        if (i2s_data->active_buffer == NULL) {
            i2s->state = I2S_STATE_OUT_OF_BUFFERS;
            rc = I2S_ERR_NO_BUFFER;
            break;
        }
        i2s->state = I2S_STATE_RUNNING;
        if (i2s->direction == I2S_IN) {
            i2s_data->active_buffer->sample_count = i2s_data->active_buffer->capacity;
            HAL_I2S_Receive_DMA(&i2s_data->hi2s,
                                i2s_data->active_buffer->sample_data,
                                i2s_data->active_buffer->sample_count);
        } else if (i2s->direction == I2S_OUT) {
            HAL_I2S_Transmit_DMA(&i2s_data->hi2s,
                                 i2s_data->active_buffer->sample_data,
                                 i2s_data->active_buffer->sample_count);
        }
        break;
    case HAL_I2S_STATE_BUSY:
    case HAL_I2S_STATE_BUSY_RX:
    case HAL_I2S_STATE_BUSY_TX:
    default:
        rc = I2S_ERR_INTERNAL;
    }
    return rc;
}

void
i2s_driver_buffer_queued(struct i2s *i2s)
{
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

bool
i2s_out_is_active(struct i2s *i2s)
{
    struct stm32_i2s *i2s_data = (struct stm32_i2s *)i2s->driver_data;
    return READ_BIT(i2s_data->hi2s.Instance->I2SCFGR, SPI_I2SCFGR_I2SE) != 0;
}

#define I2S_PIN_DEFINE(n, po, pi) \
    const struct stm32_pin_cfg I2S ## n ## _P ## po ## pi = {    \
        .pin = MCU_GPIO_PORT ## po(pi),                          \
        .hal_init = {                                            \
            .Pin = GPIO_PIN_ ## pi,                              \
            .Mode = GPIO_MODE_AF_PP,                             \
            .Pull = GPIO_NOPULL,                                 \
            .Speed = GPIO_SPEED_FREQ_LOW,                        \
        }                                                        \
    }

#define I2S_CK_PIN_DEFINE(n, po, pi) \
    const struct stm32_pin_cfg I2S ## n ## _CK_P ## po ## pi = { \
        .pin = MCU_GPIO_PORT ## po(pi),                          \
        .hal_init = {                                            \
            .Pin = GPIO_PIN_ ## pi,                              \
            .Mode = GPIO_MODE_AF_PP,                             \
            .Pull = GPIO_NOPULL,                                 \
            .Speed = GPIO_SPEED_FREQ_LOW,                        \
        }                                                        \
    }

#define I2S_WS_PIN_DEFINE(n, po, pi) \
    const struct stm32_pin_cfg I2S ## n ## _WS_P ## po ## pi = { \
        .pin = MCU_GPIO_PORT ## po(pi),                          \
        .hal_init = {                                            \
            .Pin = GPIO_PIN_ ## pi,                              \
            .Mode = GPIO_MODE_AF_PP,                             \
            .Pull = GPIO_NOPULL,                                 \
            .Speed = GPIO_SPEED_FREQ_LOW,                        \
        }                                                        \
    }

#define I2S_SD_PIN_DEFINE(n, po, pi) \
    const struct stm32_pin_cfg I2S ## n ## _SD_P ## po ## pi = { \
        .pin = MCU_GPIO_PORT ## po(pi),                          \
        .hal_init = {                                            \
            .Pin = GPIO_PIN_ ## pi,                              \
            .Mode = GPIO_MODE_AF_PP,                             \
            .Pull = GPIO_NOPULL,                                 \
            .Speed = GPIO_SPEED_FREQ_LOW,                        \
        }                                                        \
    }

const struct stm32f1_i2s_hw_cfg i2s2_tx = {
    .dma_num = 1,
    .dma_channel_irq = DMA1_Channel5_IRQn,
    .dma_channel_base = DMA1_Channel5,
    .dma_base = DMA1,
    .dma_irq_handler = i2s2_dma_stream_irq_handler,
    .dma_enable_clock = dma1_enable_clock,
    .i2s_irq_handler = i2s2_irq_handler,
    .i2s_enable_clock = i2s2_clock_enable,
    .pins = {
        .ck_pin = I2S_CK_PIN(2, B, 13),
        .ws_pin = I2S_WS_PIN(2, B, 12),
        .sd_pin = I2S_SD_PIN(2, B, 15),
    },
    .driver_data = &stm32_i2s2,
    .i2s_base = SPI2,
};

const struct stm32f1_i2s_hw_cfg i2s2_rx = {
    .dma_num = 1,
    .dma_channel_irq = DMA1_Channel4_IRQn,
    .dma_channel_base = DMA1_Channel4,
    .dma_base = DMA1,
    .dma_irq_handler = i2s2_dma_stream_irq_handler,
    .dma_enable_clock = dma1_enable_clock,
    .i2s_irq_handler = i2s2_irq_handler,
    .i2s_enable_clock = i2s2_clock_enable,
    .pins = {
        .ck_pin = I2S_CK_PIN(2, B, 13),
        .ws_pin = I2S_WS_PIN(2, B, 12),
        .sd_pin = I2S_SD_PIN(2, B, 15),
    },
    .driver_data = &stm32_i2s2,
    .i2s_base = SPI2,
};

const struct stm32f1_i2s_hw_cfg i2s3_tx = {
    .dma_num = 2,
    .dma_channel_irq = DMA2_Channel2_IRQn,
    .dma_channel_base = DMA2_Channel2,
    .dma_base = DMA2,
    .dma_irq_handler = i2s3_dma_stream_irq_handler,
    .dma_enable_clock = dma2_enable_clock,
    .i2s_irq_handler = i2s3_irq_handler,
    .i2s_enable_clock = i2s3_clock_enable,
    .pins = {
        .ck_pin = I2S_CK_PIN(3, B, 3),
        .ws_pin = I2S_WS_PIN(3, A, 15),
        .sd_pin = I2S_SD_PIN(3, B, 5),
    },
    .driver_data = &stm32_i2s3,
    .i2s_base = SPI3,
};

const struct stm32f1_i2s_hw_cfg i2s3_rx = {
    .dma_num = 2,
    .dma_channel_irq = DMA2_Channel1_IRQn,
    .dma_channel_base = DMA2_Channel1,
    .dma_base = DMA2,
    .dma_irq_handler = i2s3_dma_stream_irq_handler,
    .dma_enable_clock = dma2_enable_clock,
    .i2s_irq_handler = i2s3_irq_handler,
    .i2s_enable_clock = i2s3_clock_enable,
    .pins = {
        .ck_pin = I2S_CK_PIN(3, B, 3),
        .ws_pin = I2S_WS_PIN(3, A, 15),
        .sd_pin = I2S_SD_PIN(3, B, 5),
    },
    .driver_data = &stm32_i2s3,
    .i2s_base = SPI3,
};

/* I2S2 Possible CK pins */
I2S_CK_PIN_DEFINE(2, B, 13);
/* I2S2 possible WS pins */
I2S_WS_PIN_DEFINE(2, B, 12);
/* I2S2 possible SD pins */
I2S_SD_PIN_DEFINE(2, B, 15);
/* I2S2 Possible MCK pins */
I2S_PIN_DEFINE(2, C, 6);

/* I2S3 possible CK pins */
I2S_CK_PIN_DEFINE(3, B, 3);
/* I2S3 possible WS pins */
I2S_WS_PIN_DEFINE(3, A, 15);
/* I2S3 possible SD pins */
I2S_SD_PIN_DEFINE(3, B, 5);
/* I2S3 possible MCK pins */
I2S_PIN_DEFINE(3, C, 7);
