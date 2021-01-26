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

#include <assert.h>
#include <os/os_trace_api.h>
#include <stm32_common/stm32_dma.h>
#include <defs/error.h>

#ifdef DMA2
DMA_HandleTypeDef *stm32_dma_ch[16];
#else
DMA_HandleTypeDef *stm32_dma_ch[8];
#endif

int
stm32_dma_acquire_channel(stm32_dma_ch_t ch, DMA_HandleTypeDef *hdma)
{
    int rc = SYS_EBUSY;
    int sr;

    OS_ENTER_CRITICAL(sr);

    if (ch <= DMA_CH_NUM && stm32_dma_ch[ch] == NULL) {
        stm32_dma_ch[ch] = hdma;
        rc = SYS_EOK;
    }

    OS_EXIT_CRITICAL(sr);

    return rc;
}

int
stm32_dma_release_channel(stm32_dma_ch_t ch)
{
    int rc = SYS_EINVAL;
    int sr;

    OS_ENTER_CRITICAL(sr);

    if (stm32_dma_ch[ch]) {
        stm32_dma_ch[ch] = NULL;
        rc = SYS_EOK;
    }
    OS_EXIT_CRITICAL(sr);

    return rc;
}

void
stm32_dma1_0_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA1_CH0]);

    os_trace_isr_exit();
}

void
stm32_dma1_1_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA1_CH1]);

    os_trace_isr_exit();
}

void
stm32_dma1_2_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA1_CH2]);

    os_trace_isr_exit();
}

void
stm32_dma1_3_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA1_CH3]);

    os_trace_isr_exit();
}

void
stm32_dma1_4_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA1_CH4]);

    os_trace_isr_exit();
}

void
stm32_dma1_5_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA1_CH5]);

    os_trace_isr_exit();
}

void
stm32_dma1_6_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA1_CH6]);

    os_trace_isr_exit();
}

void
stm32_dma1_7_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA1_CH7]);

    os_trace_isr_exit();
}

#ifdef DMA2

void
stm32_dma2_0_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA2_CH0]);

    os_trace_isr_exit();
}

void
stm32_dma2_1_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA2_CH1]);

    os_trace_isr_exit();
}

void
stm32_dma2_2_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA2_CH2]);

    os_trace_isr_exit();
}

void
stm32_dma2_3_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA2_CH3]);

    os_trace_isr_exit();
}

void
stm32_dma2_4_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA2_CH4]);

    os_trace_isr_exit();
}

void
stm32_dma2_5_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA2_CH5]);

    os_trace_isr_exit();
}

void
stm32_dma2_6_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA2_CH6]);

    os_trace_isr_exit();
}

void
stm32_dma2_7_irq_handler(void)
{
    os_trace_isr_enter();

    HAL_DMA_IRQHandler(stm32_dma_ch[DMA2_CH7]);

    os_trace_isr_exit();
}

#endif

/*
 * Next two handlers are used by F0 and L0 devices that have shared interrupt for
 * several channels.
 */
void
stm32_dma1_2_3_irq_handler(void)
{
    os_trace_isr_enter();

    if (stm32_dma_ch[DMA1_CH2]) {
        HAL_DMA_IRQHandler(stm32_dma_ch[DMA1_CH2]);
    }
    if (stm32_dma_ch[DMA1_CH3]) {
        HAL_DMA_IRQHandler(stm32_dma_ch[DMA1_CH3]);
    }
#ifdef DMA2
    if (stm32_dma_ch[DMA2_CH1]) {
        HAL_DMA_IRQHandler(stm32_dma_ch[DMA2_CH1]);
    }
    if (stm32_dma_ch[DMA2_CH2]) {
        HAL_DMA_IRQHandler(stm32_dma_ch[DMA2_CH2]);
    }
#endif

    os_trace_isr_exit();
}

void
stm32_dma1_4_5_6_7_irq_handler(void)
{
    stm32_dma_ch_t i;

    os_trace_isr_enter();

    for (i = DMA1_CH4; i <= DMA1_CH7; ++i) {
        if (stm32_dma_ch[i]) {
            HAL_DMA_IRQHandler(stm32_dma_ch[i]);
        }
    }
#ifdef DMA2
    for (i = DMA2_CH3; i <= DMA2_CH5; ++i) {
        if (stm32_dma_ch[i]) {
            HAL_DMA_IRQHandler(stm32_dma_ch[i]);
        }
    }
#endif

    os_trace_isr_exit();
}

