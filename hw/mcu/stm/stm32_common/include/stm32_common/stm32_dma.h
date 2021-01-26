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

#ifndef __STM32_DMA_H_
#define __STM32_DMA_H_

#include <stm32_common/stm32_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Enum for DMA channel.
 * For F0, F1, F3, L0, L1, L4, WB it represents channel
 * For F4, F7 it represents stream (because channel has other meaning).
 */
typedef enum {
    DMA1_CH0,
    DMA1_CH1,
    DMA1_CH2,
    DMA1_CH3,
    DMA1_CH4,
    DMA1_CH5,
    DMA1_CH6,
    DMA1_CH7,
    DMA2_CH0,
    DMA2_CH1,
    DMA2_CH2,
    DMA2_CH3,
    DMA2_CH4,
    DMA2_CH5,
    DMA2_CH6,
    DMA2_CH7,
    DMA_CH_NUM,
} stm32_dma_ch_t;

/**
 * Allocate DMA channel for specific ST DMA handle.
 *
 * Function assigns DMA handle to DMA channel.
 * This handle is later used in interrupt handlers.
 *
 * @param ch    DMA channel id
 * @param hdma  DMA handle to assign to channel
 * @return SYS_EBUSY - when channel is already allocated
 *         SYS_EOK - on success
 */
int stm32_dma_acquire_channel(stm32_dma_ch_t ch, DMA_HandleTypeDef *hdma);

/**
 * Release DMA channel.
 *
 * @param ch    DMA channel that was allocated with stm32_dma_acquire_channel()
 *
 * @return  SYS_EOK on success
 *          SYS_EINVAL if channel was not previously allocated
 */
int stm32_dma_release_channel(stm32_dma_ch_t ch);

/*
 * Functions that can be used as interrupt handlers that redirect
 * code execution to ST HAL drivers.
 */
void stm32_dma1_0_irq_handler(void);
void stm32_dma1_1_irq_handler(void);
void stm32_dma1_2_irq_handler(void);
void stm32_dma1_3_irq_handler(void);
void stm32_dma1_4_irq_handler(void);
void stm32_dma1_5_irq_handler(void);
void stm32_dma1_6_irq_handler(void);
void stm32_dma1_7_irq_handler(void);
void stm32_dma2_0_irq_handler(void);
void stm32_dma2_1_irq_handler(void);
void stm32_dma2_2_irq_handler(void);
void stm32_dma2_3_irq_handler(void);
void stm32_dma2_4_irq_handler(void);
void stm32_dma2_5_irq_handler(void);
void stm32_dma2_6_irq_handler(void);
void stm32_dma2_7_irq_handler(void);

/* F0 and L0 can have common vector for several DMA channels */
void stm32_dma1_2_3_irq_handler(void);
void stm32_dma1_4_5_6_7_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32_DMA_H_ */
