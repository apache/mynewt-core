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
#include <stm32g0xx_hal_dma.h>
#include <stm32_common/stm32_dma.h>

#define DMA1_Channel2_IRQn  10
#define DMA1_Channel3_IRQn  10
#define DMA1_Channel4_IRQn  11
#define DMA1_Channel5_IRQn  11
#define DMA1_Channel6_IRQn  11
#define DMA1_Channel7_IRQn  11

#define SPI_DMA_RX_CHANNEL_DEFINE(dma, ch, spi_num, irq_handler)            \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch ## _spi ## spi_num ## _rx = { \
        DMA ## dma ## _CH ## ch,                                            \
        DMA ## dma ## _Channel ## ch ## _IRQn,                              \
        irq_handler,                                                        \
        .regs = DMA ## dma ## _Channel ## ch,                               \
        .init = {                                                           \
            .Request = DMA_REQUEST_SPI ## spi_num ## _RX,                   \
            .Direction = DMA_PERIPH_TO_MEMORY,                              \
            .PeriphInc = DMA_PINC_DISABLE,                                  \
            .MemInc = DMA_MINC_ENABLE,                                      \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,                     \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                        \
            .Mode = DMA_NORMAL,                                             \
            .Priority = DMA_PRIORITY_LOW,                                   \
        }                                                                   \
    }

#define SPI_DMA_TX_CHANNEL_DEFINE(dma, ch, spi_num, irq_handler)            \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch ## _spi ## spi_num ## _tx = { \
        DMA ## dma ## _CH ## ch,                                            \
        DMA ## dma ## _Channel ## ch ## _IRQn,                              \
        irq_handler,                                                        \
        .regs = DMA ## dma ## _Channel ## ch,                               \
        .init = {                                                           \
            .Request = DMA_REQUEST_SPI ## spi_num ## _TX,                   \
            .Direction = DMA_MEMORY_TO_PERIPH,                              \
            .PeriphInc = DMA_PINC_DISABLE,                                  \
            .MemInc = DMA_MINC_ENABLE,                                      \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,                     \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                        \
            .Mode = DMA_NORMAL,                                             \
            .Priority = DMA_PRIORITY_LOW,                                   \
        }                                                                   \
    }

SPI_DMA_RX_CHANNEL_DEFINE(1, 1, 1, stm32_dma1_1_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 1, stm32_dma1_2_3_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 3, 1, stm32_dma1_2_3_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 1, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 5, 1, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 6, 1, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 7, 1, stm32_dma1_4_5_6_7_irq_handler);

SPI_DMA_RX_CHANNEL_DEFINE(1, 1, 2, stm32_dma1_1_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 2, stm32_dma1_2_3_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 3, 2, stm32_dma1_2_3_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 2, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 5, 2, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 6, 2, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 7, 2, stm32_dma1_4_5_6_7_irq_handler);

SPI_DMA_RX_CHANNEL_DEFINE(1, 1, 3, stm32_dma1_1_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 3, stm32_dma1_2_3_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 3, 3, stm32_dma1_2_3_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 3, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 5, 3, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 6, 3, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_RX_CHANNEL_DEFINE(1, 7, 3, stm32_dma1_4_5_6_7_irq_handler);

SPI_DMA_TX_CHANNEL_DEFINE(1, 1, 1, stm32_dma1_1_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 2, 1, stm32_dma1_2_3_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 1, stm32_dma1_2_3_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 4, 1, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 1, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 6, 1, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, 1, stm32_dma1_4_5_6_7_irq_handler);

SPI_DMA_TX_CHANNEL_DEFINE(1, 1, 2, stm32_dma1_1_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 2, 2, stm32_dma1_2_3_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 2, stm32_dma1_2_3_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 4, 2, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 2, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 6, 2, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, 2, stm32_dma1_4_5_6_7_irq_handler);

SPI_DMA_TX_CHANNEL_DEFINE(1, 1, 3, stm32_dma1_1_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 2, 3, stm32_dma1_2_3_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 3, stm32_dma1_2_3_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 4, 3, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 3, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 6, 3, stm32_dma1_4_5_6_7_irq_handler);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, 3, stm32_dma1_4_5_6_7_irq_handler);
