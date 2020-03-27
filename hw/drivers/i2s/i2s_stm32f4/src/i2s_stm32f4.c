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
#include <bsp/bsp.h>
#include <i2s/i2s.h>
#include <i2s/i2s_driver.h>
#include <i2s_stm32f4/i2s_stm32f4.h>

#if defined(SPI_I2S_FULLDUPLEX_SUPPORT)
#define I2SEXT_PRESENT  1
#else
#define I2SEXT_PRESENT  0
#endif

struct stm32_spi_cfg {
    uint8_t spi_num;
    SPI_TypeDef *spi;
    IRQn_Type i2s_irq;
    struct stm32_i2s *driver_data;
    DMA_HandleTypeDef *hdma_spi;
    void (*irq_handler)(void);
    void (*i2s_dma_handler)(void);
    void (*enable_clock)(bool enable);
#if I2SEXT_PRESENT
    DMA_HandleTypeDef *hdma_i2sext;
    void (*i2sext_dma_handler)(void);
#endif
};

static struct stm32_i2s stm32_i2s1;
static struct stm32_i2s stm32_i2s2;
static struct stm32_i2s stm32_i2s3;
static struct stm32_i2s stm32_i2s4;
static struct stm32_i2s stm32_i2s5;

void
i2s1_irq_handler(void)
{
    HAL_I2S_IRQHandler(&stm32_i2s1.hi2s);
}

void
i2s2_irq_handler(void)
{
    HAL_I2S_IRQHandler(&stm32_i2s2.hi2s);
}

void
i2s3_irq_handler(void)
{
    HAL_I2S_IRQHandler(&stm32_i2s3.hi2s);
}

void
i2s4_irq_handler(void)
{
    HAL_I2S_IRQHandler(&stm32_i2s4.hi2s);
}

void
i2s5_irq_handler(void)
{
    HAL_I2S_IRQHandler(&stm32_i2s5.hi2s);
}

static void
i2s1_clock_enable(bool enable)
{
#ifdef SPI1
    if (enable) {
        __HAL_RCC_SPI1_CLK_ENABLE();
    } else {
        __HAL_RCC_SPI1_CLK_DISABLE();
    }
#endif
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

static void
i2s4_clock_enable(bool enable)
{
#ifdef SPI4
    if (enable) {
        __HAL_RCC_SPI4_CLK_ENABLE();
    } else {
        __HAL_RCC_SPI4_CLK_DISABLE();
    }
#endif
}

static void
i2s5_clock_enable(bool enable)
{
#ifdef SPI5
    if (enable) {
        __HAL_RCC_SPI5_CLK_ENABLE();
    } else {
        __HAL_RCC_SPI5_CLK_DISABLE();
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

#if I2SEXT_PRESENT
void
HAL_I2SEx_TxRxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    struct stm32_i2s *i2s_data = (struct stm32_i2s *)hi2s;
    struct i2s *i2s = i2s_data->i2s;
    struct i2s_sample_buffer *processed_buffer = i2s_data->active_buffer;

    i2s_data->active_buffer = i2s_driver_buffer_get(i2s);
    if (i2s_data->active_buffer) {
        HAL_I2SEx_TransmitReceive_DMA(hi2s,
                                      i2s_data->active_buffer->sample_data,
                                      i2s_data->active_buffer->sample_data,
                                      i2s_data->active_buffer->sample_count);
    } else {
        i2s_driver_state_changed(i2s, I2S_STATE_OUT_OF_BUFFERS);
    }
    i2s_driver_buffer_put(i2s, processed_buffer);
}
#endif

static void
i2s1_dma_stream_irq_handler(void)
{
    HAL_DMA_IRQHandler(stm32_i2s1.hdma_spi);
}

static void
i2s2_dma_stream_irq_handler(void)
{
    HAL_DMA_IRQHandler(stm32_i2s2.hdma_spi);
}

static void
i2s2ext_dma_stream_irq_handler(void)
{
    HAL_DMA_IRQHandler(stm32_i2s2.hdma_i2sext);
}

static void
i2s3_dma_stream_irq_handler(void)
{
    HAL_DMA_IRQHandler(stm32_i2s3.hdma_spi);
}

static void
i2s3ext_dma_stream_irq_handler(void)
{
    HAL_DMA_IRQHandler(stm32_i2s3.hdma_i2sext);
}

static void
i2s4_dma_stream_irq_handler(void)
{
    HAL_DMA_IRQHandler(stm32_i2s4.hdma_spi);
}

static void
i2s5_dma_stream_irq_handler(void)
{
    HAL_DMA_IRQHandler(stm32_i2s5.hdma_spi);
}

static void
i2s_init_interrupts(const struct i2s_cfg *cfg)
{
    NVIC_SetVector(cfg->dma_cfg->dma_stream_irq, (uint32_t)cfg->spi_cfg->i2s_dma_handler);
    HAL_NVIC_SetPriority(cfg->dma_cfg->dma_stream_irq, 5, 0);
    HAL_NVIC_EnableIRQ(cfg->dma_cfg->dma_stream_irq);

#if I2SEXT_PRESENT
    if (cfg->dma_i2sext_cfg) {
        NVIC_SetVector(cfg->dma_i2sext_cfg->dma_stream_irq, (uint32_t)cfg->spi_cfg->i2sext_dma_handler);
        HAL_NVIC_SetPriority(cfg->dma_i2sext_cfg->dma_stream_irq, 5, 0);
        HAL_NVIC_EnableIRQ(cfg->dma_i2sext_cfg->dma_stream_irq);
    }
#endif

    /* I2S1 interrupt Init */
    NVIC_SetVector(cfg->spi_cfg->i2s_irq, (uint32_t)cfg->spi_cfg->irq_handler);
    HAL_NVIC_SetPriority(cfg->spi_cfg->i2s_irq, 5, 0);
    HAL_NVIC_EnableIRQ(cfg->spi_cfg->i2s_irq);
}

static void
i2s_init_pins(struct stm32_i2s_pins *pins)
{
    hal_gpio_init_stm(pins->ck_pin->pin, (GPIO_InitTypeDef *)&pins->ck_pin->hal_init);
    hal_gpio_init_stm(pins->ws_pin->pin, (GPIO_InitTypeDef *)&pins->ws_pin->hal_init);
    hal_gpio_init_stm(pins->sd_pin->pin, (GPIO_InitTypeDef *)&pins->sd_pin->hal_init);
#if I2SEXT_PRESENT
    if (pins->ext_sd_pin) {
        hal_gpio_init_stm(pins->ext_sd_pin->pin, (GPIO_InitTypeDef *)&pins->ext_sd_pin->hal_init);
    }
#endif
}

static int
stm32_i2s_init(struct i2s *i2s, const struct i2s_cfg *cfg)
{
    int rc = 0;
    struct stm32_i2s *stm32_i2s;

#if I2SEXT_PRESENT
    if (cfg->pins.ext_sd_pin != NULL) {
        i2s->direction = I2S_OUT_IN;
    } else {
        i2s->direction = ((cfg->mode == I2S_MODE_MASTER_TX) ||
                          (cfg->mode == I2S_MODE_SLAVE_TX)) ? I2S_OUT : I2S_IN;
    }
#else
    i2s->direction = ((cfg->mode == I2S_MODE_MASTER_TX) ||
                      (cfg->mode == I2S_MODE_SLAVE_TX)) ? I2S_OUT : I2S_IN;
#endif

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

    stm32_i2s = cfg->spi_cfg->driver_data;
    stm32_i2s->i2s = i2s;
    stm32_i2s->hdma_spi = cfg->spi_cfg->hdma_spi;
#if I2SEXT_PRESENT
    stm32_i2s->hdma_i2sext = cfg->spi_cfg->hdma_i2sext;
#endif

    i2s->sample_rate = cfg->sample_rate;
    i2s->driver_data = stm32_i2s;

    i2s_init_pins((struct stm32_i2s_pins *)&cfg->pins);

    cfg->spi_cfg->enable_clock(true);

    stm32_i2s->hi2s.Instance = cfg->spi_cfg->spi;
    stm32_i2s->hi2s.Init.Mode = cfg->mode;
    stm32_i2s->hi2s.Init.Standard = cfg->standard;
    stm32_i2s->hi2s.Init.DataFormat = cfg->data_format;
    stm32_i2s->hi2s.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
    stm32_i2s->hi2s.Init.AudioFreq = cfg->sample_rate;
    stm32_i2s->hi2s.Init.CPOL = I2S_CPOL_LOW;
#if I2SEXT_PRESENT
    stm32_i2s->hi2s.Init.ClockSource = I2S_CLOCK_PLL;
    if (i2s->direction == I2S_OUT_IN) {
        stm32_i2s->hi2s.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_ENABLE;
    } else {
        stm32_i2s->hi2s.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
    }
#endif

    if (cfg->dma_cfg->dma_num == 1) {
        __HAL_RCC_DMA1_CLK_ENABLE();
    } else {
#ifdef __HAL_RCC_DMA2_CLK_DISABLE
        __HAL_RCC_DMA2_CLK_ENABLE();
#endif
    }

    stm32_i2s->hdma_spi->Instance = cfg->dma_cfg->dma_stream;
    stm32_i2s->hdma_spi->Init.Channel = cfg->dma_cfg->dma_channel;
    if (cfg->mode == I2S_MODE_MASTER_TX || cfg->mode == I2S_MODE_SLAVE_TX) {
        stm32_i2s->hdma_spi->Init.Direction = DMA_MEMORY_TO_PERIPH;
    } else {
        stm32_i2s->hdma_spi->Init.Direction = DMA_PERIPH_TO_MEMORY;
    }
    stm32_i2s->hdma_spi->Init.PeriphInc = DMA_PINC_DISABLE;
    stm32_i2s->hdma_spi->Init.MemInc = DMA_MINC_ENABLE;
    stm32_i2s->hdma_spi->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    stm32_i2s->hdma_spi->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    stm32_i2s->hdma_spi->Init.Mode = DMA_NORMAL;
    stm32_i2s->hdma_spi->Init.Priority = DMA_PRIORITY_LOW;
    stm32_i2s->hdma_spi->Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (cfg->mode == I2S_MODE_MASTER_TX || cfg->mode == I2S_MODE_SLAVE_TX) {
        __HAL_LINKDMA(&stm32_i2s->hi2s, hdmatx, *stm32_i2s->hdma_spi);
    } else {
        __HAL_LINKDMA(&stm32_i2s->hi2s, hdmarx, *stm32_i2s->hdma_spi);
    }

#if I2SEXT_PRESENT
    if (i2s->direction == I2S_OUT_IN) {
        stm32_i2s->hdma_i2sext->Instance = cfg->dma_i2sext_cfg->dma_stream;
        stm32_i2s->hdma_i2sext->Init.Channel = cfg->dma_i2sext_cfg->dma_channel;
        if (cfg->mode == I2S_MODE_MASTER_TX || cfg->mode == I2S_MODE_SLAVE_TX) {
            stm32_i2s->hdma_i2sext->Init.Direction = DMA_PERIPH_TO_MEMORY;
            __HAL_LINKDMA(&stm32_i2s->hi2s, hdmarx, *stm32_i2s->hdma_i2sext);
        } else {
            stm32_i2s->hdma_i2sext->Init.Direction = DMA_MEMORY_TO_PERIPH;
            __HAL_LINKDMA(&stm32_i2s->hi2s, hdmatx, *stm32_i2s->hdma_i2sext);
        }
        stm32_i2s->hdma_i2sext->Init.PeriphInc = DMA_PINC_DISABLE;
        stm32_i2s->hdma_i2sext->Init.MemInc = DMA_MINC_ENABLE;
        stm32_i2s->hdma_i2sext->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        stm32_i2s->hdma_i2sext->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        stm32_i2s->hdma_i2sext->Init.Mode = DMA_NORMAL;
        stm32_i2s->hdma_i2sext->Init.Priority = DMA_PRIORITY_LOW;
        stm32_i2s->hdma_i2sext->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    }
#endif

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
    HAL_I2S_DeInit(&i2s_data->hi2s);
    HAL_DMA_DeInit(i2s_data->hdma_spi);
#if I2SEXT_PRESENT
    if (i2s->direction == I2S_OUT_IN) {
        HAL_DMA_DeInit(i2s_data->hdma_i2sext);
    }
#endif
    return 0;
}

int
i2s_driver_start(struct i2s *i2s)
{
    int rc = 0;
    struct stm32_i2s *i2s_data = (struct stm32_i2s *)i2s->driver_data;

    switch (i2s_data->hi2s.State) {
    case HAL_I2S_STATE_RESET:
        if (i2s->sample_rate) {
            i2s_data->hi2s.Init.AudioFreq = i2s->sample_rate;
        }
        if (HAL_I2S_Init(&i2s_data->hi2s) != HAL_OK) {
            rc = SYS_EUNKNOWN;
            break;
        }
        if (HAL_DMA_Init(i2s_data->hdma_spi) != HAL_OK) {
            (void)HAL_I2S_DeInit(&i2s_data->hi2s);
            rc = SYS_EUNKNOWN;
            break;
        }
#if I2SEXT_PRESENT
        if (i2s->direction == I2S_OUT_IN) {
            if (HAL_DMA_Init(i2s_data->hdma_i2sext) != HAL_OK) {
                (void)HAL_DMA_DeInit(i2s_data->hdma_spi);
                (void)HAL_I2S_DeInit(&i2s_data->hi2s);
                rc = SYS_EUNKNOWN;
                break;
            }
        }
#endif
    /* Fallthrough */
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
        } else {
#if I2SEXT_PRESENT
            HAL_I2SEx_TransmitReceive_DMA(&i2s_data->hi2s,
                                          i2s_data->active_buffer->sample_data,
                                          i2s_data->active_buffer->sample_data,
                                          i2s_data->active_buffer->sample_count);
#endif
        }
        break;
    case HAL_I2S_STATE_BUSY:
    case HAL_I2S_STATE_BUSY_RX:
    case HAL_I2S_STATE_BUSY_TX:
    case HAL_I2S_STATE_BUSY_TX_RX:
        break;
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

#define I2S_PIN_DEFINE(n, po, pi, af) \
    const struct stm32_pin_cfg I2S ## n ## _P ## po ## pi = {    \
        .pin = MCU_GPIO_PORT ## po(pi),                          \
        .hal_init = {                                            \
            .Pin = GPIO_PIN_ ## pi,                              \
            .Mode = GPIO_MODE_AF_PP,                             \
            .Pull = GPIO_NOPULL,                                 \
            .Speed = GPIO_SPEED_FREQ_LOW,                        \
            .Alternate = af,                                     \
        }                                                        \
    }

#define I2S_CK_PIN_DEFINE(n, po, pi, af) \
    const struct stm32_pin_cfg I2S ## n ## _CK_P ## po ## pi = { \
        .pin = MCU_GPIO_PORT ## po(pi),                          \
        .hal_init = {                                            \
            .Pin = GPIO_PIN_ ## pi,                              \
            .Mode = GPIO_MODE_AF_PP,                             \
            .Pull = GPIO_NOPULL,                                 \
            .Speed = GPIO_SPEED_FREQ_LOW,                        \
            .Alternate = af,                                     \
        }                                                        \
    }

#define I2S_WS_PIN_DEFINE(n, po, pi, af) \
    const struct stm32_pin_cfg I2S ## n ## _WS_P ## po ## pi = { \
        .pin = MCU_GPIO_PORT ## po(pi),                          \
        .hal_init = {                                            \
            .Pin = GPIO_PIN_ ## pi,                              \
            .Mode = GPIO_MODE_AF_PP,                             \
            .Pull = GPIO_NOPULL,                                 \
            .Speed = GPIO_SPEED_FREQ_LOW,                        \
            .Alternate = af,                                     \
        }                                                        \
    }

#define I2S_SD_PIN_DEFINE(n, po, pi, af) \
    const struct stm32_pin_cfg I2S ## n ## _SD_P ## po ## pi = { \
        .pin = MCU_GPIO_PORT ## po(pi),                          \
        .hal_init = {                                            \
            .Pin = GPIO_PIN_ ## pi,                              \
            .Mode = GPIO_MODE_AF_PP,                             \
            .Pull = GPIO_NOPULL,                                 \
            .Speed = GPIO_SPEED_FREQ_LOW,                        \
            .Alternate = af,                                     \
        }                                                        \
    }

/* I2S1 Possible CK pins */
I2S_CK_PIN_DEFINE(1, A, 5, GPIO_AF5_SPI1);
I2S_CK_PIN_DEFINE(1, B, 3, GPIO_AF5_SPI1);

/* I2S1 possible WS pins */
I2S_WS_PIN_DEFINE(1, A, 4, GPIO_AF5_SPI1);
I2S_WS_PIN_DEFINE(1, A, 15, GPIO_AF5_SPI1);

/* I2S1 possible SD pins */
I2S_SD_PIN_DEFINE(1, B, 5, GPIO_AF5_SPI1);
I2S_SD_PIN_DEFINE(1, A, 7, GPIO_AF5_SPI1);

/* I2S2 Possible CKIN pins */
I2S_PIN_DEFINE(2, A, 2, GPIO_AF5_SPI2);
I2S_PIN_DEFINE(2, B, 11, GPIO_AF5_SPI2);
I2S_PIN_DEFINE(2, C, 9, GPIO_AF5_SPI2);

/* I2S2 Possible MCK pins */
I2S_PIN_DEFINE(2, A, 3, GPIO_AF5_SPI2);
I2S_PIN_DEFINE(2, A, 6, GPIO_AF6_SPI2);
I2S_PIN_DEFINE(2, C, 6, GPIO_AF6_SPI2);

/* I2S2 Possible CK pins */
I2S_CK_PIN_DEFINE(2, B, 10, GPIO_AF5_SPI2);
I2S_CK_PIN_DEFINE(2, B, 13, GPIO_AF5_SPI2);
I2S_CK_PIN_DEFINE(2, C, 7, GPIO_AF5_SPI2);
I2S_CK_PIN_DEFINE(2, D, 3, GPIO_AF5_SPI2);

/* I2S2 possible WS pins */
I2S_WS_PIN_DEFINE(2, B, 9, GPIO_AF5_SPI2);
I2S_WS_PIN_DEFINE(2, B, 12, GPIO_AF5_SPI2);

/* I2S2 possible SD pins */
I2S_SD_PIN_DEFINE(2, B, 15, GPIO_AF5_SPI2);
I2S_SD_PIN_DEFINE(2, C, 3, GPIO_AF5_SPI2);

/* I2S2_ext possible SD pins */
I2S_SD_PIN_DEFINE(2, B, 14, GPIO_AF6_SPI2);
I2S_SD_PIN_DEFINE(2, C, 2, GPIO_AF6_SPI2);

/* I2S3 possible CK pins */
I2S_CK_PIN_DEFINE(3, B, 3, GPIO_AF6_SPI3);
I2S_CK_PIN_DEFINE(3, C, 10, GPIO_AF6_SPI3);

/* I2S3 possible WS pins */
I2S_WS_PIN_DEFINE(3, A, 4, GPIO_AF6_SPI3);
I2S_WS_PIN_DEFINE(3, A, 15, GPIO_AF6_SPI3);

/* I2S3 possible SD pins */
I2S_SD_PIN_DEFINE(3, B, 5, GPIO_AF6_SPI3);
I2S_SD_PIN_DEFINE(3, C, 12, GPIO_AF6_SPI3);
I2S_SD_PIN_DEFINE(3, D, 6, GPIO_AF5_SPI3);

/* I2S3 possible MCK pins */
I2S_PIN_DEFINE(3, B, 10, GPIO_AF6_SPI3);
I2S_PIN_DEFINE(3, C, 7, GPIO_AF6_SPI3);

/* I2S4 possible CK pins */
I2S_CK_PIN_DEFINE(4, E, 2, GPIO_AF5_SPI4);
I2S_CK_PIN_DEFINE(4, E, 12, GPIO_AF5_SPI4);
I2S_CK_PIN_DEFINE(4, B, 13, GPIO_AF6_SPI4);

/* I2S3_ext possible SD pins */
I2S_SD_PIN_DEFINE(3, B, 4, GPIO_AF7_SPI3);
I2S_SD_PIN_DEFINE(3, C, 11, GPIO_AF5_SPI3);

/* I2S4 possible WS pins */
I2S_WS_PIN_DEFINE(4, B, 12, GPIO_AF6_SPI4);
I2S_WS_PIN_DEFINE(4, E, 4, GPIO_AF5_SPI4);
I2S_WS_PIN_DEFINE(4, E, 11, GPIO_AF5_SPI4);

/* I2S4 possible SD pins */
I2S_SD_PIN_DEFINE(4, A, 1, GPIO_AF5_SPI4);
I2S_SD_PIN_DEFINE(4, E, 6, GPIO_AF5_SPI4);
I2S_SD_PIN_DEFINE(4, E, 14, GPIO_AF5_SPI4);

/* I2S5 possible CK pins */
I2S_CK_PIN_DEFINE(5, B, 0, GPIO_AF6_SPI5);
I2S_CK_PIN_DEFINE(5, E, 2, GPIO_AF6_SPI5);
I2S_CK_PIN_DEFINE(5, E, 12, GPIO_AF6_SPI5);

/* I2S5 possible WS pins */
I2S_WS_PIN_DEFINE(5, B, 1, GPIO_AF6_SPI5);
I2S_WS_PIN_DEFINE(5, E, 4, GPIO_AF6_SPI5);
I2S_WS_PIN_DEFINE(5, E, 11, GPIO_AF6_SPI5);

/* I2S5 possible SD pins */
I2S_SD_PIN_DEFINE(5, A, 10, GPIO_AF6_SPI5);
I2S_SD_PIN_DEFINE(5, B, 8, GPIO_AF6_SPI5);
I2S_SD_PIN_DEFINE(5, E, 6, GPIO_AF6_SPI5);
I2S_SD_PIN_DEFINE(5, E, 14, GPIO_AF6_SPI5);

#define DMA_STREAM_DEFINE(dma, ch, st, name) \
    const struct stm32_dma_cfg name ## _stream ## st ## _channel ## ch = {  \
        dma,                                                                \
        DMA ## dma ## _Stream ## st ## _IRQn,                               \
        DMA ## dma ## _Stream ## st,                                        \
        DMA_CHANNEL_ ## ch,                                                 \
    }

DMA_STREAM_DEFINE(1, 0, 0, spi3_rx);
DMA_STREAM_DEFINE(1, 0, 1, i2c1_tx);
DMA_STREAM_DEFINE(1, 0, 2, spi3_rx);
DMA_STREAM_DEFINE(1, 0, 3, spi2_rx);
DMA_STREAM_DEFINE(1, 0, 4, spi2_tx);
DMA_STREAM_DEFINE(1, 0, 5, spi3_tx);
DMA_STREAM_DEFINE(1, 0, 7, spi3_tx);

DMA_STREAM_DEFINE(1, 1, 0, i2c1_rx);
DMA_STREAM_DEFINE(1, 1, 1, i2c3_rx);
DMA_STREAM_DEFINE(1, 1, 2, tim7_up);
DMA_STREAM_DEFINE(1, 1, 4, tim7_up);
DMA_STREAM_DEFINE(1, 1, 5, i2c1_rx);
DMA_STREAM_DEFINE(1, 1, 6, i2c1_tx);
DMA_STREAM_DEFINE(1, 1, 7, i2c1_tx);

DMA_STREAM_DEFINE(1, 2, 0, tim4_ch1);
DMA_STREAM_DEFINE(1, 2, 2, i2s3_ext_rx);
DMA_STREAM_DEFINE(1, 2, 3, tim4_ch2);
DMA_STREAM_DEFINE(1, 2, 4, i2s2_ext_tx);
DMA_STREAM_DEFINE(1, 2, 5, i2s3_ext_tx);
DMA_STREAM_DEFINE(1, 2, 6, tim4_up);
DMA_STREAM_DEFINE(1, 2, 7, tim4_ch3);

DMA_STREAM_DEFINE(1, 3, 0, i2s3_ext_rx);
DMA_STREAM_DEFINE(1, 3, 1, tim2_up);
DMA_STREAM_DEFINE(1, 3, 1, tim2_ch3);
DMA_STREAM_DEFINE(1, 3, 2, i2c3_rx);
DMA_STREAM_DEFINE(1, 3, 3, i2s2_ext_rx);
DMA_STREAM_DEFINE(1, 3, 4, i2c3_tx);
DMA_STREAM_DEFINE(1, 3, 5, tim2_ch1);
DMA_STREAM_DEFINE(1, 3, 6, tim2_ch2);
DMA_STREAM_DEFINE(1, 3, 6, tim2_ch4);
DMA_STREAM_DEFINE(1, 3, 7, tim2_up);
DMA_STREAM_DEFINE(1, 3, 7, tim2_ch4);

DMA_STREAM_DEFINE(1, 4, 0, uart5_rx);
DMA_STREAM_DEFINE(1, 4, 1, usart3_rx);
DMA_STREAM_DEFINE(1, 4, 2, uart4_rx);
DMA_STREAM_DEFINE(1, 4, 3, usart3_tx);
DMA_STREAM_DEFINE(1, 4, 4, uart4_tx);
DMA_STREAM_DEFINE(1, 4, 5, usart2_rx);
DMA_STREAM_DEFINE(1, 4, 6, usart2_tx);
DMA_STREAM_DEFINE(1, 4, 7, uart5_tx);

DMA_STREAM_DEFINE(2, 0, 0, adc1);
DMA_STREAM_DEFINE(2, 0, 1, sai1_a);
DMA_STREAM_DEFINE(2, 0, 2, tim8_ch1);
DMA_STREAM_DEFINE(2, 0, 2, tim8_ch2);
DMA_STREAM_DEFINE(2, 0, 2, tim8_ch3);
DMA_STREAM_DEFINE(2, 0, 3, sai1_a);
DMA_STREAM_DEFINE(2, 0, 4, adc1);
DMA_STREAM_DEFINE(2, 0, 5, sai1_b);
DMA_STREAM_DEFINE(2, 0, 6, tim1_ch1);
DMA_STREAM_DEFINE(2, 0, 6, tim1_ch2);
DMA_STREAM_DEFINE(2, 0, 6, tim1_ch3);

DMA_STREAM_DEFINE(2, 1, 1, dcmi);
DMA_STREAM_DEFINE(2, 1, 2, adc2);
DMA_STREAM_DEFINE(2, 1, 3, adc2);
DMA_STREAM_DEFINE(2, 1, 4, sai1_b);
DMA_STREAM_DEFINE(2, 1, 5, spi6_tx);
DMA_STREAM_DEFINE(2, 1, 6, spi6_rx);
DMA_STREAM_DEFINE(2, 1, 7, dcmi);

DMA_STREAM_DEFINE(2, 2, 0, adc3);
DMA_STREAM_DEFINE(2, 2, 1, adc3);
DMA_STREAM_DEFINE(2, 2, 2, spi1_tx);
DMA_STREAM_DEFINE(2, 2, 3, spi5_rx);
DMA_STREAM_DEFINE(2, 2, 4, spi5_tx);
DMA_STREAM_DEFINE(2, 2, 5, cryp_out);
DMA_STREAM_DEFINE(2, 2, 6, cryp_in);
DMA_STREAM_DEFINE(2, 2, 7, hash_in);

DMA_STREAM_DEFINE(2, 3, 0, spi1_rx);
DMA_STREAM_DEFINE(2, 3, 2, spi1_rx);
DMA_STREAM_DEFINE(2, 3, 3, spi1_tx);
DMA_STREAM_DEFINE(2, 3, 5, spi1_tx);

DMA_STREAM_DEFINE(2, 4, 0, spi4_rx);
DMA_STREAM_DEFINE(2, 4, 1, spi4_tx);
DMA_STREAM_DEFINE(2, 4, 2, usart1_rx);
DMA_STREAM_DEFINE(2, 4, 3, sdio);
DMA_STREAM_DEFINE(2, 4, 4, spi4_rx);
DMA_STREAM_DEFINE(2, 4, 5, usart1_rx);
DMA_STREAM_DEFINE(2, 4, 6, sdio);
DMA_STREAM_DEFINE(2, 4, 7, usart1_tx);

DMA_STREAM_DEFINE(2, 5, 1, usart6_rx);
DMA_STREAM_DEFINE(2, 5, 2, usart6_rx);
DMA_STREAM_DEFINE(2, 5, 3, spi4_rx);
DMA_STREAM_DEFINE(2, 5, 4, spi4_tx);
DMA_STREAM_DEFINE(2, 5, 5, spi5_tx);
DMA_STREAM_DEFINE(2, 5, 6, usart6_tx);
DMA_STREAM_DEFINE(2, 5, 7, usart6_tx);

DMA_STREAM_DEFINE(2, 6, 0, tim1_trig);
DMA_STREAM_DEFINE(2, 6, 1, tim1_ch1);
DMA_STREAM_DEFINE(2, 6, 2, tim1_ch2);
DMA_STREAM_DEFINE(2, 6, 3, tim1_ch1);
DMA_STREAM_DEFINE(2, 6, 4, tim1_ch4);
DMA_STREAM_DEFINE(2, 6, 4, tim1_trig);
DMA_STREAM_DEFINE(2, 6, 4, tim1_com);
DMA_STREAM_DEFINE(2, 6, 5, tim1_up);
DMA_STREAM_DEFINE(2, 6, 6, tim1_ch3);

DMA_STREAM_DEFINE(2, 7, 1, tim8_up);
DMA_STREAM_DEFINE(2, 7, 2, tim8_ch1);
DMA_STREAM_DEFINE(2, 7, 3, tim8_ch2);
DMA_STREAM_DEFINE(2, 7, 4, tim8_ch3);
DMA_STREAM_DEFINE(2, 7, 5, spi5_rx);
DMA_STREAM_DEFINE(2, 7, 6, spi5_tx);
DMA_STREAM_DEFINE(2, 7, 7, tim8_ch4);
DMA_STREAM_DEFINE(2, 7, 7, tim8_trig);
DMA_STREAM_DEFINE(2, 7, 7, tim8_com);

#define SPI_CFG_DEFINE(n) \
    struct stm32_spi_cfg spi ## n ## _cfg = {                   \
        .spi_num = n,                                           \
        .spi = SPI ## n,                                        \
        .i2s_irq = SPI ## n ## _IRQn,                           \
        .driver_data = &stm32_i2s ## n,                         \
        .irq_handler = i2s ## n ## _irq_handler,                \
        .i2s_dma_handler = i2s ## n ## _dma_stream_irq_handler, \
        .hdma_spi = &hdma_spi ## n,                             \
        .enable_clock = i2s ## n ## _clock_enable,              \
    }

#define I2S_CFG_DEFINE(n) \
    struct stm32_spi_cfg spi ## n ## _cfg = {                         \
        .spi_num = n,                                                 \
        .spi = SPI ## n,                                              \
        .i2s_irq = SPI ## n ## _IRQn,                                 \
        .driver_data = &stm32_i2s ## n,                               \
        .irq_handler = i2s ## n ## _irq_handler,                      \
        .i2s_dma_handler = i2s ## n ## _dma_stream_irq_handler,       \
        .i2sext_dma_handler = i2s ## n ## ext_dma_stream_irq_handler, \
        .hdma_spi = &hdma_spi ## n,                                   \
        .hdma_i2sext = &hdma_i2s ## n ## ext,                         \
        .enable_clock = i2s ## n ## _clock_enable,                    \
    }

#ifdef SPI1
static DMA_HandleTypeDef hdma_spi1;
SPI_CFG_DEFINE(1);
#endif
#ifdef SPI2
static DMA_HandleTypeDef hdma_spi2;
#ifdef I2S2ext
static DMA_HandleTypeDef hdma_i2s2ext;
I2S_CFG_DEFINE(2);
#else
SPI_CFG_DEFINE(2);
#endif
#endif
#ifdef SPI3
static DMA_HandleTypeDef hdma_spi3;
#ifdef I2S3ext
static DMA_HandleTypeDef hdma_i2s3ext;
I2S_CFG_DEFINE(3);
#else
SPI_CFG_DEFINE(3);
#endif
#endif
#ifdef SPI4
static DMA_HandleTypeDef hdma_spi4;
SPI_CFG_DEFINE(4);
#endif
#ifdef SPI5
static DMA_HandleTypeDef hdma_spi5;
SPI_CFG_DEFINE(5);
#endif
