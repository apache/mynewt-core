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

#ifndef _I2S_STM32_H
#define _I2S_STM32_H

#include <mcu/stm32_hal.h>
#include <i2s/i2s.h>
#include <mcu/stm32_pin_cfg.h>

#include <mcu/stm32_clock.h>

#ifdef __cplusplus
//extern "C" {
#endif

struct i2s;
struct i2s_cfg;

struct stm32_sai_pins {
    stm32_pin_cfg_t sck_pin;
    stm32_pin_cfg_t fs_pin;
    stm32_pin_cfg_t sdo_pin;
    stm32_pin_cfg_t sdi_pin;
    stm32_pin_cfg_t mck_pin;
};

typedef struct irq_cfg irq_cfg_t;
typedef struct stm32_dma_cfg stm32_dma_cfg_t;

struct irq_cfg {
    IRQn_Type irqn;
    uint8_t pri;
    uint8_t sub_pri;
    void (*isr)(void);
};

void nvic_config_isr(const irq_cfg_t *cfg);

struct stm32_dma_cfg {
    uint8_t dma_num;
    const irq_cfg_t *irq_cfg;
    stm32_clock_t clock;
    DMA_HandleTypeDef *dma_channel;
};

struct stm32_dma {
    DMA_Channel_TypeDef dma_channel;
    uint8_t dma_num;
    const irq_cfg_t *irq_cfg;
    const stm32_clock_t *clock;
    const DMA_InitTypeDef *dma_init;
};

extern const DMA_InitTypeDef sai_a_tx_dma_init;
extern const DMA_InitTypeDef sai_a_rx_dma_init;
extern const DMA_InitTypeDef sai_b_tx_dma_init;
extern const DMA_InitTypeDef sai_b_rx_dma_init;

#define SAI_DMA_DECLARE(sai, dma, ch, dir) \
extern void sai##_##dir##_dma##dma##_Channel##ch##_isr(void);\
extern stm32_dma_cfg_t sai##_##dir##_GPDMA##dma##_Channel##ch##_cfg;

SAI_DMA_DECLARE(SAI1_A, 1, 0, tx);
SAI_DMA_DECLARE(SAI1_A, 1, 1, tx);
SAI_DMA_DECLARE(SAI1_A, 1, 2, tx);
SAI_DMA_DECLARE(SAI1_A, 1, 3, tx);
SAI_DMA_DECLARE(SAI1_A, 1, 4, tx);
SAI_DMA_DECLARE(SAI1_A, 1, 5, tx);

SAI_DMA_DECLARE(SAI1_B, 1, 0, tx);
SAI_DMA_DECLARE(SAI1_B, 1, 1, tx);
SAI_DMA_DECLARE(SAI1_B, 1, 2, tx);
SAI_DMA_DECLARE(SAI1_B, 1, 3, tx);
SAI_DMA_DECLARE(SAI1_B, 1, 4, tx);
SAI_DMA_DECLARE(SAI1_B, 1, 5, tx);

SAI_DMA_DECLARE(SAI2_A, 1, 0, tx);
SAI_DMA_DECLARE(SAI2_A, 1, 1, tx);
SAI_DMA_DECLARE(SAI2_A, 1, 2, tx);
SAI_DMA_DECLARE(SAI2_A, 1, 3, tx);
SAI_DMA_DECLARE(SAI2_A, 1, 4, tx);
SAI_DMA_DECLARE(SAI2_A, 1, 5, tx);

SAI_DMA_DECLARE(SAI2_B, 1, 0, tx);
SAI_DMA_DECLARE(SAI2_B, 1, 1, tx);
SAI_DMA_DECLARE(SAI2_B, 1, 2, tx);
SAI_DMA_DECLARE(SAI2_B, 1, 3, tx);
SAI_DMA_DECLARE(SAI2_B, 1, 4, tx);
SAI_DMA_DECLARE(SAI2_B, 1, 5, tx);

void SAI1_IRQHandler(void);
void SAI2_IRQHandler(void);
void SAI3_IRQHandler(void);

#define SAI_IRQ(sai) (&(sai##_irq_def))
#define SAI_IRQ_DECLARE(sai) extern const irq_cfg_t sai##_irq_def;

SAI_IRQ_DECLARE(SAI1)
SAI_IRQ_DECLARE(SAI2)
SAI_IRQ_DECLARE(SAI3)

typedef struct i2s_cfg sai_cfg_t;
typedef struct stm32_sai stm32_sai_t;

struct i2s_cfg {
    enum i2s_direction direction;
    bool slave;
    stm32_clock_t sai_clock_src;
    /** @ref SAI_Protocol_DataSize */
    uint32_t data_size;
    uint32_t sample_rate;

    struct i2s_buffer_pool *pool;

    struct stm32_sai *sai;
    const struct stm32_dma_cfg *rx_dma_cfg;
    const struct stm32_dma_cfg *tx_dma_cfg;
    struct stm32_sai_pins pins;
};

typedef struct stm32_sai stm32_sai_t;

#define SAI_CFG_DECLARE(n) \
    extern struct stm32_sai_cfg sai ## n ## _cfg;

SAI_CFG_DECLARE(1);
SAI_CFG_DECLARE(2);
SAI_CFG_DECLARE(3);
SAI_CFG_DECLARE(4);
SAI_CFG_DECLARE(5);

#define SAI_DECLARE(sai) \
extern stm32_sai_t _##sai##_A_tx;\
extern stm32_sai_t _##sai##_B_tx;\
extern stm32_sai_t _##sai##_A_rx;\
extern stm32_sai_t _##sai##_B_rx;\
static stm32_sai_t *const sai##_A_tx __unused = &_##sai##_A_tx;\
static stm32_sai_t *const sai##_A_rx __unused = &_##sai##_A_rx;\
static stm32_sai_t *const sai##_B_tx __unused = &_##sai##_B_tx;\
static stm32_sai_t *const sai##_B_rx __unused = &_##sai##_B_rx;\

/* Define SAI device handles to use SAI1_A_tx, SAI1_A_rx, SAI1_B_tx... */

SAI_DECLARE(SAI1);
SAI_DECLARE(SAI2);
#ifdef SAI3
SAI_DECLARE(SAI3);
#endif

#ifdef __cplusplus
//}
#endif

#endif /* _I2S_STM32_H */
