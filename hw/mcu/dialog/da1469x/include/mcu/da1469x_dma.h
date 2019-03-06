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

#ifndef __MCU_DA1469X_DMA_H_
#define __MCU_DA1469X_DMA_H_

#include <stdbool.h>
#include <stdint.h>
#include "DA1469xAB.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DMA peripheral can be one of following values */
#define MCI_DMA_PERIPH_SPI              (0)
#define MCU_DMA_PERIPH_SPI2             (1)
#define MCU_DMA_PERIPH_UART             (2)
#define MCU_DMA_PERIPH_UART2            (3)
#define MCU_DMA_PERIPH_I2C              (4)
#define MCU_DMA_PERIPH_I2C2             (5)
#define MCU_DMA_PERIPH_USB              (6)
#define MCU_DMA_PERIPH_UART3            (7)
#define MCU_DMA_PERIPH_PCM              (8)
#define MCU_DMA_PERIPH_SRC              (9)
#define MCU_DMA_PERIPH_GPADC            (12)
#define MCU_DMA_PERIPH_SDADC            (13)
#define MCU_DMA_PERIPH_NONE             (15)

/* DMA bus width can be one of following values */
#define MCU_DMA_BUS_WIDTH_1B            (0)
#define MCU_DMA_BUS_WIDTH_2B            (1)
#define MCU_DMA_BUS_WIDTH_4B            (2)

/* DMA burst mode can be one of following values */
#define MCU_DMA_BURST_MODE_DISABLED     (0)
#define MCU_DMA_BURST_MODE_4B           (1)
#define MCU_DMA_BURST_MODE_8B           (2)

/* Use DMA_DMA0_??? symbols to access those registers */
struct da1469x_dma_regs {
    __IO uint32_t DMA_A_START_REG;
    __IO uint32_t DMA_B_START_REG;
    __IO uint32_t DMA_INT_REG;
    __IO uint32_t DMA_LEN_REG;
    __IO uint32_t DMA_CTRL_REG;
    __IO uint32_t DMA_IDX_REG;
    __I  uint32_t RESERVED[2];
};

struct da1469x_dma_config {
    bool src_inc;           /* Increase source address after access */
    bool dst_inc;           /* Increase destination address after access */
    uint8_t priority;       /* Channel priority (0-7) */
    uint8_t bus_width;      /* Bus transfer width (see MCU_DMA_BUS_WIDTH_*) */
    uint8_t burst_mode;     /* Burst mode (see (see MCU_DMA_BURST_MODE_*) */
};

typedef int (*da1469x_dma_interrupt_cb)(void *arg);

/**
 * Initialize DMA
 */
void da1469x_dma_init(void);

/**
 * Acquire DMA channel
 *
 * If specified channel index is less than zero, any non-acquired channel will
 * be returned.
 *
 * @param cidx       Channel index
 *
 * @return valid pointer on success, NULL on failure
 */
struct da1469x_dma_regs *da1469x_dma_acquire_single(int cidx);

/**
 * Acquire DMA channels pair for use with peripheral trigger
 *
 * Channel index can be either of channels in pair.
 *
 * If specified channel index is less than zero, any non-acquired channels pair
 * will be returned.
 *
 * @param cidx    Channel index
 * @param periph  Peripheral identified
 * @param chans   Acquired channels
 *
 * @return 0 on success, negative value on failure
 */
int da1469x_dma_acquire_periph(int cidx, uint8_t periph,
                               struct da1469x_dma_regs *chans[2]);

/**
 * Release DMA channel of pair of associated channels
 *
 * \p chan be either of channels in pair to release pair.
 *
 * @chan  Channel to release
 *
 * @return 0 on success, negative value on failure
 */
int da1469x_dma_release_channel(struct da1469x_dma_regs *chan);

/**
 * Configure single DMA channel
 *
 * @chan     Channel acquired by da1469x_dma_acquire_channel()
 * @cfg      Channel configuration to set
 * @isr_cb   Function to be called on interrupt
 * @isr_arg  User data argument to pass as a parameter to callback
 *
 * @return 0 on success, negative value on failure
 */
int da1469x_dma_configure(struct da1469x_dma_regs *chan,
                           const struct da1469x_dma_config *cfg,
                           da1469x_dma_interrupt_cb isr_cb, void *isr_arg);

#ifdef __cplusplus
}
#endif

#endif /* __MCU_DA1469X_DMA_H_ */
