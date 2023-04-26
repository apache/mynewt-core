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
#include <stm32u5xx_hal_dma.h>
#include <stm32_common/stm32_dma.h>

#define SPI_DMA_RX_CHANNEL_DEFINE(dma, ch, spi_num)                         \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch ## _spi ## spi_num ## _rx = { \
        DMA ## dma ## _CH ## ch,                                            \
        GPDMA ## dma ## _Channel ## ch ## _IRQn,                            \
        stm32_dma ## dma ## _ ## ch ## _irq_handler,                        \
        .regs = GPDMA ## dma ## _Channel ## ch,                             \
        .init = {                                                           \
            .Request = GPDMA ## dma ## _REQUEST_SPI ## spi_num ## _RX,      \
            .BlkHWRequest = DMA_BREQ_SINGLE_BURST,                          \
            .Direction = DMA_PERIPH_TO_MEMORY,                              \
            .SrcInc = DMA_SINC_FIXED,                                       \
            .DestInc = DMA_DINC_INCREMENTED,                                \
            .SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE,                         \
            .DestDataWidth = DMA_DEST_DATAWIDTH_BYTE,                       \
            .SrcBurstLength = 1,                                            \
            .DestBurstLength = 1,                                           \
            .TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0,               \
            .TransferEventMode = DMA_TCEM_BLOCK_TRANSFER,                   \
            .Mode = DMA_NORMAL,                                             \
            .Priority = DMA_LOW_PRIORITY_LOW_WEIGHT,                        \
        }                                                                   \
    }

#define SPI_DMA_TX_CHANNEL_DEFINE(dma, ch, spi_num)                         \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch ## _spi ## spi_num ## _tx = { \
        DMA ## dma ## _CH ## ch,                                            \
        GPDMA ## dma ## _Channel ## ch ## _IRQn,                            \
        stm32_dma ## dma ## _ ## ch ## _irq_handler,                        \
        .regs = GPDMA ## dma ## _Channel ## ch,                             \
        .init = {                                                           \
            .Request = GPDMA ## dma ## _REQUEST_SPI ## spi_num ## _TX,      \
            .BlkHWRequest = DMA_BREQ_SINGLE_BURST,                          \
            .Direction = DMA_MEMORY_TO_PERIPH,                              \
            .SrcInc = DMA_SINC_INCREMENTED,                                 \
            .DestInc = DMA_DINC_FIXED,                                      \
            .SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE,                         \
            .DestDataWidth = DMA_DEST_DATAWIDTH_BYTE,                       \
            .SrcBurstLength = 1,                                            \
            .DestBurstLength = 1,                                           \
            .TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0,               \
            .TransferEventMode = DMA_TCEM_BLOCK_TRANSFER,                   \
            .Mode = DMA_NORMAL,                                             \
            .Priority = DMA_LOW_PRIORITY_LOW_WEIGHT,                        \
        }                                                                   \
    }

SPI_DMA_RX_CHANNEL_DEFINE(1, 1, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 3, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 5, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 6, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 7, 1);

SPI_DMA_RX_CHANNEL_DEFINE(1, 1, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 3, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 5, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 6, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 7, 2);

SPI_DMA_TX_CHANNEL_DEFINE(1, 1, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 2, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 4, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 6, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, 1);

SPI_DMA_TX_CHANNEL_DEFINE(1, 1, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 2, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 4, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 6, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, 2);
