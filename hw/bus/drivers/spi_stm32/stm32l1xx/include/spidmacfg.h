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

#include <stdint.h>
#include <stm32l1xx_hal_dma.h>

struct stm32_dma_cfg {
    uint8_t dma_ch;
    uint8_t irqn;
    void (*irq_handler)(void);
    DMA_Channel_TypeDef *regs;
    DMA_InitTypeDef init;
};

#define SPI_DMA_CHANNEL(dma, ch, name) \
    extern const struct stm32_dma_cfg DMA ## dma ## _channel ## ch

SPI_DMA_CHANNEL(1, 2, spi1_rx);
SPI_DMA_CHANNEL(1, 3, spi1_tx);

SPI_DMA_CHANNEL(1, 4, spi2_rx);
SPI_DMA_CHANNEL(1, 5, spi2_tx);

SPI_DMA_CHANNEL(2, 1, spi3_rx);
SPI_DMA_CHANNEL(2, 2, spi3_tx);
