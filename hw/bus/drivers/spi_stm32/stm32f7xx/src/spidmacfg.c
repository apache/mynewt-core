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
#include <stm32f7xx_hal_dma.h>
#include <stm32_common/stm32_dma.h>

#define SPI_DMA_RX_STREAM_DEFINE(dma, ch, st, name)                         \
    const struct stm32_dma_cfg DMA ## dma ## _stream ## st ## _channel ## ch = {  \
        DMA ## dma ## _CH ## st,                                            \
        DMA ## dma ## _Stream ## st ## _IRQn,                               \
        stm32_dma ## dma ## _ ## st ## _irq_handler,                        \
        DMA ## dma ## _Stream ## st,                                        \
        .init = {                                                           \
            .Channel = DMA_CHANNEL_ ## ch,                                  \
            .Direction = DMA_PERIPH_TO_MEMORY,                              \
            .PeriphInc = DMA_PINC_DISABLE,                                  \
            .MemInc = DMA_MINC_ENABLE,                                      \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,                     \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                        \
            .Mode = DMA_NORMAL,                                             \
            .Priority = DMA_PRIORITY_LOW,                                   \
            .FIFOMode = DMA_FIFOMODE_DISABLE,                               \
            .FIFOThreshold = DMA_FIFO_THRESHOLD_1QUARTERFULL,               \
            .MemBurst = DMA_MBURST_SINGLE,                                  \
            .PeriphBurst = DMA_PBURST_SINGLE,                               \
        }                                                                   \
    }

#define SPI_DMA_TX_STREAM_DEFINE(dma, ch, st, name)                         \
    const struct stm32_dma_cfg DMA ## dma ## _stream ## st ## _channel ## ch = {  \
        DMA ## dma ## _CH ## st,                                            \
        DMA ## dma ## _Stream ## st ## _IRQn,                               \
        stm32_dma ## dma ## _ ## st ## _irq_handler,                        \
        DMA ## dma ## _Stream ## st,                                        \
        .init = {                                                           \
            .Channel = DMA_CHANNEL_ ## ch,                                  \
            .Direction = DMA_MEMORY_TO_PERIPH,                              \
            .PeriphInc = DMA_PINC_DISABLE,                                  \
            .MemInc = DMA_MINC_ENABLE,                                      \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,                     \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                        \
            .Mode = DMA_NORMAL,                                             \
            .Priority = DMA_PRIORITY_LOW,                                   \
            .FIFOMode = DMA_FIFOMODE_DISABLE,                               \
            .FIFOThreshold = DMA_FIFO_THRESHOLD_1QUARTERFULL,               \
            .MemBurst = DMA_MBURST_SINGLE,                                  \
            .PeriphBurst = DMA_PBURST_SINGLE,                               \
        }                                                                   \
    }

SPI_DMA_RX_STREAM_DEFINE(1, 0, 0, spi3_rx);
SPI_DMA_RX_STREAM_DEFINE(1, 0, 2, spi3_rx);
SPI_DMA_RX_STREAM_DEFINE(1, 0, 3, spi2_rx);
SPI_DMA_TX_STREAM_DEFINE(1, 0, 4, spi2_tx);
SPI_DMA_TX_STREAM_DEFINE(1, 0, 5, spi3_tx);
SPI_DMA_TX_STREAM_DEFINE(1, 0, 7, spi3_tx);

SPI_DMA_TX_STREAM_DEFINE(2, 1, 5, spi6_tx);
SPI_DMA_RX_STREAM_DEFINE(2, 1, 6, spi6_rx);

SPI_DMA_TX_STREAM_DEFINE(2, 2, 2, spi1_tx);
SPI_DMA_RX_STREAM_DEFINE(2, 2, 3, spi5_rx);
SPI_DMA_TX_STREAM_DEFINE(2, 2, 4, spi5_tx);

SPI_DMA_RX_STREAM_DEFINE(2, 3, 0, spi1_rx);
SPI_DMA_RX_STREAM_DEFINE(2, 3, 2, spi1_rx);
SPI_DMA_TX_STREAM_DEFINE(2, 3, 3, spi1_tx);
SPI_DMA_TX_STREAM_DEFINE(2, 3, 5, spi1_tx);

SPI_DMA_RX_STREAM_DEFINE(2, 4, 0, spi4_rx);
SPI_DMA_TX_STREAM_DEFINE(2, 4, 1, spi4_tx);
SPI_DMA_RX_STREAM_DEFINE(2, 4, 4, spi4_rx);

SPI_DMA_RX_STREAM_DEFINE(2, 5, 3, spi4_rx);
SPI_DMA_TX_STREAM_DEFINE(2, 5, 4, spi4_tx);
SPI_DMA_TX_STREAM_DEFINE(2, 5, 5, spi5_tx);

SPI_DMA_RX_STREAM_DEFINE(2, 7, 5, spi5_rx);
SPI_DMA_TX_STREAM_DEFINE(2, 7, 6, spi5_tx);
