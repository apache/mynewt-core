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

#ifndef _I2S_STM32F1_H
#define _I2S_STM32F1_H

#include <mcu/stm32_hal.h>
#include <i2s_stm32f1/stm32_pin_cfg.h>

/* Structure with I2S config, needed for i2s_create() */
struct i2s_cfg {
    /* Value I2S_MODE_xxxxxx */
    uint32_t mode;
    /* Value I2S_STANDARD_xxxxxx */
    uint32_t standard;
    /* Value I2S_DATAFORMAT_xxxxxx */
    uint32_t data_format;
    uint32_t sample_rate;

    struct i2s_buffer_pool *pool;
    /* Use I2S_HW_CFG to fill this filed */
    const struct stm32f1_i2s_hw_cfg *hw_cfg;
};

/*
 * Macro for filling out hw_cfg filed in i2s_cfg
 * example usage:
 *   xxx.hw_cfg = I2S_HW_CFG(2, tx);
 *   xxx.hw_cfg = I2S_HW_CFG(3, rx);
 */
#define I2S_HW_CFG(i2s_num, dir) &(i2s ## i2s_num ## _ ## dir)

struct i2s;
struct stm32_i2s;

struct stm32_i2s_pins {
    stm32_pin_cfg_t ck_pin;
    stm32_pin_cfg_t ws_pin;
    stm32_pin_cfg_t sd_pin;
};

#define I2S_PIN(n, port, pin) &(I2S ## n ## _P ## port ## pin)
#define I2S_CK_PIN(n, port, pin) &(I2S ## n ## _CK_P ## port ## pin)
#define I2S_WS_PIN(n, port, pin) &(I2S ## n ## _WS_P ## port ## pin)
#define I2S_SD_PIN(n, port, pin) &(I2S ## n ## _SD_P ## port ## pin)

struct stm32f1_i2s_hw_cfg {
    uint8_t dma_num;
    IRQn_Type i2s_irq;
    IRQn_Type dma_channel_irq;
    SPI_TypeDef *i2s_base;
    DMA_Channel_TypeDef *dma_channel_base;
    DMA_TypeDef *dma_base;
    struct stm32_i2s_pins pins;
    struct stm32_i2s *driver_data;
    void (*i2s_irq_handler)(void);
    void (*dma_irq_handler)(void);
    void (*i2s_enable_clock)(bool enable);
    void (*dma_enable_clock)(bool enable);
};

struct stm32_i2s {
    I2S_HandleTypeDef hi2s;
    DMA_HandleTypeDef hdma_spi;

    struct i2s *i2s;
    struct i2s_sample_buffer *active_buffer;
};

#define I2S_HW_DECLARE(i2s_num, dir) \
    extern const struct stm32f1_i2s_hw_cfg i2s ## i2s_num ## _ ## dir

I2S_HW_DECLARE(2, rx);
I2S_HW_DECLARE(2, tx);
I2S_HW_DECLARE(3, rx);
I2S_HW_DECLARE(3, tx);

#define I2S_PIN_DECLARE(n, port, pin) \
    extern const struct stm32_pin_cfg I2S ## n ## _P ## port ## pin;
#define I2S_CK_PIN_DECLARE(n, port, pin) \
    extern const struct stm32_pin_cfg I2S ## n ## _CK_P ## port ## pin;
#define I2S_WS_PIN_DECLARE(n, port, pin) \
    extern const struct stm32_pin_cfg I2S ## n ## _WS_P ## port ## pin;
#define I2S_SD_PIN_DECLARE(n, port, pin) \
    extern const struct stm32_pin_cfg I2S ## n ## _SD_P ## port ## pin;

/* I2S2 Possible CK pins */
I2S_CK_PIN_DECLARE(2, B, 13)
/* I2S2 possible WS pins */
I2S_WS_PIN_DECLARE(2, B, 12)
/* I2S2 possible SD pins */
I2S_SD_PIN_DECLARE(2, B, 15)
/* I2S2 Possible MCK pins */
I2S_PIN_DECLARE(2, C, 6)

/* I2S3 possible CK pins */
I2S_CK_PIN_DECLARE(3, B, 3)
/* I2S3 possible WS pins */
I2S_WS_PIN_DECLARE(3, A, 15)
/* I2S3 possible SD pins */
I2S_SD_PIN_DECLARE(3, B, 5)
/* I2S3 possible MCK pins */
I2S_PIN_DECLARE(3, C, 7)

#endif /* _I2S_STM32F1_H */
