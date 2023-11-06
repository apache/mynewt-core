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
#include <stm32h7xx_hal_dma.h>
#include <stm32_common/stm32_dma.h>

#define SPI_DMA_RX_CHANNEL_DEFINE(dma, st, spi_num)                         \
    const struct stm32_dma_cfg DMA ## dma ## _stream ## st ## _spi ## spi_num ## _rx = { \
        st,                                                                  \
        DMA ## dma ## _Stream ## st ## _IRQn,                               \
        stm32_dma ## dma ## _ ## st ## _irq_handler,                        \
        .regs = DMA ## dma ## _Stream ## st,                                \
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
#define SPI_BDMA_RX_CHANNEL_DEFINE(dma, st, spi_num)                        \
    const struct stm32_dma_cfg DMA ## dma ## _stream ## st ## _spi ## spi_num ## _rx = { \
        st,                                                                  \
        DMA ## dma ## _Stream ## st ## _IRQn,                               \
        stm32_dma ## dma ## _ ## st ## _irq_handler,                        \
        .regs = DMA ## dma ## _Stream ## st,                                \
        .init = {                                                           \
            .Request = BDMA_REQUEST_SPI ## spi_num ## _RX,                  \
            .Direction = DMA_PERIPH_TO_MEMORY,                              \
            .PeriphInc = DMA_PINC_DISABLE,                                  \
            .MemInc = DMA_MINC_ENABLE,                                      \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,                     \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                        \
            .Mode = DMA_NORMAL,                                             \
            .Priority = DMA_PRIORITY_LOW,                                   \
        }                                                                   \
    }

#define SPI_DMA_TX_CHANNEL_DEFINE(dma, st, spi_num)                         \
    const struct stm32_dma_cfg DMA ## dma ## _stream ## st ## _spi ## spi_num ## _tx = { \
        st,                                            \
        DMA ## dma ## _Stream ## st ## _IRQn,                                    \
        stm32_dma ## dma ## _ ## st ## _irq_handler,                        \
        .regs = DMA ## dma ## _Stream ## st,                                     \
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

#define SPI_BDMA_TX_CHANNEL_DEFINE(dma, st, spi_num)                        \
    const struct stm32_dma_cfg DMA ## dma ## _stream ## st ## _spi ## spi_num ## _tx = { \
        st,                                            \
        DMA ## dma ## _Stream ## st ## _IRQn,                               \
        stm32_dma ## dma ## _ ## st ## _irq_handler,                        \
        .regs = DMA ## dma ## _Stream ## st,                                \
        .init = {                                                           \
            .Request = BDMA_REQUEST_SPI ## spi_num ## _TX,                  \
            .Direction = DMA_MEMORY_TO_PERIPH,                              \
            .PeriphInc = DMA_PINC_DISABLE,                                  \
            .MemInc = DMA_MINC_ENABLE,                                      \
            .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,                     \
            .MemDataAlignment = DMA_MDATAALIGN_BYTE,                        \
            .Mode = DMA_NORMAL,                                             \
            .Priority = DMA_PRIORITY_LOW,                                   \
        }                                                                   \
    }

SPI_DMA_RX_CHANNEL_DEFINE(1, 1, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 3, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 5, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 6, 1);
SPI_DMA_RX_CHANNEL_DEFINE(1, 7, 1);
SPI_DMA_RX_CHANNEL_DEFINE(2, 1, 1);
SPI_DMA_RX_CHANNEL_DEFINE(2, 2, 1);
SPI_DMA_RX_CHANNEL_DEFINE(2, 3, 1);
SPI_DMA_RX_CHANNEL_DEFINE(2, 4, 1);
SPI_DMA_RX_CHANNEL_DEFINE(2, 5, 1);
SPI_DMA_RX_CHANNEL_DEFINE(2, 6, 1);
SPI_DMA_RX_CHANNEL_DEFINE(2, 7, 1);

SPI_DMA_RX_CHANNEL_DEFINE(1, 1, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 3, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 5, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 6, 2);
SPI_DMA_RX_CHANNEL_DEFINE(1, 7, 2);
SPI_DMA_RX_CHANNEL_DEFINE(2, 1, 2);
SPI_DMA_RX_CHANNEL_DEFINE(2, 2, 2);
SPI_DMA_RX_CHANNEL_DEFINE(2, 3, 2);
SPI_DMA_RX_CHANNEL_DEFINE(2, 4, 2);
SPI_DMA_RX_CHANNEL_DEFINE(2, 5, 2);
SPI_DMA_RX_CHANNEL_DEFINE(2, 6, 2);
SPI_DMA_RX_CHANNEL_DEFINE(2, 7, 2);

SPI_DMA_RX_CHANNEL_DEFINE(1, 1, 3);
SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 3);
SPI_DMA_RX_CHANNEL_DEFINE(1, 3, 3);
SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 3);
SPI_DMA_RX_CHANNEL_DEFINE(1, 5, 3);
SPI_DMA_RX_CHANNEL_DEFINE(1, 6, 3);
SPI_DMA_RX_CHANNEL_DEFINE(1, 7, 3);
SPI_DMA_RX_CHANNEL_DEFINE(2, 1, 3);
SPI_DMA_RX_CHANNEL_DEFINE(2, 2, 3);
SPI_DMA_RX_CHANNEL_DEFINE(2, 3, 3);
SPI_DMA_RX_CHANNEL_DEFINE(2, 4, 3);
SPI_DMA_RX_CHANNEL_DEFINE(2, 5, 3);
SPI_DMA_RX_CHANNEL_DEFINE(2, 6, 3);
SPI_DMA_RX_CHANNEL_DEFINE(2, 7, 3);

SPI_DMA_RX_CHANNEL_DEFINE(1, 1, 4);
SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 4);
SPI_DMA_RX_CHANNEL_DEFINE(1, 3, 4);
SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 4);
SPI_DMA_RX_CHANNEL_DEFINE(1, 5, 4);
SPI_DMA_RX_CHANNEL_DEFINE(1, 6, 4);
SPI_DMA_RX_CHANNEL_DEFINE(1, 7, 4);
SPI_DMA_RX_CHANNEL_DEFINE(2, 1, 4);
SPI_DMA_RX_CHANNEL_DEFINE(2, 2, 4);
SPI_DMA_RX_CHANNEL_DEFINE(2, 3, 4);
SPI_DMA_RX_CHANNEL_DEFINE(2, 4, 4);
SPI_DMA_RX_CHANNEL_DEFINE(2, 5, 4);
SPI_DMA_RX_CHANNEL_DEFINE(2, 6, 4);
SPI_DMA_RX_CHANNEL_DEFINE(2, 7, 4);

SPI_DMA_RX_CHANNEL_DEFINE(1, 1, 5);
SPI_DMA_RX_CHANNEL_DEFINE(1, 2, 5);
SPI_DMA_RX_CHANNEL_DEFINE(1, 3, 5);
SPI_DMA_RX_CHANNEL_DEFINE(1, 4, 5);
SPI_DMA_RX_CHANNEL_DEFINE(1, 5, 5);
SPI_DMA_RX_CHANNEL_DEFINE(1, 6, 5);
SPI_DMA_RX_CHANNEL_DEFINE(1, 7, 5);
SPI_DMA_RX_CHANNEL_DEFINE(2, 1, 5);
SPI_DMA_RX_CHANNEL_DEFINE(2, 2, 5);
SPI_DMA_RX_CHANNEL_DEFINE(2, 3, 5);
SPI_DMA_RX_CHANNEL_DEFINE(2, 4, 5);
SPI_DMA_RX_CHANNEL_DEFINE(2, 5, 5);
SPI_DMA_RX_CHANNEL_DEFINE(2, 6, 5);
SPI_DMA_RX_CHANNEL_DEFINE(2, 7, 5);

SPI_BDMA_RX_CHANNEL_DEFINE(1, 1, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(1, 2, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(1, 3, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(1, 4, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(1, 5, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(1, 6, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(1, 7, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(2, 1, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(2, 2, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(2, 3, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(2, 4, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(2, 5, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(2, 6, 6);
SPI_BDMA_RX_CHANNEL_DEFINE(2, 7, 6);

SPI_DMA_TX_CHANNEL_DEFINE(1, 1, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 2, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 4, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 6, 1);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, 1);
SPI_DMA_TX_CHANNEL_DEFINE(2, 1, 1);
SPI_DMA_TX_CHANNEL_DEFINE(2, 2, 1);
SPI_DMA_TX_CHANNEL_DEFINE(2, 3, 1);
SPI_DMA_TX_CHANNEL_DEFINE(2, 4, 1);
SPI_DMA_TX_CHANNEL_DEFINE(2, 5, 1);
SPI_DMA_TX_CHANNEL_DEFINE(2, 6, 1);
SPI_DMA_TX_CHANNEL_DEFINE(2, 7, 1);

SPI_DMA_TX_CHANNEL_DEFINE(1, 1, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 2, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 4, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 6, 2);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, 2);
SPI_DMA_TX_CHANNEL_DEFINE(2, 1, 2);
SPI_DMA_TX_CHANNEL_DEFINE(2, 2, 2);
SPI_DMA_TX_CHANNEL_DEFINE(2, 3, 2);
SPI_DMA_TX_CHANNEL_DEFINE(2, 4, 2);
SPI_DMA_TX_CHANNEL_DEFINE(2, 5, 2);
SPI_DMA_TX_CHANNEL_DEFINE(2, 6, 2);
SPI_DMA_TX_CHANNEL_DEFINE(2, 7, 2);

SPI_DMA_TX_CHANNEL_DEFINE(1, 1, 3);
SPI_DMA_TX_CHANNEL_DEFINE(1, 2, 3);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 3);
SPI_DMA_TX_CHANNEL_DEFINE(1, 4, 3);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 3);
SPI_DMA_TX_CHANNEL_DEFINE(1, 6, 3);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, 3);
SPI_DMA_TX_CHANNEL_DEFINE(2, 1, 3);
SPI_DMA_TX_CHANNEL_DEFINE(2, 2, 3);
SPI_DMA_TX_CHANNEL_DEFINE(2, 3, 3);
SPI_DMA_TX_CHANNEL_DEFINE(2, 4, 3);
SPI_DMA_TX_CHANNEL_DEFINE(2, 5, 3);
SPI_DMA_TX_CHANNEL_DEFINE(2, 6, 3);
SPI_DMA_TX_CHANNEL_DEFINE(2, 7, 3);

SPI_DMA_TX_CHANNEL_DEFINE(1, 1, 4);
SPI_DMA_TX_CHANNEL_DEFINE(1, 2, 4);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 4);
SPI_DMA_TX_CHANNEL_DEFINE(1, 4, 4);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 4);
SPI_DMA_TX_CHANNEL_DEFINE(1, 6, 4);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, 4);
SPI_DMA_TX_CHANNEL_DEFINE(2, 1, 4);
SPI_DMA_TX_CHANNEL_DEFINE(2, 2, 4);
SPI_DMA_TX_CHANNEL_DEFINE(2, 3, 4);
SPI_DMA_TX_CHANNEL_DEFINE(2, 4, 4);
SPI_DMA_TX_CHANNEL_DEFINE(2, 5, 4);
SPI_DMA_TX_CHANNEL_DEFINE(2, 6, 4);
SPI_DMA_TX_CHANNEL_DEFINE(2, 7, 4);

SPI_DMA_TX_CHANNEL_DEFINE(1, 1, 5);
SPI_DMA_TX_CHANNEL_DEFINE(1, 2, 5);
SPI_DMA_TX_CHANNEL_DEFINE(1, 3, 5);
SPI_DMA_TX_CHANNEL_DEFINE(1, 4, 5);
SPI_DMA_TX_CHANNEL_DEFINE(1, 5, 5);
SPI_DMA_TX_CHANNEL_DEFINE(1, 6, 5);
SPI_DMA_TX_CHANNEL_DEFINE(1, 7, 5);
SPI_DMA_TX_CHANNEL_DEFINE(2, 1, 5);
SPI_DMA_TX_CHANNEL_DEFINE(2, 2, 5);
SPI_DMA_TX_CHANNEL_DEFINE(2, 3, 5);
SPI_DMA_TX_CHANNEL_DEFINE(2, 4, 5);
SPI_DMA_TX_CHANNEL_DEFINE(2, 5, 5);
SPI_DMA_TX_CHANNEL_DEFINE(2, 6, 5);
SPI_DMA_TX_CHANNEL_DEFINE(2, 7, 5);

SPI_BDMA_TX_CHANNEL_DEFINE(1, 1, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(1, 2, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(1, 3, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(1, 4, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(1, 5, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(1, 6, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(1, 7, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(2, 1, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(2, 2, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(2, 3, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(2, 4, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(2, 5, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(2, 6, 6);
SPI_BDMA_TX_CHANNEL_DEFINE(2, 7, 6);
