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
#include <mcu/stm32_pin_cfg.h>
#include <bsp/bsp.h>
#include <i2s/i2s.h>
#include <i2s/i2s_driver.h>
#include <sai_stm32/sai_stm32.h>
#include <mcu/stm32_clock.h>

#include "stm32h5xx_hal_sai.h"
#include "stm32h5xx_hal_dma.h"
#include "stm32h5xx_hal_dma_ex.h"
#include "stm32h5xx_ll_dma.h"

struct stm32_sai {
    SAI_HandleTypeDef hsai;
    DMA_HandleTypeDef *hdma_sai_tx;
    DMA_HandleTypeDef *hdma_sai_rx;

    struct i2s *i2s;
    stm32_clock_t clock;
    const irq_cfg_t *irq_cfg;
    struct i2s_sample_buffer *dma_buffers[2];
    DMA_QListTypeDef dma_list;
    DMA_NodeTypeDef nodes[2];
    uint8_t dma_buffer_count;
#ifdef STM32_SAI_STATS
    uint32_t frames;
    uint32_t missed_frames;
#endif
};

/* SPI_I2S_FULLDUPLEX_SUPPORT is defined (or not) in ST provided MCU specific header */
#define I2S_FULLDUPLEX_SUPPORT  0

static uint16_t zero_buffer[96];

const DMA_InitTypeDef sai_a_tx_dma_init = {
    .Request = GPDMA1_REQUEST_SAI1_A,
    .BlkHWRequest = DMA_BREQ_SINGLE_BURST,
    .Direction = DMA_MEMORY_TO_PERIPH,
    .SrcInc = DMA_SINC_INCREMENTED,
    .DestInc = DMA_DINC_FIXED,
    .SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD,
    .DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD,
    .SrcBurstLength = 1,
    .DestBurstLength = 1,
    .TransferEventMode = DMA_TCEM_BLOCK_TRANSFER,
    .Priority = DMA_LOW_PRIORITY_LOW_WEIGHT,
    .Mode = DMA_LINKEDLIST_CIRCULAR,
};

const DMA_InitTypeDef sai_b_tx_dma_init = {
    .Request = GPDMA1_REQUEST_SAI1_B,
    .BlkHWRequest = DMA_BREQ_SINGLE_BURST,
    .Direction = DMA_MEMORY_TO_PERIPH,
    .SrcInc = DMA_SINC_INCREMENTED,
    .DestInc = DMA_DINC_FIXED,
    .SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD,
    .DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD,
    .SrcBurstLength = 1,
    .DestBurstLength = 1,
    .TransferEventMode = DMA_TCEM_BLOCK_TRANSFER,
    .Priority = DMA_LOW_PRIORITY_LOW_WEIGHT,
    .Mode = DMA_LINKEDLIST_CIRCULAR,
};

const DMA_InitTypeDef sai_a_rx_dma_init = {
    .Request = GPDMA1_REQUEST_SAI1_A,
    .BlkHWRequest = DMA_BREQ_SINGLE_BURST,
    .Direction = DMA_PERIPH_TO_MEMORY,
    .SrcInc = DMA_SINC_FIXED,
    .DestInc = DMA_DINC_INCREMENTED,
    .SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD,
    .DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD,
    .SrcBurstLength = 1,
    .DestBurstLength = 1,
    .TransferEventMode = DMA_TCEM_BLOCK_TRANSFER,
    .Priority = DMA_LOW_PRIORITY_LOW_WEIGHT,
    .Mode = DMA_LINKEDLIST_CIRCULAR,
};

const DMA_InitTypeDef sai_b_rx_dma_init = {
    .Request = GPDMA1_REQUEST_SAI1_B,
    .BlkHWRequest = DMA_BREQ_SINGLE_BURST,
    .Direction = DMA_PERIPH_TO_MEMORY,
    .SrcInc = DMA_SINC_FIXED,
    .DestInc = DMA_DINC_INCREMENTED,
    .SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD,
    .DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD,
    .SrcBurstLength = 1,
    .DestBurstLength = 1,
    .TransferEventMode = DMA_TCEM_BLOCK_TRANSFER,
    .Priority = DMA_LOW_PRIORITY_LOW_WEIGHT,
    .Mode = DMA_LINKEDLIST_CIRCULAR,
};

void
nvic_config_isr(const irq_cfg_t *cfg)
{
    NVIC_SetVector(cfg->irqn, (uint32_t)(cfg->isr));
    HAL_NVIC_SetPriority(cfg->irqn, cfg->pri, cfg->sub_pri);
    HAL_NVIC_EnableIRQ(cfg->irqn);
}

struct stm32_sai_cfg {
    uint8_t sai_num;
    SAI_TypeDef *sai;
    IRQn_Type sai_irq;
    struct stm32_sai *driver_data;
    DMA_HandleTypeDef *hdma_sai_rx;
    DMA_HandleTypeDef *hdma_sai_tx;
    void (*sai_irq_handler)(void);
    void (*sai_dma_handler)(void);
    void (*enable_clock)(bool enable);
};

#define SAI_IRQ_DEFINE(sai) \
void sai_IRQHandler(void);\
const irq_cfg_t sai##_irq_def = {\
    .irqn = sai##_IRQn,\
    .pri = 1,\
    .sub_pri = 0,\
    .isr = sai##_IRQHandler,\
}

#define DMA_IRQN(dma, ch) GPDMA##dma##_Channel##ch##_IRQn

#define SAI_DMA_ISR_DEFINE(sai, dma, ch, dir) \
void sai##_##dir##_dma_isr(void);\
const irq_cfg_t sai##_##dir##_dma##dma##_Channel##ch##_irq_cfg = {\
    .irqn = DMA_IRQN(dma, ch),\
    .pri = 1,\
    .isr = sai##_##dir##_dma_isr,\
};

#define SAI_DMA_CFG_DEFINE(sai, dma, ch, dir) \
stm32_dma_cfg_t sai##_##dir##_GPDMA##dma##_Channel##ch##_cfg = {\
    .dma_num = dma,\
    .irq_cfg = &sai##_##dir##_dma##dma##_Channel##ch##_irq_cfg,\
    .clock = PER_CLOCK(GPDMA##dma),\
    .dma_channel =  &sai##_##dir##_GPDMA##dma##_Channel##ch,\
}

#define SAI_DMA_TX_DEFINE(sai, dma, ch) \
SAI_DMA_ISR_DEFINE(sai, dma, ch, tx)\
DMA_HandleTypeDef sai##_tx_GPDMA##dma##_Channel##ch = {\
    .Instance = GPDMA##dma##_Channel##ch,\
    .Init = {\
        .Request = GPDMA1_REQUEST_##sai,\
        .BlkHWRequest = DMA_BREQ_SINGLE_BURST,\
        .Direction = DMA_MEMORY_TO_PERIPH,\
        .SrcInc = DMA_SINC_INCREMENTED,\
        .DestInc = DMA_DINC_FIXED,\
        .SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD,\
        .DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD,\
        .SrcBurstLength = 1,\
        .DestBurstLength = 1,\
        .TransferEventMode = DMA_TCEM_EACH_LL_ITEM_TRANSFER,\
        .Priority = DMA_LOW_PRIORITY_LOW_WEIGHT,\
        .Mode = DMA_NORMAL,\
    },\
    .InitLinkedList = {\
        .Priority = DMA_LOW_PRIORITY_MID_WEIGHT,\
        .LinkStepMode = DMA_LSM_FULL_EXECUTION,\
        .LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0,\
        .TransferEventMode = DMA_TCEM_EACH_LL_ITEM_TRANSFER,\
        .LinkedListMode = DMA_LINKEDLIST_CIRCULAR,\
    },\
};\
SAI_DMA_CFG_DEFINE(sai, dma, ch, tx)

#define SAI_DMA_RX_DEFINE(sai, dma, ch) \
SAI_DMA_ISR_DEFINE(sai, dma, ch, rx)\
DMA_HandleTypeDef sai##_rx_GPDMA##dma##_Channel##ch = {\
    .Instance = GPDMA##dma##_Channel##ch,\
    .Init = {\
        .Request = GPDMA1_REQUEST_##sai,\
        .BlkHWRequest = DMA_BREQ_SINGLE_BURST,\
        .Direction = DMA_PERIPH_TO_MEMORY,\
        .SrcInc = DMA_SINC_FIXED,\
        .DestInc = DMA_DINC_INCREMENTED,\
        .SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD,\
        .DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD,\
        .SrcBurstLength = 1,\
        .DestBurstLength = 1,\
        .TransferEventMode = DMA_TCEM_BLOCK_TRANSFER,\
        .Priority = DMA_LOW_PRIORITY_LOW_WEIGHT,\
        .Mode = DMA_LINKEDLIST_CIRCULAR,\
    },\
    .InitLinkedList = {\
    },\
};\
SAI_DMA_CFG_DEFINE(sai, dma, ch, rx)

SAI_DMA_TX_DEFINE(SAI1_A, 1, 0);
SAI_DMA_TX_DEFINE(SAI1_A, 1, 1);
SAI_DMA_TX_DEFINE(SAI1_A, 1, 2);
SAI_DMA_TX_DEFINE(SAI1_A, 1, 3);
SAI_DMA_TX_DEFINE(SAI1_A, 1, 4);
SAI_DMA_TX_DEFINE(SAI1_A, 1, 5);

SAI_DMA_TX_DEFINE(SAI1_B, 1, 0);
SAI_DMA_TX_DEFINE(SAI1_B, 1, 1);
SAI_DMA_TX_DEFINE(SAI1_B, 1, 2);
SAI_DMA_TX_DEFINE(SAI1_B, 1, 3);
SAI_DMA_TX_DEFINE(SAI1_B, 1, 4);
SAI_DMA_TX_DEFINE(SAI1_B, 1, 5);

SAI_DMA_TX_DEFINE(SAI2_A, 1, 0);
SAI_DMA_TX_DEFINE(SAI2_A, 1, 1);
SAI_DMA_TX_DEFINE(SAI2_A, 1, 2);
SAI_DMA_TX_DEFINE(SAI2_A, 1, 3);
SAI_DMA_TX_DEFINE(SAI2_A, 1, 4);
SAI_DMA_TX_DEFINE(SAI2_A, 1, 5);

SAI_DMA_TX_DEFINE(SAI2_B, 1, 0);
SAI_DMA_TX_DEFINE(SAI2_B, 1, 1);
SAI_DMA_TX_DEFINE(SAI2_B, 1, 2);
SAI_DMA_TX_DEFINE(SAI2_B, 1, 3);
SAI_DMA_TX_DEFINE(SAI2_B, 1, 4);
SAI_DMA_TX_DEFINE(SAI2_B, 1, 5);

SAI_DMA_RX_DEFINE(SAI1_A, 1, 0);
SAI_DMA_RX_DEFINE(SAI1_A, 1, 1);
SAI_DMA_RX_DEFINE(SAI1_A, 1, 2);
SAI_DMA_RX_DEFINE(SAI1_A, 1, 3);
SAI_DMA_RX_DEFINE(SAI1_A, 1, 4);
SAI_DMA_RX_DEFINE(SAI1_A, 1, 5);
SAI_DMA_RX_DEFINE(SAI1_B, 1, 0);
SAI_DMA_RX_DEFINE(SAI1_B, 1, 1);
SAI_DMA_RX_DEFINE(SAI1_B, 1, 2);
SAI_DMA_RX_DEFINE(SAI1_B, 1, 3);
SAI_DMA_RX_DEFINE(SAI1_B, 1, 4);
SAI_DMA_RX_DEFINE(SAI1_B, 1, 5);
SAI_DMA_RX_DEFINE(SAI2_A, 1, 0);
SAI_DMA_RX_DEFINE(SAI2_A, 1, 1);
SAI_DMA_RX_DEFINE(SAI2_A, 1, 2);
SAI_DMA_RX_DEFINE(SAI2_A, 1, 3);
SAI_DMA_RX_DEFINE(SAI2_A, 1, 4);
SAI_DMA_RX_DEFINE(SAI2_A, 1, 5);
SAI_DMA_RX_DEFINE(SAI2_B, 1, 0);
SAI_DMA_RX_DEFINE(SAI2_B, 1, 1);
SAI_DMA_RX_DEFINE(SAI2_B, 1, 2);
SAI_DMA_RX_DEFINE(SAI2_B, 1, 3);
SAI_DMA_RX_DEFINE(SAI2_B, 1, 4);
SAI_DMA_RX_DEFINE(SAI2_B, 1, 5);

SAI_IRQ_DEFINE(SAI1);
SAI_IRQ_DEFINE(SAI2);
#ifdef SAI3
SAI_IRQ_DEFINE(SAI3);
#endif

#define SAI_TX(sai_num, block) \
stm32_sai_t _SAI##sai_num##_##block##_tx = {\
    .hsai.Instance = SAI##sai_num##_Block_##block,\
    .hsai.Init.AudioMode = SAI_MODEMASTER_TX,\
    .hsai.Init.Synchro = SAI_ASYNCHRONOUS,\
    .hsai.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE,\
    .hsai.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE,\
    .hsai.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY,\
    .hsai.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K,\
    .hsai.Init.SynchroExt = SAI_SYNCEXT_DISABLE,\
    .hsai.Init.MonoStereoMode = SAI_STEREOMODE,\
    .hsai.Init.CompandingMode = SAI_NOCOMPANDING,\
    .hsai.Init.TriState = SAI_OUTPUT_RELEASED,\
    .clock = SAI##sai_num##_clock,\
    .irq_cfg = &SAI##sai_num##_irq_def,\
};

#define SAI_RX(sai_num, block) \
stm32_sai_t _SAI##sai_num##_##block##_rx = {\
    .hsai.Instance = SAI##sai_num##_Block_##block,\
    .hsai.Init.AudioMode = SAI_MODEMASTER_RX,\
    .hsai.Init.Synchro = SAI_ASYNCHRONOUS,\
    .hsai.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE,\
    .hsai.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE,\
    .hsai.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY,\
    .hsai.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K,\
    .hsai.Init.SynchroExt = SAI_SYNCEXT_DISABLE,\
    .hsai.Init.MonoStereoMode = SAI_STEREOMODE,\
    .hsai.Init.CompandingMode = SAI_NOCOMPANDING,\
    .hsai.Init.TriState = SAI_OUTPUT_RELEASED,\
    .clock = SAI##sai_num##_clock,\
    .irq_cfg = &SAI##sai_num##_irq_def,\
};

SAI_TX(1, A)
SAI_TX(1, B)
SAI_TX(2, A)
SAI_TX(2, B)
#ifdef SAI3
SAI_TX(3, A)
SAI_TX(3, B)
#endif

SAI_RX(1, A)
SAI_RX(1, B)
SAI_RX(2, A)
SAI_RX(2, B)
#ifdef SAI3
SAI_RX(3, A)
SAI_RX(3, B)
#endif

void
SAI1_IRQHandler(void)
{
    os_trace_isr_enter();

    if (SAI1_Block_A->SR & SAI1_Block_A->IMR) {
        HAL_SAI_IRQHandler(&SAI1_A_tx->hsai);
    }
    if (SAI1_Block_B->SR & SAI1_Block_B->IMR) {
        HAL_SAI_IRQHandler(&SAI1_B_tx->hsai);
    }

    os_trace_isr_exit();
}

void
SAI2_IRQHandler(void)
{
    os_trace_isr_enter();

    if (SAI2_Block_A->SR & SAI2_Block_A->IMR) {
        HAL_SAI_IRQHandler(&SAI2_A_tx->hsai);
    }
    if (SAI2_Block_B->SR & SAI2_Block_B->IMR) {
        HAL_SAI_IRQHandler(&SAI2_B_tx->hsai);
    }

    os_trace_isr_exit();
}

#ifdef SAI3
void
SAI3_IRQHandler(void)
{
    os_trace_isr_enter();

    if (SAI2_Block_A->SR & SAI2_Block_A->IMR) {
        HAL_SAI_IRQHandler(&SAI3_A_tx->hsai);
    }
    if (SAI2_Block_B->SR & SAI2_Block_B->IMR) {
        HAL_SAI_IRQHandler(&SAI3_B_tx->hsai);
    }

    os_trace_isr_exit();
}

SAI_IRQ_DEFINE(SAI3);
#endif

#if I2S_FULLDUPLEX_SUPPORT
void
HAL_I2SEx_TxRxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    /* TODO: Finish full duplex mode */
    assert(0);
}
#endif

void
SAI1_A_rx_dma_isr(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(SAI1_A_rx->hdma_sai_rx);

    os_trace_isr_exit();
}

void
SAI1_A_tx_dma_isr(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(SAI1_A_tx->hdma_sai_tx);

    os_trace_isr_exit();
}

void
SAI1_B_rx_dma_isr(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(SAI1_B_rx->hdma_sai_rx);

    os_trace_isr_exit();
}

void
SAI1_B_tx_dma_isr(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(SAI2_B_tx->hdma_sai_tx);

    os_trace_isr_exit();
}

void
SAI2_A_rx_dma_isr(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(SAI2_A_rx->hdma_sai_rx);

    os_trace_isr_exit();
}

void
SAI2_A_tx_dma_isr(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(SAI2_A_tx->hdma_sai_tx);

    os_trace_isr_exit();
}

void
SAI2_B_rx_dma_isr(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(SAI2_B_rx->hdma_sai_rx);

    os_trace_isr_exit();
}

void
SAI2_B_tx_dma_isr(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(SAI2_B_tx->hdma_sai_tx);

    os_trace_isr_exit();
}

static void
sai_init_interrupts(const sai_cfg_t *cfg)
{
    if (cfg->rx_dma_cfg) {
        nvic_config_isr(cfg->rx_dma_cfg->irq_cfg);
    }
    if (cfg->tx_dma_cfg) {
        nvic_config_isr(cfg->tx_dma_cfg->irq_cfg);
    }

    /* SAIx interrupt Init */
    nvic_config_isr(cfg->sai->irq_cfg);
}

static void
sai_init_pins(struct stm32_sai_pins *pins)
{
    hal_gpio_init_out(MCU_GPIO_PORTD(11), 0);
    hal_gpio_init_out(MCU_GPIO_PORTD(12), 0);
    hal_gpio_init_out(MCU_GPIO_PORTD(13), 0);
    for (int i = 0; i < 16; i++) {
        hal_gpio_write(MCU_GPIO_PORTD(11), 1);
        hal_gpio_write(MCU_GPIO_PORTD(12), 1);
        hal_gpio_write(MCU_GPIO_PORTD(13), 1);
        hal_gpio_write(MCU_GPIO_PORTD(11), 0);
        hal_gpio_write(MCU_GPIO_PORTD(12), 0);
        hal_gpio_write(MCU_GPIO_PORTD(13), 0);
    }

    stm32_pin_config(pins->fs_pin);
    stm32_pin_config(pins->mck_pin);
    stm32_pin_config(pins->sck_pin);
    stm32_pin_config(pins->sdi_pin);
    stm32_pin_config(pins->sdo_pin);
}

static int
stm32_sai_init(struct i2s *i2s, const sai_cfg_t *cfg)
{
    int rc = 0;
    struct stm32_sai *stm32_sai;

#if I2S_FULLDUPLEX_SUPPORT
    if (cfg->pins.ext_sd_pin != NULL) {
        i2s->direction = I2S_OUT_IN;
    } else {
        i2s->direction = ((cfg->mode == I2S_MODE_MASTER_TX) ||
                          (cfg->mode == I2S_MODE_SLAVE_TX)) ? I2S_OUT : I2S_IN;
    }
#else
    i2s->direction = cfg->direction;
#endif

    if (cfg->data_size <= SAI_PROTOCOL_DATASIZE_16BITEXTENDED) {
        i2s->sample_size_in_bytes = 2;
    } else {
        i2s->sample_size_in_bytes = 4;
    }

    rc = i2s_init(i2s, cfg->pool);

    if (rc != OS_OK) {
        goto end;
    }

    stm32_sai = cfg->sai;
    stm32_sai->i2s = i2s;
    // stm32_sai->hdma_spi = cfg->hdma_spi; ???
#if I2S_FULLDUPLEX_SUPPORT
    stm32_sai->hdma_i2sext = cfg->spi_cfg->hdma_i2sext;
#endif

    i2s->sample_rate = cfg->sample_rate;
    i2s->driver_data = stm32_sai;

    sai_init_pins((struct stm32_sai_pins *)&cfg->pins);

    stm32_clock_enable(cfg->sai_clock_src);
    stm32_clock_enable(stm32_sai->clock);
    if (cfg->rx_dma_cfg) {
        stm32_sai->hdma_sai_rx = cfg->rx_dma_cfg->dma_channel;
        nvic_config_isr(cfg->rx_dma_cfg->irq_cfg);
        stm32_clock_enable(cfg->rx_dma_cfg->clock);
    }
    if (cfg->tx_dma_cfg) {
        stm32_sai->hdma_sai_tx = cfg->tx_dma_cfg->dma_channel;
        nvic_config_isr(cfg->tx_dma_cfg->irq_cfg);
        stm32_clock_enable(cfg->tx_dma_cfg->clock);
    }

#if I2S_FULLDUPLEX_SUPPORT
    if (i2s->direction == I2S_OUT_IN) {
        stm32_sai->hdma_i2sext->Instance = cfg->dma_i2sext_cfg->dma_stream;
        stm32_sai->hdma_i2sext->Init.Channel = cfg->dma_i2sext_cfg->dma_channel;
        if (cfg->mode == I2S_MODE_MASTER_TX || cfg->mode == I2S_MODE_SLAVE_TX) {
            stm32_sai->hdma_i2sext->Init.Direction = DMA_PERIPH_TO_MEMORY;
            __HAL_LINKDMA(&stm32_sai->hi2s, hdmarx, *stm32_sai->hdma_i2sext);
        } else {
            stm32_sai->hdma_i2sext->Init.Direction = DMA_MEMORY_TO_PERIPH;
            __HAL_LINKDMA(&stm32_sai->hi2s, hdmatx, *stm32_sai->hdma_i2sext);
        }
        stm32_sai->hdma_i2sext->Init.PeriphInc = DMA_PINC_DISABLE;
        stm32_sai->hdma_i2sext->Init.MemInc = DMA_MINC_ENABLE;
        stm32_sai->hdma_i2sext->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        stm32_sai->hdma_i2sext->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        stm32_sai->hdma_i2sext->Init.Mode = DMA_NORMAL;
        stm32_sai->hdma_i2sext->Init.Priority = DMA_PRIORITY_LOW;
        stm32_sai->hdma_i2sext->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    }
#endif

    sai_init_interrupts(cfg);
end:
    return rc;
}

int
sai_create(struct i2s *i2s, const char *name, const sai_cfg_t *cfg)
{
    return os_dev_create(&i2s->dev, name, OS_DEV_INIT_PRIMARY,
                         100, (os_dev_init_func_t)stm32_sai_init, (void *)cfg);
}

int
i2s_create(struct i2s *i2s, const char *name, const sai_cfg_t *cfg)
{
    return os_dev_create(&i2s->dev, name, OS_DEV_INIT_PRIMARY,
                         100, (os_dev_init_func_t)stm32_sai_init, (void *)cfg);
}

int
i2s_driver_stop(struct i2s *i2s)
{
    struct stm32_sai *sai_data = (struct stm32_sai *)i2s->driver_data;
    struct i2s_sample_buffer *buffer;

    HAL_SAI_DMAStop(&sai_data->hsai);
    if (i2s->state == I2S_STATE_RUNNING && i2s->direction == I2S_OUT) {
        /*
         * When DMA is stopped and then I2S peripheral is stopped, it
         * may happen that DMA put some data already in SPI data buffer.
         * In that case single sample may be in the I2S output buffer.
         * If this happens next transmission will swap channels due to
         * one extra sample already present.
         * To avoid this just wait till all samples are gone.
         */
        // if (0 == (i2s_data->hsai.Instance->SR & SAI_SR_TXP_Msk)) {
        //     __HAL_I2S_ENABLE(&i2s_data->hi2s);
        //     while (0 == (i2s_data->hi2s.Instance->SR & SPI_SR_TXP_Msk))
        //         ;
        //     __HAL_I2S_DISABLE(&i2s_data->hi2s);
        // }
    }

    assert(sai_data->hsai.State == HAL_SAI_STATE_READY);
    if (sai_data->dma_buffer_count > 0) {
        buffer = sai_data->dma_buffers[0];
        sai_data->dma_buffers[0] = NULL;
        i2s_driver_buffer_put(i2s, buffer);
        if (sai_data->dma_buffer_count > 1) {
            buffer = sai_data->dma_buffers[1];
            i2s_driver_buffer_put(i2s, buffer);
        }
        sai_data->dma_buffers[1] = NULL;
        sai_data->dma_buffer_count = 0;
    }
    HAL_SAI_DeInit(&sai_data->hsai);
    if (sai_data->hdma_sai_rx) {
        HAL_DMA_DeInit(sai_data->hdma_sai_rx);
    }
    if (sai_data->hdma_sai_tx) {
        HAL_DMA_DeInit(sai_data->hdma_sai_tx);
    }

    return 0;
}

static int
active_buffer_ix(struct stm32_sai *sai_data)
{
    int ix = (sai_data->hsai.hdmatx->Instance->CLLR & 0xFFFF) == (0xFFFF & (uint32_t)(&sai_data->nodes[1])) ? 0 : 1;

    return ix;
}

static void
set_dma_address(struct stm32_sai *sai_data, int ix, void *address)
{
    sai_data->nodes[ix].LinkRegisters[NODE_CSAR_DEFAULT_OFFSET] = (uint32_t)address;
}

/* Callback for when a buffer finishes transmitting */
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
    struct stm32_sai *sai_data = (struct stm32_sai *)hsai;
    struct i2s *i2s = sai_data->i2s;
    struct i2s_sample_buffer *processed_buffer = NULL;
    static struct i2s_sample_buffer *next_buffer;
    int ix;

    hal_gpio_write(ARDUINO_PIN_D10, 1);

    /* Get inactive DMA node index */
    ix = active_buffer_ix(sai_data) ^ 1;

    /* Get associated i2s_buffer (can be NULL) */
    processed_buffer = sai_data->dma_buffers[ix];
    /* Get next buffer queuesd to the driver and set it to DMA */
    next_buffer = i2s_driver_buffer_get(i2s);
    sai_data->dma_buffers[ix] = next_buffer;

    /* Set DMA address of inactive Node to next buffer data or zero_buffer if buffer */
    set_dma_address(sai_data, ix, next_buffer ? next_buffer->sample_data : zero_buffer);

    /* Buffer depende on presence of next and previous buffer */
    sai_data->dma_buffer_count += (processed_buffer ? -1 : 0) + (next_buffer ? 1 : 0);

    /* Return processed buffer */
    if (processed_buffer) {
        processed_buffer->sample_count = processed_buffer->capacity;
        i2s_driver_buffer_put(i2s, processed_buffer);
    }

    if (processed_buffer != NULL && sai_data->dma_buffer_count == 0) {
        /* Run out of buffers */
        stm32_pin_pulse(PF3);
        i2s->client->state_changed_cb(i2s, I2S_STATE_OUT_OF_BUFFERS);
    }
    hal_gpio_write(ARDUINO_PIN_D10, 0);
}

/* Callback for when a buffer finishes receiving */
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
    struct stm32_sai *sai_data = (struct stm32_sai *)hsai;
    struct i2s *i2s = sai_data->i2s;
    struct i2s_sample_buffer *processed_buffer = NULL;
    static struct i2s_sample_buffer *next_buffer;
    int ix;

    hal_gpio_write(ARDUINO_PIN_D10, 1);

    /* Get inactive DMA node index */
    ix = active_buffer_ix(sai_data) ^ 1;

    /* Get associated i2s_buffer (can be NULL) */
    processed_buffer = sai_data->dma_buffers[ix];
    /* Get next buffer queuesd to the driver and set it to DMA */
    next_buffer = i2s_driver_buffer_get(i2s);
    sai_data->dma_buffers[ix] = next_buffer;

    /* Set DMA address of inactive Node to next buffer data or zero_buffer if buffer */
    set_dma_address(sai_data, ix, next_buffer ? next_buffer->sample_data : zero_buffer);

    /* Buffer depende on presence of next and previous buffer */
    sai_data->dma_buffer_count += (processed_buffer ? -1 : 0) + (next_buffer ? 1 : 0);

    /* Return processed buffer */
    if (processed_buffer) {
        processed_buffer->sample_count = processed_buffer->capacity;
        i2s_driver_buffer_put(i2s, processed_buffer);
    }

    if (processed_buffer != NULL && sai_data->dma_buffer_count == 0) {
        /* Run out of buffers */
        stm32_pin_pulse_n(PF3, 4);
        i2s->client->state_changed_cb(i2s, I2S_STATE_OUT_OF_BUFFERS);
    }
    hal_gpio_write(ARDUINO_PIN_D10, 0);
}

// static void
// sai_dma_complete(DMA_HandleTypeDef *hdma, HAL_DMA_MemoryTypeDef memory)
// {
//     SAI_HandleTypeDef *hsai = (SAI_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent; /* Derogation MISRAC2012-Rule-11.5 */
//     struct stm32_sai *sai_data = (struct stm32_sai *)hsai;
//     struct i2s *i2s = sai_data->i2s;
//     struct i2s_sample_buffer *processed_buffer = NULL;
//     const int ix = (int)memory;
//
//     if (sai_data->dma_buffer_count == 2) {
//         /* There were already two different memory buffers, one can be returned to the user */
//         processed_buffer = sai_data->dma_buffers[ix];
//         sai_data->dma_buffers[ix] = i2s_driver_buffer_get(i2s);
//         /* If there are not more waiting buffers, use same buffer again */
//         if (sai_data->dma_buffers[ix] == NULL) {
//             sai_data->dma_buffer_count = 1;
//             sai_data->dma_buffers[ix] = sai_data->dma_buffers[ix ^ 1];
//         }
//         HAL_DMAEx_List_SetCircularModeConfig()
//         HAL_DMAEx_ChangeMemory(hdma, (uint32_t)sai_data->dma_buffers[ix]->sample_data, memory);
//         processed_buffer->sample_count = processed_buffer->capacity;
//         i2s_driver_buffer_put(i2s, processed_buffer);
//     }
// }

/*
 * Following functions:
 * i2s_dma_error(), i2s_receive_start_dma() i2s_transmit_start_dma(), i2s_transmit_receive_dma()
 * are close copies of ST HAL functions with exception that they program DMA in double buffering
 * mode.  Style and naming is in most part unchanged.
 */
void
sai_dma_error(DMA_HandleTypeDef *hdma)
{
//     SAI_HandleTypeDef *hsai = (SAI_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent; /* Derogation MISRAC2012-Rule-11.5 */
//
//     /* Disable Rx and Tx DMA Request */
//     CLEAR_BIT(hi2s->Instance->CFG1, (SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN));
//     hsai->TxXferCount = 0U;
//     hsai->RxXferCount = 0U;
//
//     hsai->State = HAL_SAI_STATE_READY;
//
//     /* Set the error code and execute error callback */
//     SET_BIT(hsai->ErrorCode, HAL_SAI_ERROR_DMA);
//     /* Call user error callback */
// #if (USE_HAL_SAI_REGISTER_CALLBACKS == 1U)
//     hsai->ErrorCallback(hi2s);
// #else
//     HAL_SAI_ErrorCallback(hsai);
// #endif /* USE_HAL_I2S_REGISTER_CALLBACKS */
}

static HAL_StatusTypeDef
sai_receive_start_dma(stm32_sai_t *sai_data)
{
    HAL_StatusTypeDef ret;
    struct i2s_sample_buffer *buffer = sai_data->dma_buffers[1];

    if (buffer != NULL) {
        set_dma_address(sai_data, 1, buffer->sample_data);
    } else {
        set_dma_address(sai_data, 1, zero_buffer);
    }

    buffer = sai_data->dma_buffers[0];
    ret = HAL_SAI_Receive_DMA(&sai_data->hsai, buffer->sample_data, buffer->capacity);

    return ret;
}

static HAL_StatusTypeDef
sai_transmit_start_dma(stm32_sai_t *sai_data)
{
    HAL_StatusTypeDef ret;
    struct i2s_sample_buffer *buffer = sai_data->dma_buffers[1];

    if (buffer != NULL) {
        set_dma_address(sai_data, 1, buffer->sample_data);
    } else {
        set_dma_address(sai_data, 1, zero_buffer);
    }

    buffer = sai_data->dma_buffers[0];
    ret = HAL_SAI_Transmit_DMA(&sai_data->hsai, buffer->sample_data, buffer->sample_count);

    return ret;
}

#if I2S_FULLDUPLEX_SUPPORT
static HAL_StatusTypeDef
i2s_transmit_receive_dma(I2S_HandleTypeDef *hi2s, uint16_t *buf0, uint16_t *buf1, uint16_t sample_count)
{
    uint32_t tmp1 = 0U;
    HAL_StatusTypeDef errorcode = HAL_OK;
    uint16_t size;

    if (hi2s->State != HAL_I2S_STATE_READY) {
        errorcode = HAL_BUSY;
        goto error;
    }

    if ((buf0 == NULL) || (buf1 == NULL) || (sample_count == 0U)) {
        return HAL_ERROR;
    }

    /* Process Locked */
    __HAL_LOCK(hi2s);

    hi2s->pTxBuffPtr = buf0;
    hi2s->pRxBuffPtr = buf0;

    tmp1 = hi2s->Instance->I2SCFGR & (SPI_I2SCFGR_DATLEN | SPI_I2SCFGR_CHLEN);
    /*
     * Check the Data format: When a 16-bit data frame or a 16-bit data frame extended
     * is selected during the I2S configuration phase, the Size parameter means the number
     * of 16-bit data length in the transaction and when a 24-bit data frame or a 32-bit data
     * frame is selected the Size parameter means the number of 16-bit data length.
     */
    if ((tmp1 == I2S_DATAFORMAT_24B) || (tmp1 == I2S_DATAFORMAT_32B)) {
        size = sample_count << 1U;
    } else {
        size = sample_count;
    }
    hi2s->TxXferSize = size;
    hi2s->TxXferCount = size;
    hi2s->RxXferSize = size;
    hi2s->RxXferCount = size;

    hi2s->ErrorCode = HAL_I2S_ERROR_NONE;
    hi2s->State = HAL_I2S_STATE_BUSY_TX_RX;

    /* No Half transfer needed */
    hi2s->hdmarx->XferHalfCpltCallback = NULL;
    hi2s->hdmatx->XferHalfCpltCallback = NULL;

    /* Set the I2S Rx DMA transfer complete callback */
    hi2s->hdmarx->XferCpltCallback = sai_dma_m0_complete;
    hi2s->hdmarx->XferM1CpltCallback = sai_dma_m1_complete;

    /* Set the I2S Rx DMA error callback */
    hi2s->hdmarx->XferErrorCallback = sai_dma_error;

    hi2s->hdmarx->Instance->CR &= ~DMA_SxCR_CT_Msk;

    /* Set the I2S Tx DMA transfer complete callback */
    hi2s->hdmatx->XferCpltCallback = sai_dma_m0_complete;
    hi2s->hdmatx->XferM1CpltCallback = sai_dma_m1_complete;

    /* Set the I2S Tx DMA error callback */
    hi2s->hdmatx->XferErrorCallback = sai_dma_error;

    hi2s->hdmatx->Instance->CR &= ~DMA_SxCR_CT_Msk;

    tmp1 = hi2s->Instance->I2SCFGR & SPI_I2SCFGR_I2SCFG;
    /* Check if the I2S_MODE_MASTER_TX or I2S_MODE_SLAVE_TX Mode is selected */
    if ((tmp1 == I2S_MODE_MASTER_TX) || (tmp1 == I2S_MODE_SLAVE_TX)) {
        /* Enable the Rx DMA Stream */
        HAL_DMAEx_MultiBufferStart_IT(hi2s->hdmarx, (uint32_t)&I2SxEXT(hi2s->Instance)->DR,
                                      (uint32_t)buf0, (uint32_t)buf1, size);

        /* Enable Rx DMA Request */
        SET_BIT(I2SxEXT(hi2s->Instance)->CR2, SPI_CR2_RXDMAEN);

        /* Enable the Tx DMA Stream */
        HAL_DMAEx_MultiBufferStart_IT(hi2s->hdmatx, (uint32_t)buf0, (uint32_t)&hi2s->Instance->DR,
                                      (uint32_t)buf1, size);

        /* Enable Tx DMA Request */
        SET_BIT(hi2s->Instance->CR2, SPI_CR2_TXDMAEN);

        /* Check if the I2S is already enabled */
        if ((hi2s->Instance->I2SCFGR & SPI_I2SCFGR_I2SE) != SPI_I2SCFGR_I2SE) {
            /* Enable I2Sext(receiver) before enabling I2Sx peripheral */
            __HAL_I2SEXT_ENABLE(hi2s);

            /* Enable I2S peripheral after the I2Sext */
            __HAL_I2S_ENABLE(hi2s);
        }
    } else {
        /* Check if Master Receiver mode is selected */
        if ((hi2s->Instance->I2SCFGR & SPI_I2SCFGR_I2SCFG) == I2S_MODE_MASTER_RX) {
            /*
             * Clear the Overrun Flag by a read operation on the SPI_DR register followed by a read
             * access to the SPI_SR register.
             */
            __HAL_I2S_CLEAR_OVRFLAG(hi2s);
        }
        /* Enable the Tx DMA Stream */
        HAL_DMAEx_MultiBufferStart_IT(hi2s->hdmatx, (uint32_t)buf0, (uint32_t)&I2SxEXT(hi2s->Instance)->DR,
                                      (uint32_t)buf1, size);

        /* Enable Tx DMA Request */
        SET_BIT(I2SxEXT(hi2s->Instance)->CR2, SPI_CR2_TXDMAEN);

        /* Enable the Rx DMA Stream */
        HAL_DMAEx_MultiBufferStart_IT(hi2s->hdmarx, (uint32_t)&hi2s->Instance->DR, (uint32_t)buf0,
                                      (uint32_t)buf1, size);

        /* Enable Rx DMA Request */
        SET_BIT(hi2s->Instance->CR2, SPI_CR2_RXDMAEN);

        /* Check if the I2S is already enabled */
        if ((hi2s->Instance->I2SCFGR & SPI_I2SCFGR_I2SE) != SPI_I2SCFGR_I2SE) {
            /* Enable I2Sext(transmitter) before enabling I2Sx peripheral */
            __HAL_I2SEXT_ENABLE(hi2s);
            /* Enable I2S peripheral before the I2Sext */
            __HAL_I2S_ENABLE(hi2s);
        }
    }

error:
    __HAL_UNLOCK(hi2s);
    return errorcode;
}
#endif

void sai_dma_init(struct stm32_sai *sai_data)
{
    DMA_NodeConfTypeDef nodeConfig = {0};

    __HAL_RCC_GPDMA1_CLK_ENABLE();

    /* Basic Node Configuration */
    nodeConfig.NodeType = DMA_GPDMA_LINEAR_NODE;
    /* Take DMA Node configuration from DMA init struct */
    nodeConfig.Init = sai_data->hdma_sai_tx->Init;

    /* Build the Linked List Queue */
    memset(&sai_data->dma_list, 0, sizeof(sai_data->dma_list));

    nodeConfig.SrcAddress = (uint32_t)zero_buffer;
    nodeConfig.DstAddress = (uint32_t)&sai_data->hsai.Instance->DR;
    nodeConfig.DataSize = 192;

    HAL_DMAEx_List_BuildNode(&nodeConfig, &sai_data->nodes[0]);
    HAL_DMAEx_List_BuildNode(&nodeConfig, &sai_data->nodes[1]);
    HAL_DMAEx_List_InsertNode_Tail(&sai_data->dma_list, &sai_data->nodes[0]);
    HAL_DMAEx_List_InsertNode_Tail(&sai_data->dma_list, &sai_data->nodes[1]);
    HAL_DMAEx_List_SetCircularMode(&sai_data->dma_list);
    HAL_DMAEx_List_Init(sai_data->hdma_sai_tx);
    HAL_DMAEx_List_LinkQ(sai_data->hdma_sai_tx, &sai_data->dma_list);

    /* Link DMA handle */
    __HAL_LINKDMA(&sai_data->hsai, hdmatx, *sai_data->hdma_sai_tx);
}

int
i2s_driver_start(struct i2s *i2s)
{
    int rc = 0;
    struct stm32_sai *sai_data = (struct stm32_sai *)i2s->driver_data;

    switch (sai_data->hsai.State) {
    case HAL_SAI_STATE_RESET:
        if (i2s->sample_rate) {
            sai_data->hsai.Init.AudioFrequency = i2s->sample_rate;
        }
        if (HAL_SAI_Init(&sai_data->hsai) != HAL_OK) {
            rc = SYS_EUNKNOWN;
            break;
        }
        sai_dma_init(sai_data);
        HAL_SAI_InitProtocol(&sai_data->hsai, SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_16BIT, 2);
        // if (HAL_DMA_Init(sai_data->hdma_spi) != HAL_OK) {
        //     (void)HAL_I2S_DeInit(&sai_data->hi2s);
        //     rc = SYS_EUNKNOWN;
        //     break;
        // }
#if I2S_FULLDUPLEX_SUPPORT
        if (i2s->direction == I2S_OUT_IN) {
            if (HAL_DMA_Init(sai_data->hdma_i2sext) != HAL_OK) {
                (void)HAL_DMA_DeInit(sai_data->hdma_spi);
                (void)HAL_I2S_DeInit(&sai_data->hi2s);
                rc = SYS_EUNKNOWN;
                break;
            }
        }
#endif
    /* Fallthrough */
    case HAL_SAI_STATE_READY:
        assert(sai_data->dma_buffers[0] == NULL);
        assert(sai_data->dma_buffers[1] == NULL);
        assert(sai_data->dma_buffer_count == 0);
        sai_data->dma_buffers[0] = i2s_driver_buffer_get(i2s);
        sai_data->dma_buffers[1] = i2s_driver_buffer_get(i2s);
        if (sai_data->dma_buffers[0] == NULL) {
            i2s->state = I2S_STATE_OUT_OF_BUFFERS;
            rc = I2S_ERR_NO_BUFFER;
            break;
        }
        if (sai_data->dma_buffers[1] == NULL) {
            sai_data->dma_buffer_count = 1;
        } else {
            sai_data->dma_buffer_count = 2;
        }
        i2s->state = I2S_STATE_RUNNING;
        if (i2s->direction == I2S_IN) {
            sai_data->dma_buffers[0]->sample_count = sai_data->dma_buffers[0]->capacity;
            sai_receive_start_dma(sai_data);
            assert(sai_data->dma_buffers[0]->capacity == sai_data->dma_buffers[1]->capacity);
        } else if (i2s->direction == I2S_OUT) {
            sai_transmit_start_dma(sai_data);
        } else {
#if I2S_FULLDUPLEX_SUPPORT
            i2s_transmit_receive_dma(&sai_data->hi2s,
                                     sai_data->dma_buffers[0]->sample_data,
                                     sai_data->dma_buffers[1]->sample_data,
                                     sai_data->dma_buffers[0]->sample_count);
#endif
        }
        break;
    case HAL_SAI_STATE_BUSY:
    case HAL_SAI_STATE_BUSY_RX:
    case HAL_SAI_STATE_BUSY_TX:
        break;
    default:
        rc = I2S_ERR_INTERNAL;
    }
    return rc;
}

void
i2s_driver_buffer_queued(struct i2s *i2s)
{
    struct stm32_sai *sai_data = (struct stm32_sai *)i2s->driver_data;
    struct i2s_sample_buffer *buffer;
    int sr;
    int ix;

    hal_gpio_write(ARDUINO_PIN_D9, 1);
    hal_gpio_write(ARDUINO_PIN_D9, 0);

    if (0) {
    if (i2s->state != I2S_STATE_RUNNING) {
        return;
    }
    OS_ENTER_CRITICAL(sr);
    switch (sai_data->dma_buffer_count) {
    case 0:
        /* Driver run out of buffer before and operates on zero_buffer */
        buffer = i2s_driver_buffer_get(i2s);
        /* Get inactive buffer */
        ix = active_buffer_ix(sai_data) ^ 1;
        assert(sai_data->dma_buffers[ix] == NULL);
        sai_data->dma_buffers[ix] = buffer;
        set_dma_address(sai_data, ix, buffer->sample_data);
        sai_data->dma_buffer_count = 1;
        break;
    case 1:
        buffer = i2s_driver_buffer_get(i2s);
        if ((sai_data->hsai.hdmatx->Instance->CCR & DMA_CCR_EN) == 0) {
            stm32_pin_pulse_n(PF3, 3);
            ix = 1;
        } else {
            ix = active_buffer_ix(sai_data) ^ 1;
        }
        assert(sai_data->dma_buffers[ix] == NULL);
        assert(buffer != NULL);
        if (buffer) {
            stm32_pin_pulse(PF3);
            sai_data->dma_buffers[ix] = buffer;
            set_dma_address(sai_data, ix, buffer->sample_data);
            sai_data->dma_buffer_count = 2;
        }
        break;
    default:
        break;
    }
    OS_EXIT_CRITICAL(sr);
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

// bool
// i2s_out_is_active(struct i2s *i2s)
// {
//     struct stm32_i2s *i2s_data = (struct stm32_i2s *)i2s->driver_data;
//     return READ_BIT(i2s_data->hi2s.Instance->I2SCFGR, SPI_I2SCFGR_I2SE) != 0;
// }
