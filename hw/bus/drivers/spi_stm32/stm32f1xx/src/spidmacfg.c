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

#include <stm32_common/stm32_dma.h>
#include <spidmacfg.h>
#include <stm32f1xx_hal_dma.h>

extern DMA_HandleTypeDef *stm32_dma_ch[];

#define SPI_DMA_RX_CHANNEL_DEFINE(dma, ch, name)                \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch = { \
        DMA ## dma ## _CH ## ch,                                \
        DMA ## dma ## _Channel ## ch ## _IRQn,                  \
        stm32_dma ## dma ## _ ## ch ## _irq_handler,            \
        .regs = DMA ## dma ## _Channel ## ch,                   \
        .init = {                                               \
            .Direction = DMA_PERIPH_TO_MEMORY,                  \
            .PeriphInc = DMA_PINC_DISABLE,                      \
            .MemInc = DMA_MINC_ENABLE,                          \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,         \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,            \
            .Mode = DMA_NORMAL,                                 \
            .Priority = DMA_PRIORITY_LOW,                       \
        }                                                       \
    }

#define SPI_DMA_TX_CHANNEL_DEFINE(dma, ch, name)                \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch = { \
        DMA ## dma ## _CH ## ch,                                \
        DMA ## dma ## _Channel ## ch ## _IRQn,                  \
        stm32_dma ## dma ## _ ## ch ## _irq_handler,            \
        .regs = DMA ## dma ## _Channel ## ch,                   \
        .init = {                                               \
            .Direction = DMA_MEMORY_TO_PERIPH,                  \
            .PeriphInc = DMA_PINC_DISABLE,                      \
            .MemInc = DMA_MINC_ENABLE,                          \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,         \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,            \
            .Mode = DMA_NORMAL,                                 \
            .Priority = DMA_PRIORITY_LOW,                       \
        }                                                       \
    }

#ifdef SPI1
SPI_DMA_RX_CHANNEL_DEFINE(1, 2, spi1_rx);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, spi1_tx);
#endif

#ifdef SPI2
SPI_DMA_RX_CHANNEL_DEFINE(1, 4, spi2_rx);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, spi2_tx);
#endif

#ifdef SPI3
SPI_DMA_RX_CHANNEL_DEFINE(2, 1, spi3_rx);
SPI_DMA_TX_CHANNEL_DEFINE(2, 2, spi3_tx);
#endif
