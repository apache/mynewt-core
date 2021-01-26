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

#include <spidmacfg.h>
#include <stm32l0xx_hal_dma.h>
#include <stm32_common/stm32_dma.h>

#define DMA_IRQn(ch) ((uint8_t)DMA1_Channel1_IRQn + (((DMA1_CH3) > 3) ? 2 : ((DMA1_CH3) >> 1)))
#define DMA_IRQ_HANDLER(ch) (((ch) > DMA1_CH3) ? stm32_dma1_4_5_6_7_irq_handler : stm32_dma1_2_3_irq_handler)

extern DMA_HandleTypeDef *stm32_dma_ch[];

#define SPI_DMA_RX_CHANNEL_DEFINE(dma, ch, req, name)               \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch ## _request ## req = {  \
        DMA ## dma ## _CH ## ch,                                    \
        DMA_IRQn(ch),                                               \
        DMA_IRQ_HANDLER(ch),                                        \
        .regs = DMA ## dma ## _Channel ## ch,                       \
        .init = {                                                   \
            .Request = DMA_REQUEST_ ## req,                         \
            .Direction = DMA_PERIPH_TO_MEMORY,                      \
            .PeriphInc = DMA_PINC_DISABLE,                          \
            .MemInc = DMA_MINC_ENABLE,                              \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,             \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                \
            .Mode = DMA_NORMAL,                                     \
            .Priority = DMA_PRIORITY_LOW,                           \
        }                                                           \
    }

#define SPI_DMA_TX_CHANNEL_DEFINE(dma, ch, req, name)               \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch ## _request ## req = {  \
        DMA ## dma ## _CH ## ch,                                    \
        DMA_IRQn(ch),                                               \
        DMA_IRQ_HANDLER(ch),                                        \
        .regs = DMA ## dma ## _Channel ## ch,                       \
        .init = {                                                   \
            .Request = DMA_REQUEST_ ## req,                         \
            .Direction = DMA_MEMORY_TO_PERIPH,                      \
            .PeriphInc = DMA_PINC_DISABLE,                          \
            .MemInc = DMA_MINC_ENABLE,                              \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,             \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                \
            .Mode = DMA_NORMAL,                                     \
            .Priority = DMA_PRIORITY_LOW,                           \
        }                                                           \
    }

SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 1, spi1_rx);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 1, spi1_tx);

SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 1, spi1_rx);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 1, spi1_tx);

SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 2, spi2_rx);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 2, spi2_tx);

SPI_DMA_RX_CHANNEL_DEFINE(1, 6, 2, spi2_rx);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, 2, spi2_tx);
