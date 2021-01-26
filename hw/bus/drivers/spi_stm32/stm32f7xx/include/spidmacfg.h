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
#include <stm32f7xx_hal_dma.h>

struct stm32_dma_cfg {
    uint8_t dma_ch;
    uint8_t irqn;
    void (*irq_handler)(void);
    DMA_Stream_TypeDef *regs;
    DMA_InitTypeDef init;
};

#define DMA_STREAM(dma, ch, st, name) \
    extern const struct stm32_dma_cfg DMA ## dma ## _stream ## st ## _channel ## ch

DMA_STREAM(1, 0, 0, spi3_rx);
DMA_STREAM(1, 0, 2, spi3_rx);
DMA_STREAM(1, 0, 3, spi2_rx);
DMA_STREAM(1, 0, 4, spi2_tx);
DMA_STREAM(1, 0, 5, spi3_tx);
DMA_STREAM(1, 0, 7, spi3_tx);

DMA_STREAM(2, 1, 5, spi6_tx);
DMA_STREAM(2, 1, 6, spi6_rx);

DMA_STREAM(2, 2, 2, spi1_tx);
DMA_STREAM(2, 2, 3, spi5_rx);
DMA_STREAM(2, 2, 4, spi5_tx);

DMA_STREAM(2, 3, 0, spi1_rx);
DMA_STREAM(2, 3, 2, spi1_rx);
DMA_STREAM(2, 3, 3, spi1_tx);
DMA_STREAM(2, 3, 5, spi1_tx);

DMA_STREAM(2, 4, 0, spi4_rx);
DMA_STREAM(2, 4, 1, spi4_tx);
DMA_STREAM(2, 4, 4, spi4_rx);

DMA_STREAM(2, 5, 3, spi4_rx);
DMA_STREAM(2, 5, 4, spi4_tx);
DMA_STREAM(2, 5, 5, spi5_tx);

DMA_STREAM(2, 7, 5, spi5_rx);
DMA_STREAM(2, 7, 6, spi5_tx);
