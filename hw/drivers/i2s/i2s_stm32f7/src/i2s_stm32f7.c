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
#include <i2s_stm32f7/i2s_stm32f7.h>

struct stm32_spi_cfg {
    uint8_t spi_num;
    SPI_TypeDef *spi;
    IRQn_Type i2s_irq;
    struct stm32_i2s *driver_data;
    DMA_HandleTypeDef *hdma_spi;
    void (*irq_handler)(void);
    void (*i2s_dma_handler)(void);
    void (*enable_clock)(bool enable);
};

static struct stm32_i2s stm32_i2s1;
static struct stm32_i2s stm32_i2s2;
static struct stm32_i2s stm32_i2s3;

void
i2s1_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_I2S_IRQHandler(&stm32_i2s1.hi2s);

    os_trace_isr_exit();
}

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
i2s1_dma_stream_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_i2s1.hdma_spi);

    os_trace_isr_exit();
}

static void
i2s2_dma_stream_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_i2s2.hdma_spi);

    os_trace_isr_exit();
}

static void
i2s3_dma_stream_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_i2s3.hdma_spi);

    os_trace_isr_exit();
}

static void
i2s_init_interrupts(const struct i2s_cfg *cfg)
{
    NVIC_SetVector(cfg->dma_cfg->dma_stream_irq, (uint32_t)cfg->spi_cfg->i2s_dma_handler);
    HAL_NVIC_SetPriority(cfg->dma_cfg->dma_stream_irq, 5, 0);
    HAL_NVIC_EnableIRQ(cfg->dma_cfg->dma_stream_irq);

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
    if (pins->mck_pin) {
        hal_gpio_init_stm(pins->mck_pin->pin, (GPIO_InitTypeDef *)&pins->mck_pin->hal_init);
    }
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

    stm32_i2s = cfg->spi_cfg->driver_data;
    stm32_i2s->i2s = i2s;
    stm32_i2s->hdma_spi = cfg->spi_cfg->hdma_spi;

    i2s->sample_rate = cfg->sample_rate;
    i2s->driver_data = stm32_i2s;

    i2s_init_pins((struct stm32_i2s_pins *)&cfg->pins);

    cfg->spi_cfg->enable_clock(true);

    stm32_i2s->hi2s.Instance = cfg->spi_cfg->spi;
    stm32_i2s->hi2s.Init.Mode = cfg->mode;
    stm32_i2s->hi2s.Init.Standard = cfg->standard;
    stm32_i2s->hi2s.Init.DataFormat = cfg->data_format;
    stm32_i2s->hi2s.Init.MCLKOutput = (cfg->pins.mck_pin) ? I2S_MCLKOUTPUT_ENABLE : I2S_MCLKOUTPUT_DISABLE;
    stm32_i2s->hi2s.Init.AudioFreq = cfg->sample_rate;
    stm32_i2s->hi2s.Init.CPOL = I2S_CPOL_LOW;
    stm32_i2s->hi2s.Init.ClockSource = I2S_CLOCK_PLL;

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
    if (i2s_data->dma_buffer_count > 0) {
        buffer = i2s_data->dma_buffers[0];
        i2s_data->dma_buffers[0] = NULL;
        i2s_driver_buffer_put(i2s, buffer);
        if (i2s_data->dma_buffer_count > 1) {
            buffer = i2s_data->dma_buffers[1];
            i2s_driver_buffer_put(i2s, buffer);
        }
        i2s_data->dma_buffers[1] = NULL;
        i2s_data->dma_buffer_count = 0;
    }
    HAL_I2S_DeInit(&i2s_data->hi2s);
    HAL_DMA_DeInit(i2s_data->hdma_spi);
    return 0;
}

static void
i2s_dma_complete(DMA_HandleTypeDef *hdma, HAL_DMA_MemoryTypeDef memory)
{
    I2S_HandleTypeDef *hi2s = (I2S_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent; /* Derogation MISRAC2012-Rule-11.5 */
    struct stm32_i2s *i2s_data = (struct stm32_i2s *)hi2s;
    struct i2s *i2s = i2s_data->i2s;
    struct i2s_sample_buffer *processed_buffer = NULL;
    const int ix = (int)memory;

    if (i2s_data->dma_buffer_count == 2) {
        /* There were already two different memory buffers, one can be returned to the user */
        processed_buffer = i2s_data->dma_buffers[ix];
        i2s_data->dma_buffers[ix] = i2s_driver_buffer_get(i2s);
        /* If there are not more waiting buffers, use same buffer again */
        if (i2s_data->dma_buffers[ix] == NULL) {
            i2s_data->dma_buffer_count = 1;
            i2s_data->dma_buffers[ix] = i2s_data->dma_buffers[ix ^ 1];
        }
        HAL_DMAEx_ChangeMemory(hdma, (uint32_t)i2s_data->dma_buffers[ix]->sample_data, memory);
        processed_buffer->sample_count = processed_buffer->capacity;
        i2s_driver_buffer_put(i2s, processed_buffer);
    }
}

static void
i2s_dma_m0_complete(DMA_HandleTypeDef *hdma)
{
    i2s_dma_complete(hdma, MEMORY0);
}

static void
i2s_dma_m1_complete(DMA_HandleTypeDef *hdma)
{
    i2s_dma_complete(hdma, MEMORY1);
}

/*
 * Following functions:
 * i2s_dma_error(), i2s_receive_start_dma() i2s_transmit_start_dma(), i2s_transmit_receive_dma()
 * are close copies of ST HAL functions with exception that they program DMA in double buffering
 * mode.  Style and naming is in most part unchanged.
 */
static void
i2s_dma_error(DMA_HandleTypeDef *hdma)
{
    I2S_HandleTypeDef *hi2s = (I2S_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent; /* Derogation MISRAC2012-Rule-11.5 */

    /* Disable Rx and Tx DMA Request */
    CLEAR_BIT(hi2s->Instance->CR2, (SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN));
    hi2s->TxXferCount = 0U;
    hi2s->RxXferCount = 0U;

    hi2s->State = HAL_I2S_STATE_READY;

    /* Set the error code and execute error callback */
    SET_BIT(hi2s->ErrorCode, HAL_I2S_ERROR_DMA);
    /* Call user error callback */
#if (USE_HAL_I2S_REGISTER_CALLBACKS == 1U)
    hi2s->ErrorCallback(hi2s);
#else
    HAL_I2S_ErrorCallback(hi2s);
#endif /* USE_HAL_I2S_REGISTER_CALLBACKS */
}

static HAL_StatusTypeDef
i2s_receive_start_dma(I2S_HandleTypeDef *hi2s, uint16_t *buf0, uint16_t *buf1, uint16_t sample_count)
{
    uint32_t tmpreg_cfgr;
    uint16_t size;

    if ((buf0 == NULL) || (buf1 == NULL) || (sample_count == 0)) {
        return HAL_ERROR;
    }

    /* Process Locked */
    __HAL_LOCK(hi2s);

    if (hi2s->State != HAL_I2S_STATE_READY) {
        __HAL_UNLOCK(hi2s);
        return HAL_BUSY;
    }

    /* Set state and reset error code */
    hi2s->State = HAL_I2S_STATE_BUSY_RX;
    hi2s->ErrorCode = HAL_I2S_ERROR_NONE;
    hi2s->pRxBuffPtr = buf0;

    tmpreg_cfgr = hi2s->Instance->I2SCFGR & (SPI_I2SCFGR_DATLEN | SPI_I2SCFGR_CHLEN);

    if ((tmpreg_cfgr == I2S_DATAFORMAT_24B) || (tmpreg_cfgr == I2S_DATAFORMAT_32B)) {
        size = sample_count << 1U;
    } else {
        size = sample_count;
    }

    /* Half transfer callback not needed */
    hi2s->hdmarx->XferHalfCpltCallback = NULL;

    /* Set the I2S Rx DMA transfer complete callback */
    hi2s->hdmarx->XferCpltCallback = i2s_dma_m0_complete;
    hi2s->hdmarx->XferM1CpltCallback = i2s_dma_m1_complete;

    /* Set the DMA error callback */
    hi2s->hdmarx->XferErrorCallback = i2s_dma_error;

    /* Check if Master Receiver mode is selected */
    if ((hi2s->Instance->I2SCFGR & SPI_I2SCFGR_I2SCFG) == I2S_MODE_MASTER_RX) {
        /*
         * Clear the Overrun Flag by a read operation to the SPI_DR register followed by a read
         * access to the SPI_SR register.
         */
        __HAL_I2S_CLEAR_OVRFLAG(hi2s);
    }

    hi2s->hdmarx->Instance->CR &= ~DMA_SxCR_CT_Msk;
    /* Enable the Rx DMA Stream/Channel */
    if (HAL_OK != HAL_DMAEx_MultiBufferStart_IT(hi2s->hdmarx, (uint32_t)&hi2s->Instance->DR,
                                                (uint32_t)buf0, (uint32_t)buf1, size)) {
        /* Update SPI error code */
        SET_BIT(hi2s->ErrorCode, HAL_I2S_ERROR_DMA);
        hi2s->State = HAL_I2S_STATE_READY;

        __HAL_UNLOCK(hi2s);
        return HAL_ERROR;
    }

    /* Check if the I2S is already enabled */
    if (HAL_IS_BIT_CLR(hi2s->Instance->I2SCFGR, SPI_I2SCFGR_I2SE)) {
        /* Enable I2S peripheral */
        __HAL_I2S_ENABLE(hi2s);
    }

    /* Check if the I2S Rx request is already enabled */
    if (HAL_IS_BIT_CLR(hi2s->Instance->CR2, SPI_CR2_RXDMAEN)) {
        /* Enable Rx DMA Request */
        SET_BIT(hi2s->Instance->CR2, SPI_CR2_RXDMAEN);
    }

    __HAL_UNLOCK(hi2s);
    return HAL_OK;
}

static HAL_StatusTypeDef
i2s_transmit_start_dma(I2S_HandleTypeDef *hi2s, uint16_t *buf0, uint16_t *buf1, uint16_t sample_count)
{
    uint32_t tmpreg_cfgr;
    uint16_t size;

    if ((buf0 == NULL) || (buf1 == NULL) || (sample_count == 0)) {
        return HAL_ERROR;
    }

    /* Process Locked */
    __HAL_LOCK(hi2s);

    if (hi2s->State != HAL_I2S_STATE_READY) {
        __HAL_UNLOCK(hi2s);
        return HAL_BUSY;
    }

    /* Set state and reset error code */
    hi2s->State = HAL_I2S_STATE_BUSY_TX;
    hi2s->ErrorCode = HAL_I2S_ERROR_NONE;
    hi2s->pTxBuffPtr = buf0;

    tmpreg_cfgr = hi2s->Instance->I2SCFGR & (SPI_I2SCFGR_DATLEN | SPI_I2SCFGR_CHLEN);

    if ((tmpreg_cfgr == I2S_DATAFORMAT_24B) || (tmpreg_cfgr == I2S_DATAFORMAT_32B)) {
        size = sample_count << 1U;
    } else {
        size = sample_count;
    }

    /* Set the I2S Tx DMA Half transfer complete callback */
    hi2s->hdmatx->XferHalfCpltCallback = NULL;

    /* Set the I2S Tx DMA transfer complete callback */
    hi2s->hdmatx->XferCpltCallback = i2s_dma_m0_complete;
    hi2s->hdmatx->XferM1CpltCallback = i2s_dma_m1_complete;

    /* Set the DMA error callback */
    hi2s->hdmatx->XferErrorCallback = i2s_dma_error;

    hi2s->hdmatx->Instance->CR &= ~DMA_SxCR_CT_Msk;
    /* Enable the Tx DMA Stream/Channel */
    if (HAL_OK != HAL_DMAEx_MultiBufferStart_IT(hi2s->hdmatx, (uint32_t)buf0, (uint32_t)&hi2s->Instance->DR,
                                                (uint32_t)buf1, size)) {
        /* Update SPI error code */
        SET_BIT(hi2s->ErrorCode, HAL_I2S_ERROR_DMA);
        hi2s->State = HAL_I2S_STATE_READY;

        __HAL_UNLOCK(hi2s);
        return HAL_ERROR;
    }

    /* Check if the I2S is already enabled */
    if (HAL_IS_BIT_CLR(hi2s->Instance->I2SCFGR, SPI_I2SCFGR_I2SE)) {
        /* Enable I2S peripheral */
        __HAL_I2S_ENABLE(hi2s);
    }

    /* Check if the I2S Rx request is already enabled */
    if (HAL_IS_BIT_CLR(hi2s->Instance->CR2, SPI_CR2_TXDMAEN)) {
        /* Enable Rx DMA Request */
        SET_BIT(hi2s->Instance->CR2, SPI_CR2_TXDMAEN);
    }

    __HAL_UNLOCK(hi2s);
    return HAL_OK;
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
    /* Fallthrough */
    case HAL_I2S_STATE_READY:
        assert(i2s_data->dma_buffers[0] == NULL);
        assert(i2s_data->dma_buffers[1] == NULL);
        assert(i2s_data->dma_buffer_count == 0);
        i2s_data->dma_buffers[0] = i2s_driver_buffer_get(i2s);
        i2s_data->dma_buffers[1] = i2s_driver_buffer_get(i2s);
        if (i2s_data->dma_buffers[0] == NULL) {
            i2s->state = I2S_STATE_OUT_OF_BUFFERS;
            rc = I2S_ERR_NO_BUFFER;
            break;
        }
        if (i2s_data->dma_buffers[1] == NULL) {
            i2s_data->dma_buffers[1] = i2s_data->dma_buffers[0];
            i2s_data->dma_buffer_count = 1;
        } else {
            i2s_data->dma_buffer_count = 2;
        }
        i2s->state = I2S_STATE_RUNNING;
        if (i2s->direction == I2S_IN) {
            i2s_data->dma_buffers[0]->sample_count = i2s_data->dma_buffers[0]->capacity;
            assert(i2s_data->dma_buffers[0]->capacity == i2s_data->dma_buffers[1]->capacity);
            i2s_receive_start_dma(&i2s_data->hi2s, i2s_data->dma_buffers[0]->sample_data,
                                  i2s_data->dma_buffers[1]->sample_data, i2s_data->dma_buffers[0]->sample_count);
        } else if (i2s->direction == I2S_OUT) {
            i2s_transmit_start_dma(&i2s_data->hi2s,
                                   i2s_data->dma_buffers[0]->sample_data,
                                   i2s_data->dma_buffers[1]->sample_data,
                                   i2s_data->dma_buffers[0]->sample_count);
        }
        break;
    case HAL_I2S_STATE_BUSY:
    case HAL_I2S_STATE_BUSY_RX:
    case HAL_I2S_STATE_BUSY_TX:
        break;
    default:
        rc = I2S_ERR_INTERNAL;
    }
    return rc;
}

void
i2s_driver_buffer_queued(struct i2s *i2s)
{
    struct stm32_i2s *i2s_data = (struct stm32_i2s *)i2s->driver_data;
    struct i2s_sample_buffer *next_buffer;
    int sr;
    int inactive_memory_buffer_ix;
    uint32_t sample_buffer_addr;

    if (i2s->state != I2S_STATE_RUNNING) {
        return;
    }
    OS_ENTER_CRITICAL(sr);
    switch (i2s_data->dma_buffer_count) {
    case 0:
        i2s_data->dma_buffers[0] = i2s_driver_buffer_get(i2s);
        i2s_data->dma_buffer_count = 1;
        break;
    case 1:
        next_buffer = i2s_driver_buffer_get(i2s);
        if ((i2s_data->hdma_spi->Instance->CR & DMA_SxCR_EN) == 0) {
            i2s_data->dma_buffers[1] = next_buffer;
        } else {
            sample_buffer_addr = (uint32_t)next_buffer->sample_data;
            /* DMA is active with just one buffer, inactive buffer needs to be changed. */
            inactive_memory_buffer_ix = (i2s_data->hdma_spi->Instance->CR & DMA_SxCR_CT_Msk) ? 0 : 1;
            HAL_DMAEx_ChangeMemory(i2s_data->hdma_spi, sample_buffer_addr,
                                   inactive_memory_buffer_ix ? MEMORY1 : MEMORY0);
            if ((i2s_data->hdma_spi->Instance->CR & DMA_SxCR_EN_Msk) == 0) {
                /*
                 * There was race between checking current buffer and settings next.
                 * In this case M1AR or M0AR was write protected and write did not succeeded,
                 * Just write the other memory address.
                 */
                inactive_memory_buffer_ix ^= 1;
                HAL_DMAEx_ChangeMemory(i2s_data->hdma_spi, sample_buffer_addr,
                                       inactive_memory_buffer_ix ? MEMORY1 : MEMORY0);

                /* Writing to MxAR that was used stopped transfer with errors, make it continue */
                __HAL_DMA_CLEAR_FLAG(i2s_data->hdma_spi,
                                     (DMA_FLAG_FEIF0_4 | DMA_FLAG_DMEIF0_4 |
                                      DMA_FLAG_TEIF0_4 | DMA_FLAG_HTIF0_4 |
                                      DMA_FLAG_TCIF0_4) << i2s_data->hdma_spi->StreamIndex);
                i2s_data->hdma_spi->Instance->CR |= DMA_SxCR_EN;
            }
            i2s_data->dma_buffers[inactive_memory_buffer_ix] = next_buffer;
        }
        i2s_data->dma_buffer_count = 2;
        break;
    default:
        break;
    }
    OS_EXIT_CRITICAL(sr);
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
I2S_CK_PIN_DEFINE(1, G, 11, GPIO_AF5_SPI1);

/* I2S1 possible WS pins */
I2S_WS_PIN_DEFINE(1, A, 4, GPIO_AF5_SPI1);
I2S_WS_PIN_DEFINE(1, A, 15, GPIO_AF5_SPI1);
I2S_WS_PIN_DEFINE(1, G, 10, GPIO_AF5_SPI1);

/* I2S1 possible SD pins */
I2S_SD_PIN_DEFINE(1, B, 5, GPIO_AF5_SPI1);
I2S_SD_PIN_DEFINE(1, A, 7, GPIO_AF5_SPI1);
I2S_SD_PIN_DEFINE(1, D, 7, GPIO_AF5_SPI1);

/* I2S1 possible MCK pins */
I2S_SD_PIN_DEFINE(1, C, 4, GPIO_AF5_SPI1);

/* I2S2 Possible CKIN pins */
I2S_PIN_DEFINE(2, C, 9, GPIO_AF5_SPI2);

/* I2S2 Possible MCK pins */
#ifdef GPIO_AF5_SPI2
I2S_PIN_DEFINE(2, C, 6, GPIO_AF5_SPI2);
#endif

/* I2S2 Possible CK pins */
I2S_CK_PIN_DEFINE(2, A, 9, GPIO_AF5_SPI2);
I2S_CK_PIN_DEFINE(2, A, 12, GPIO_AF5_SPI2);
I2S_CK_PIN_DEFINE(2, B, 10, GPIO_AF5_SPI2);
I2S_CK_PIN_DEFINE(2, B, 13, GPIO_AF5_SPI2);
I2S_CK_PIN_DEFINE(2, D, 3, GPIO_AF5_SPI2);
I2S_CK_PIN_DEFINE(2, I, 1, GPIO_AF5_SPI2);

/* I2S2 possible WS pins */
I2S_WS_PIN_DEFINE(2, A, 11, GPIO_AF5_SPI2);
I2S_WS_PIN_DEFINE(2, B, 4, GPIO_AF7_SPI2);
I2S_WS_PIN_DEFINE(2, B, 9, GPIO_AF5_SPI2);
I2S_WS_PIN_DEFINE(2, B, 12, GPIO_AF5_SPI2);
I2S_WS_PIN_DEFINE(2, I, 0, GPIO_AF5_SPI2);

/* I2S2 possible SD pins */
I2S_SD_PIN_DEFINE(2, B, 15, GPIO_AF5_SPI2);
I2S_SD_PIN_DEFINE(2, C, 1, GPIO_AF5_SPI2);
I2S_SD_PIN_DEFINE(2, C, 3, GPIO_AF5_SPI2);
I2S_SD_PIN_DEFINE(2, I, 3, GPIO_AF5_SPI2);

/* I2S3 possible CK pins */
I2S_CK_PIN_DEFINE(3, B, 3, GPIO_AF6_SPI3);
I2S_CK_PIN_DEFINE(3, C, 10, GPIO_AF6_SPI3);

/* I2S3 possible WS pins */
I2S_WS_PIN_DEFINE(3, A, 4, GPIO_AF6_SPI3);
I2S_WS_PIN_DEFINE(3, A, 15, GPIO_AF6_SPI3);

/* I2S3 possible SD pins */
I2S_SD_PIN_DEFINE(3, B, 2, GPIO_AF6_SPI3);
I2S_SD_PIN_DEFINE(3, B, 5, GPIO_AF6_SPI3);
I2S_SD_PIN_DEFINE(3, C, 12, GPIO_AF6_SPI3);
#ifdef GPIO_AF5_SPI3
I2S_SD_PIN_DEFINE(3, D, 6, GPIO_AF5_SPI3);
#endif

/* I2S3 possible MCK pins */
I2S_PIN_DEFINE(3, C, 7, GPIO_AF6_SPI3);

#define DMA_STREAM_DEFINE(dma, ch, st, name) \
    const struct stm32_dma_cfg name ## _stream ## st ## _channel ## ch = {  \
        dma,                                                                \
        DMA ## dma ## _Stream ## st ## _IRQn,                               \
        DMA ## dma ## _Stream ## st,                                        \
        DMA_CHANNEL_ ## ch,                                                 \
    }

DMA_STREAM_DEFINE(1, 0, 0, spi3_rx);
DMA_STREAM_DEFINE(1, 0, 1, spdifrx_dt);
DMA_STREAM_DEFINE(1, 0, 2, spi3_rx);
DMA_STREAM_DEFINE(1, 0, 3, spi2_rx);
DMA_STREAM_DEFINE(1, 0, 4, spi2_tx);
DMA_STREAM_DEFINE(1, 0, 5, spi3_tx);
DMA_STREAM_DEFINE(1, 0, 6, spdifrx_cs);
DMA_STREAM_DEFINE(1, 0, 7, spi3_tx);

DMA_STREAM_DEFINE(1, 1, 0, i2c1_rx);
DMA_STREAM_DEFINE(1, 1, 1, i2c3_rx);
DMA_STREAM_DEFINE(1, 1, 2, tim7_up);
DMA_STREAM_DEFINE(1, 1, 4, tim7_up);
DMA_STREAM_DEFINE(1, 1, 5, i2c1_rx);
DMA_STREAM_DEFINE(1, 1, 6, i2c1_tx);
DMA_STREAM_DEFINE(1, 1, 7, i2c1_tx);

DMA_STREAM_DEFINE(1, 2, 0, tim4_ch1);
DMA_STREAM_DEFINE(1, 2, 2, i2c4_rx);
DMA_STREAM_DEFINE(1, 2, 3, tim4_ch2);
DMA_STREAM_DEFINE(1, 2, 5, i2c4_rx);
DMA_STREAM_DEFINE(1, 2, 6, tim4_up);
DMA_STREAM_DEFINE(1, 2, 7, tim4_ch3);

DMA_STREAM_DEFINE(1, 3, 1, tim2_up);
DMA_STREAM_DEFINE(1, 3, 1, tim2_ch3);
DMA_STREAM_DEFINE(1, 3, 2, i2c3_rx);
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

DMA_STREAM_DEFINE(1, 5, 0, uart8_tx);
DMA_STREAM_DEFINE(1, 5, 1, uart7_tx);
DMA_STREAM_DEFINE(1, 5, 2, tim3_ch4);
DMA_STREAM_DEFINE(1, 5, 2, tim3_up);
DMA_STREAM_DEFINE(1, 5, 3, uart7_rx);
DMA_STREAM_DEFINE(1, 5, 4, tim3_ch1);
DMA_STREAM_DEFINE(1, 5, 4, tim3_trig);
DMA_STREAM_DEFINE(1, 5, 5, tim3_ch2);
DMA_STREAM_DEFINE(1, 5, 6, uart8_rx);
DMA_STREAM_DEFINE(1, 5, 7, tim3_ch3);

DMA_STREAM_DEFINE(1, 6, 0, tim5_ch3);
DMA_STREAM_DEFINE(1, 6, 0, tim5_up);
DMA_STREAM_DEFINE(1, 6, 1, tim5_ch4);
DMA_STREAM_DEFINE(1, 6, 1, tim5_trig);
DMA_STREAM_DEFINE(1, 6, 2, tim5_ch1);
DMA_STREAM_DEFINE(1, 6, 2, tim3_up);
DMA_STREAM_DEFINE(1, 6, 3, tim5_ch4);
DMA_STREAM_DEFINE(1, 6, 3, tim5_trig);
DMA_STREAM_DEFINE(1, 6, 4, tim5_ch2);
DMA_STREAM_DEFINE(1, 6, 6, tim5_up);

DMA_STREAM_DEFINE(1, 7, 1, tim6_up);
DMA_STREAM_DEFINE(1, 7, 2, i2c2_rx);
DMA_STREAM_DEFINE(1, 7, 3, i2c2_rx);
DMA_STREAM_DEFINE(1, 7, 4, usart3_tx);
DMA_STREAM_DEFINE(1, 7, 5, dac1);
DMA_STREAM_DEFINE(1, 7, 6, dac2);
DMA_STREAM_DEFINE(1, 7, 7, i2c2_tx);

DMA_STREAM_DEFINE(1, 8, 0, i2c3_tx);
DMA_STREAM_DEFINE(1, 8, 1, i2c4_rx);
DMA_STREAM_DEFINE(1, 8, 4, i2c2_tx);
DMA_STREAM_DEFINE(1, 8, 6, i2c4_tx);

DMA_STREAM_DEFINE(1, 9, 1, i2c2_rx);
DMA_STREAM_DEFINE(1, 9, 6, i2c2_tx);

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
DMA_STREAM_DEFINE(2, 0, 7, sai1_b);

DMA_STREAM_DEFINE(2, 1, 1, dcmi);
DMA_STREAM_DEFINE(2, 1, 2, adc2);
DMA_STREAM_DEFINE(2, 1, 3, adc2);
DMA_STREAM_DEFINE(2, 1, 4, sai1_b);
DMA_STREAM_DEFINE(2, 1, 5, spi6_tx);
DMA_STREAM_DEFINE(2, 1, 6, spi6_rx);
DMA_STREAM_DEFINE(2, 1, 7, dcmi);

DMA_STREAM_DEFINE(2, 2, 0, adc3);
DMA_STREAM_DEFINE(2, 2, 1, adc3);
DMA_STREAM_DEFINE(2, 2, 3, spi5_rx);
DMA_STREAM_DEFINE(2, 2, 4, spi5_tx);
DMA_STREAM_DEFINE(2, 2, 5, cryp_out);
DMA_STREAM_DEFINE(2, 2, 6, cryp_in);
DMA_STREAM_DEFINE(2, 2, 7, hash_in);

DMA_STREAM_DEFINE(2, 3, 0, spi1_rx);
DMA_STREAM_DEFINE(2, 3, 2, spi1_rx);
DMA_STREAM_DEFINE(2, 3, 3, spi1_tx);
DMA_STREAM_DEFINE(2, 3, 4, sai2_a);
DMA_STREAM_DEFINE(2, 3, 5, spi1_tx);
DMA_STREAM_DEFINE(2, 3, 6, sai2_b);
DMA_STREAM_DEFINE(2, 3, 7, quadspi);

DMA_STREAM_DEFINE(2, 4, 0, spi4_rx);
DMA_STREAM_DEFINE(2, 4, 1, spi4_tx);
DMA_STREAM_DEFINE(2, 4, 2, usart1_rx);
DMA_STREAM_DEFINE(2, 4, 3, sdmmc1);
DMA_STREAM_DEFINE(2, 4, 5, usart1_rx);
DMA_STREAM_DEFINE(2, 4, 6, sdmmc1);
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

DMA_STREAM_DEFINE(2, 8, 0, dfsdm1_flt0);
DMA_STREAM_DEFINE(2, 8, 1, dfsdm1_flt1);
DMA_STREAM_DEFINE(2, 8, 2, dfsdm1_flt2);
DMA_STREAM_DEFINE(2, 8, 3, dfsdm1_flt3);
DMA_STREAM_DEFINE(2, 8, 4, dfsdm1_flt0);
DMA_STREAM_DEFINE(2, 8, 5, dfsdm1_flt1);
DMA_STREAM_DEFINE(2, 8, 6, dfsdm1_flt2);
DMA_STREAM_DEFINE(2, 8, 7, dfsdm1_flt3);

DMA_STREAM_DEFINE(2, 9, 0, jpeg_in);
DMA_STREAM_DEFINE(2, 9, 1, jpeg_out);
DMA_STREAM_DEFINE(2, 9, 2, spi4_tx);
DMA_STREAM_DEFINE(2, 9, 3, jpeg_in);
DMA_STREAM_DEFINE(2, 9, 4, jpeg_out);
DMA_STREAM_DEFINE(2, 9, 5, spi5_rx);

DMA_STREAM_DEFINE(2, 10, 0, sai1_b);
DMA_STREAM_DEFINE(2, 10, 1, sai2_b);
DMA_STREAM_DEFINE(2, 10, 2, sai2_a);
DMA_STREAM_DEFINE(2, 10, 6, sai1_a);

DMA_STREAM_DEFINE(2, 11, 0, sdmmc2);
DMA_STREAM_DEFINE(2, 11, 2, suadspi);
DMA_STREAM_DEFINE(2, 11, 5, sdmmc2);

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
SPI_CFG_DEFINE(2);
#endif
#ifdef SPI3
static DMA_HandleTypeDef hdma_spi3;
SPI_CFG_DEFINE(3);
#endif
