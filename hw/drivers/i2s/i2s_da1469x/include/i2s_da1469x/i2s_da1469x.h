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

#ifndef _I2S_DA1469X_H
#define _I2S_DA1469X_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct da1469x_dma_buffer {
    uint16_t size;
    uint8_t *buffer;
};

#define I2S_DA1469X_DMA_BUFFER_DEF(_name, _size) \
    static uint8_t _Alignas(uint32_t) _name ## _dma_mem[2][_size]; \
    static struct da1469x_dma_buffer _name ## _buffers = {         \
        .size = _size,                                             \
        .buffer = &_name ## _dma_mem[0][0],                        \
    }

#define I2S_DA1469X_DMA_BUFFER(name) (struct da1469x_dma_buffer *)(&name ## _buffers)

typedef enum {
    I2S_DATA_FRAME_16_16,
    I2S_DATA_FRAME_16_32,
    I2S_DATA_FRAME_32_32,
} i2s_data_format_t;

struct i2s_cfg {
    /* Data pin from I2S microphone. */
    int8_t sdin_pin;
    /* Data pin to I2S speaker(s). */
    int8_t sdout_pin;
    /* Left right clock pin. */
    int8_t lrcl_pin;
    /* Bit clock pin. */
    int8_t bclk_pin;

    /* Samples per second. */
    uint32_t sample_rate;
    /* Bits per sample. */
    uint8_t sample_bits;
    /* I2S data format. */
    i2s_data_format_t data_format;

    /* Standard I2S buffer pool, should be set with I2S_BUFFER_POOL() macro. */
    struct i2s_buffer_pool *pool;

    /* DMA buffers, should be set with I2S_DA1469X_DMA_BUFFER(). */
    struct da1469x_dma_buffer *dma_memory;
};

#ifdef __cplusplus
}
#endif

#endif /* _I2S_DA1469X_H */
