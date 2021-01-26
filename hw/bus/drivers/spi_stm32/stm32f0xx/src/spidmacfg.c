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
#include <stm32f0xx_hal_dma.h>

#define DMA_IRQn(ch) ((ch) == DMA1_CH1 ? DMA1_Ch1_IRQn : \
                      ((ch) >= DMA1_CH2 && (ch) <= DMA1_CH3) || \
                      ((ch) >= DMA2_CH1 && (ch) <= DMA2_CH2) ? DMA1_Channel2_3_IRQn : \
                      DMA1_Channel4_5_IRQn)
#define DMA_IRQ_HANDLER(ch) ((ch) == DMA1_CH1 ? stm32_dma1_1_irq_handler : \
                             ((ch) >= DMA1_CH2 && (ch) <= DMA1_CH3) || \
                             ((ch) >= DMA2_CH1 && (ch) <= DMA2_CH2) ? stm32_dma1_2_3_irq_handler : \
                             stm32_dma1_4_5_6_7_irq_handler)

#define SPI_DMA_RX_CHANNEL_DEFINE(dma, ch, name)                    \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch = {     \
        DMA ## dma ## _CH ## ch,                                    \
        DMA_IRQn(ch),                                               \
        DMA_IRQ_HANDLER(ch),                                        \
        .regs = DMA ## dma ## _Channel ## ch,                       \
        .init = {                                                   \
            .Direction = DMA_PERIPH_TO_MEMORY,                      \
            .PeriphInc = DMA_PINC_DISABLE,                          \
            .MemInc = DMA_MINC_ENABLE,                              \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,             \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                \
            .Mode = DMA_NORMAL,                                     \
            .Priority = DMA_PRIORITY_LOW,                           \
        }                                                           \
    }

#define SPI_DMA_TX_CHANNEL_DEFINE(dma, ch, name)                    \
    const struct stm32_dma_cfg DMA ## dma ## _channel ## ch = {     \
        DMA ## dma ## _CH ## ch,                                    \
        DMA_IRQn(ch),                                               \
        DMA_IRQ_HANDLER(ch),                                        \
        .regs = DMA ## dma ## _Channel ## ch,                       \
        .init = {                                                   \
            .Direction = DMA_MEMORY_TO_PERIPH,                      \
            .PeriphInc = DMA_PINC_DISABLE,                          \
            .MemInc = DMA_MINC_ENABLE,                              \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,             \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                \
            .Mode = DMA_NORMAL,                                     \
            .Priority = DMA_PRIORITY_LOW,                           \
        }                                                           \
    }

SPI_DMA_RX_CHANNEL_DEFINE(1, 2, spi1_rx);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, spi1_tx);

SPI_DMA_RX_CHANNEL_DEFINE(1, 4, spi2_rx);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, spi2_tx);

#if defined(DMA1_Channel6) && defined(DMA1_Channel7)
SPI_DMA_RX_CHANNEL_DEFINE(1, 6, spi2_rx);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, spi2_tx);
#endif

#if defined(DMA2_Channel3)
SPI_DMA_RX_CHANNEL_DEFINE(2, 3, spi1_rx);
SPI_DMA_TX_CHANNEL_DEFINE(2, 4, spi1_tx);
#endif
