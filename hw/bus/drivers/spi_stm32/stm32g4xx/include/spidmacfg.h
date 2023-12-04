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
#include <stm32g4xx_hal_dma.h>

struct stm32_dma_cfg {
    uint8_t dma_ch;
    uint8_t irqn;
    void (*irq_handler)(void);
    DMA_Channel_TypeDef *regs;
    DMA_InitTypeDef init;
};

#define SPI_DMA_RX_CHANNEL(dma, ch, spi_num) \
    extern const struct stm32_dma_cfg DMA ## dma ## _channel ## ch ## _spi ## spi_num ## _rx;

#define SPI_DMA_TX_CHANNEL(dma, ch, spi_num) \
    extern const struct stm32_dma_cfg DMA ## dma ## _channel ## ch ## _spi ## spi_num ## _tx;

SPI_DMA_RX_CHANNEL(1, 1, 1);
SPI_DMA_RX_CHANNEL(1, 2, 1);
SPI_DMA_RX_CHANNEL(1, 3, 1);
SPI_DMA_RX_CHANNEL(1, 4, 1);
SPI_DMA_RX_CHANNEL(1, 5, 1);
SPI_DMA_RX_CHANNEL(1, 6, 1);
SPI_DMA_RX_CHANNEL(1, 7, 1);
SPI_DMA_RX_CHANNEL(2, 1, 1);
SPI_DMA_RX_CHANNEL(2, 2, 1);
SPI_DMA_RX_CHANNEL(2, 3, 1);
SPI_DMA_RX_CHANNEL(2, 4, 1);
SPI_DMA_RX_CHANNEL(2, 5, 1);
SPI_DMA_RX_CHANNEL(2, 6, 1);
SPI_DMA_RX_CHANNEL(2, 7, 1);

SPI_DMA_RX_CHANNEL(1, 1, 2);
SPI_DMA_RX_CHANNEL(1, 2, 2);
SPI_DMA_RX_CHANNEL(1, 3, 2);
SPI_DMA_RX_CHANNEL(1, 4, 2);
SPI_DMA_RX_CHANNEL(1, 5, 2);
SPI_DMA_RX_CHANNEL(1, 6, 2);
SPI_DMA_RX_CHANNEL(1, 7, 2);
SPI_DMA_RX_CHANNEL(2, 1, 2);
SPI_DMA_RX_CHANNEL(2, 2, 2);
SPI_DMA_RX_CHANNEL(2, 3, 2);
SPI_DMA_RX_CHANNEL(2, 4, 2);
SPI_DMA_RX_CHANNEL(2, 5, 2);
SPI_DMA_RX_CHANNEL(2, 6, 2);
SPI_DMA_RX_CHANNEL(2, 7, 2);

SPI_DMA_RX_CHANNEL(1, 1, 3);
SPI_DMA_RX_CHANNEL(1, 2, 3);
SPI_DMA_RX_CHANNEL(1, 3, 3);
SPI_DMA_RX_CHANNEL(1, 4, 3);
SPI_DMA_RX_CHANNEL(1, 5, 3);
SPI_DMA_RX_CHANNEL(1, 6, 3);
SPI_DMA_RX_CHANNEL(1, 7, 3);
SPI_DMA_RX_CHANNEL(2, 1, 3);
SPI_DMA_RX_CHANNEL(2, 2, 3);
SPI_DMA_RX_CHANNEL(2, 3, 3);
SPI_DMA_RX_CHANNEL(2, 4, 3);
SPI_DMA_RX_CHANNEL(2, 5, 3);
SPI_DMA_RX_CHANNEL(2, 6, 3);
SPI_DMA_RX_CHANNEL(2, 7, 3);

#if defined(SPI4)
SPI_DMA_RX_CHANNEL(1, 1, 4);
SPI_DMA_RX_CHANNEL(1, 2, 4);
SPI_DMA_RX_CHANNEL(1, 3, 4);
SPI_DMA_RX_CHANNEL(1, 4, 4);
SPI_DMA_RX_CHANNEL(1, 5, 4);
SPI_DMA_RX_CHANNEL(1, 6, 4);
SPI_DMA_RX_CHANNEL(1, 7, 4);
SPI_DMA_RX_CHANNEL(2, 1, 4);
SPI_DMA_RX_CHANNEL(2, 2, 4);
SPI_DMA_RX_CHANNEL(2, 3, 4);
SPI_DMA_RX_CHANNEL(2, 4, 4);
SPI_DMA_RX_CHANNEL(2, 5, 4);
SPI_DMA_RX_CHANNEL(2, 6, 4);
SPI_DMA_RX_CHANNEL(2, 7, 4);
#endif

SPI_DMA_TX_CHANNEL(1, 1, 1);
SPI_DMA_TX_CHANNEL(1, 2, 1);
SPI_DMA_TX_CHANNEL(1, 3, 1);
SPI_DMA_TX_CHANNEL(1, 4, 1);
SPI_DMA_TX_CHANNEL(1, 5, 1);
SPI_DMA_TX_CHANNEL(1, 6, 1);
SPI_DMA_TX_CHANNEL(1, 7, 1);
SPI_DMA_TX_CHANNEL(2, 1, 1);
SPI_DMA_TX_CHANNEL(2, 2, 1);
SPI_DMA_TX_CHANNEL(2, 3, 1);
SPI_DMA_TX_CHANNEL(2, 4, 1);
SPI_DMA_TX_CHANNEL(2, 5, 1);
SPI_DMA_TX_CHANNEL(2, 6, 1);
SPI_DMA_TX_CHANNEL(2, 7, 1);

SPI_DMA_TX_CHANNEL(1, 1, 2);
SPI_DMA_TX_CHANNEL(1, 2, 2);
SPI_DMA_TX_CHANNEL(1, 3, 2);
SPI_DMA_TX_CHANNEL(1, 4, 2);
SPI_DMA_TX_CHANNEL(1, 5, 2);
SPI_DMA_TX_CHANNEL(1, 6, 2);
SPI_DMA_TX_CHANNEL(1, 7, 2);
SPI_DMA_TX_CHANNEL(2, 1, 2);
SPI_DMA_TX_CHANNEL(2, 2, 2);
SPI_DMA_TX_CHANNEL(2, 3, 2);
SPI_DMA_TX_CHANNEL(2, 4, 2);
SPI_DMA_TX_CHANNEL(2, 5, 2);
SPI_DMA_TX_CHANNEL(2, 6, 2);
SPI_DMA_TX_CHANNEL(2, 7, 2);

SPI_DMA_TX_CHANNEL(1, 1, 3);
SPI_DMA_TX_CHANNEL(1, 2, 3);
SPI_DMA_TX_CHANNEL(1, 3, 3);
SPI_DMA_TX_CHANNEL(1, 4, 3);
SPI_DMA_TX_CHANNEL(1, 5, 3);
SPI_DMA_TX_CHANNEL(1, 6, 3);
SPI_DMA_TX_CHANNEL(1, 7, 3);
SPI_DMA_TX_CHANNEL(2, 1, 3);
SPI_DMA_TX_CHANNEL(2, 2, 3);
SPI_DMA_TX_CHANNEL(2, 3, 3);
SPI_DMA_TX_CHANNEL(2, 4, 3);
SPI_DMA_TX_CHANNEL(2, 5, 3);
SPI_DMA_TX_CHANNEL(2, 6, 3);
SPI_DMA_TX_CHANNEL(2, 7, 3);

#if defined(SPI4)
SPI_DMA_TX_CHANNEL(1, 1, 4);
SPI_DMA_TX_CHANNEL(1, 2, 4);
SPI_DMA_TX_CHANNEL(1, 3, 4);
SPI_DMA_TX_CHANNEL(1, 4, 4);
SPI_DMA_TX_CHANNEL(1, 5, 4);
SPI_DMA_TX_CHANNEL(1, 6, 4);
SPI_DMA_TX_CHANNEL(1, 7, 4);
SPI_DMA_TX_CHANNEL(2, 1, 4);
SPI_DMA_TX_CHANNEL(2, 2, 4);
SPI_DMA_TX_CHANNEL(2, 3, 4);
SPI_DMA_TX_CHANNEL(2, 4, 4);
SPI_DMA_TX_CHANNEL(2, 5, 4);
SPI_DMA_TX_CHANNEL(2, 6, 4);
SPI_DMA_TX_CHANNEL(2, 7, 4);
#endif
