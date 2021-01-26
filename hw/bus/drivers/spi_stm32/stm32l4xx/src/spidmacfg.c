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
#include <stm32l4xx_hal_dma.h>
#include <stm32_common/stm32_dma.h>

#define SPI_DMA_RX_CHANNEL_DEFINE(dma, ch, req, name)                       \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch ## _request ## req = {  \
        DMA ## dma ## _CH ## ch,                                            \
        DMA ## dma ## _Channel ## ch ## _IRQn,                              \
        stm32_dma ## dma ## _ ## ch ## _irq_handler,                        \
        .regs = DMA ## dma ## _Channel ## ch,                               \
        .init = {                                                           \
            .Request = DMA_REQUEST_ ## req,                                 \
            .Direction = DMA_PERIPH_TO_MEMORY,                              \
            .PeriphInc = DMA_PINC_DISABLE,                                  \
            .MemInc = DMA_MINC_ENABLE,                                      \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,                     \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                        \
            .Mode = DMA_NORMAL,                                             \
            .Priority = DMA_PRIORITY_LOW,                                   \
        }                                                                   \
    }

#define SPI_DMA_TX_CHANNEL_DEFINE(dma, ch, req, name)                       \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch ## _request ## req = {  \
        DMA ## dma ## _CH ## ch,                                            \
        DMA ## dma ## _Channel ## ch ## _IRQn,                              \
        stm32_dma ## dma ## _ ## ch ## _irq_handler,                        \
        .regs = DMA ## dma ## _Channel ## ch,                               \
        .init = {                                                           \
            .Request = DMA_REQUEST_ ## req,                                 \
            .Direction = DMA_MEMORY_TO_PERIPH,                              \
            .PeriphInc = DMA_PINC_DISABLE,                                  \
            .MemInc = DMA_MINC_ENABLE,                                      \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,                     \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                        \
            .Mode = DMA_NORMAL,                                             \
            .Priority = DMA_PRIORITY_LOW,                                   \
        }                                                                   \
    }

SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 1, spi1_rx);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 1, spi1_tx);

SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 1, spi2_rx);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 1, spi2_tx);

SPI_DMA_RX_CHANNEL_DEFINE(2, 1, 3, spi3_rx);
SPI_DMA_TX_CHANNEL_DEFINE(2, 2, 3, spi3_tx);

SPI_DMA_RX_CHANNEL_DEFINE(2, 3, 4, spi1_rx);
SPI_DMA_TX_CHANNEL_DEFINE(2, 4, 4, spi1_tx);
