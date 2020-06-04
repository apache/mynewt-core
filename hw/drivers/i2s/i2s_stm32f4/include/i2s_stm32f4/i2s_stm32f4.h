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

#include <stm32f4xx_hal.h>
#include <i2s_stm32f4/stm32_pin_cfg.h>

struct i2s;
struct i2s_cfg;
struct stm32_spi_cfg;

struct stm32_i2s_pins {
    stm32_pin_cfg_t ck_pin;
    stm32_pin_cfg_t ws_pin;
    stm32_pin_cfg_t sd_pin;
    stm32_pin_cfg_t ext_sd_pin;
};

#define I2S_PIN(n, port, pin) &(I2S ## n ## _P ## port ## pin)
#define I2S_CK_PIN(n, port, pin) &(I2S ## n ## _CK_P ## port ## pin)
#define I2S_WS_PIN(n, port, pin) &(I2S ## n ## _WS_P ## port ## pin)
#define I2S_SD_PIN(n, port, pin) &(I2S ## n ## _SD_P ## port ## pin)

struct stm32_dma_cfg {
    uint8_t dma_num;
    IRQn_Type dma_stream_irq;
    DMA_Stream_TypeDef *dma_stream;
    uint32_t dma_channel;
};

struct i2s_cfg {
    uint32_t mode;
    uint32_t standard;
    uint32_t data_format;
    uint32_t sample_rate;

    struct i2s_buffer_pool *pool;
    const struct stm32_spi_cfg *spi_cfg;
    const struct stm32_dma_cfg *dma_cfg;
    const struct stm32_dma_cfg *dma_i2sext_cfg;
    struct stm32_i2s_pins pins;
};

#define SPI_CFG(n) &(spi ## n ## _cfg)

struct stm32_i2s {
    I2S_HandleTypeDef hi2s;
    DMA_HandleTypeDef *hdma_spi;
    DMA_HandleTypeDef *hdma_i2sext;

    struct i2s *i2s;
    struct i2s_sample_buffer *dma_buffers[2];
    uint8_t dma_buffer_count;
};

#define DMA_CFG(dma, ch, st, name) &(name ## _stream ## st ## _channel ## ch)

#define DMA_STREAM_DECLARE(dma, ch, st, name) \
    extern const struct stm32_dma_cfg name ## _stream ## st ## _channel ## ch

DMA_STREAM_DECLARE(1, 0, 0, spi3_rx);
DMA_STREAM_DECLARE(1, 0, 1, i2c1_tx);
DMA_STREAM_DECLARE(1, 0, 2, spi3_rx);
DMA_STREAM_DECLARE(1, 0, 3, spi2_rx);
DMA_STREAM_DECLARE(1, 0, 4, spi2_tx);
DMA_STREAM_DECLARE(1, 0, 5, spi3_tx);
DMA_STREAM_DECLARE(1, 0, 7, spi3_tx);
DMA_STREAM_DECLARE(1, 1, 0, i2c1_rx);
DMA_STREAM_DECLARE(1, 1, 1, i2c3_rx);
DMA_STREAM_DECLARE(1, 1, 2, tim7_up);
DMA_STREAM_DECLARE(1, 1, 4, tim7_up);
DMA_STREAM_DECLARE(1, 1, 5, i2c1_rx);
DMA_STREAM_DECLARE(1, 1, 6, i2c1_tx);
DMA_STREAM_DECLARE(1, 1, 7, i2c1_tx);
DMA_STREAM_DECLARE(1, 2, 0, tim4_ch1);
DMA_STREAM_DECLARE(1, 2, 2, i2s3_ext_rx);
DMA_STREAM_DECLARE(1, 2, 3, tim4_ch2);
DMA_STREAM_DECLARE(1, 2, 4, i2s2_ext_tx);
DMA_STREAM_DECLARE(1, 2, 5, i2s3_ext_tx);
DMA_STREAM_DECLARE(1, 2, 6, tim4_up);
DMA_STREAM_DECLARE(1, 2, 7, tim4_ch3);
DMA_STREAM_DECLARE(1, 3, 0, i2s3_ext_rx);
DMA_STREAM_DECLARE(1, 3, 1, tim2_up);
DMA_STREAM_DECLARE(1, 3, 1, tim2_ch3);
DMA_STREAM_DECLARE(1, 3, 2, i2c3_rx);
DMA_STREAM_DECLARE(1, 3, 3, i2s2_ext_rx);
DMA_STREAM_DECLARE(1, 3, 4, i2c3_tx);
DMA_STREAM_DECLARE(1, 3, 5, tim2_ch1);
DMA_STREAM_DECLARE(1, 3, 6, tim2_ch2);
DMA_STREAM_DECLARE(1, 3, 6, tim2_ch4);
DMA_STREAM_DECLARE(1, 3, 7, tim2_up);
DMA_STREAM_DECLARE(1, 3, 7, tim2_ch4);
DMA_STREAM_DECLARE(1, 4, 0, uart5_rx);
DMA_STREAM_DECLARE(1, 4, 1, usart3_rx);
DMA_STREAM_DECLARE(1, 4, 2, uart4_rx);
DMA_STREAM_DECLARE(1, 4, 3, usart3_tx);
DMA_STREAM_DECLARE(1, 4, 4, uart4_tx);
DMA_STREAM_DECLARE(1, 4, 5, usart2_rx);
DMA_STREAM_DECLARE(1, 4, 6, usart2_tx);
DMA_STREAM_DECLARE(1, 4, 7, uart5_tx);

DMA_STREAM_DECLARE(2, 0, 0, adc1);
DMA_STREAM_DECLARE(2, 0, 1, sai1_a);
DMA_STREAM_DECLARE(2, 0, 2, tim8_ch1);
DMA_STREAM_DECLARE(2, 0, 2, tim8_ch2);
DMA_STREAM_DECLARE(2, 0, 2, tim8_ch3);
DMA_STREAM_DECLARE(2, 0, 3, sai1_a);
DMA_STREAM_DECLARE(2, 0, 4, adc1);
DMA_STREAM_DECLARE(2, 0, 5, sai1_b);
DMA_STREAM_DECLARE(2, 0, 6, tim1_ch1);
DMA_STREAM_DECLARE(2, 0, 6, tim1_ch2);
DMA_STREAM_DECLARE(2, 0, 6, tim1_ch3);
DMA_STREAM_DECLARE(2, 1, 1, dcmi);
DMA_STREAM_DECLARE(2, 1, 2, adc2);
DMA_STREAM_DECLARE(2, 1, 3, adc2);
DMA_STREAM_DECLARE(2, 1, 4, sai1_b);
DMA_STREAM_DECLARE(2, 1, 5, spi6_tx);
DMA_STREAM_DECLARE(2, 1, 6, spi6_rx);
DMA_STREAM_DECLARE(2, 1, 7, dcmi);
DMA_STREAM_DECLARE(2, 2, 0, adc3);
DMA_STREAM_DECLARE(2, 2, 1, adc3);
DMA_STREAM_DECLARE(2, 2, 2, spi1_tx);
DMA_STREAM_DECLARE(2, 2, 3, spi5_rx);
DMA_STREAM_DECLARE(2, 2, 4, spi5_tx);
DMA_STREAM_DECLARE(2, 2, 5, cryp_out);
DMA_STREAM_DECLARE(2, 2, 6, cryp_in);
DMA_STREAM_DECLARE(2, 2, 7, hash_in);
DMA_STREAM_DECLARE(2, 3, 0, spi1_rx);
DMA_STREAM_DECLARE(2, 3, 2, spi1_rx);
DMA_STREAM_DECLARE(2, 3, 3, spi1_tx);
DMA_STREAM_DECLARE(2, 3, 5, spi1_tx);
DMA_STREAM_DECLARE(2, 4, 0, spi4_rx);
DMA_STREAM_DECLARE(2, 4, 1, spi4_tx);
DMA_STREAM_DECLARE(2, 4, 2, usart1_rx);
DMA_STREAM_DECLARE(2, 4, 3, sdio);
DMA_STREAM_DECLARE(2, 4, 4, spi4_rx);
DMA_STREAM_DECLARE(2, 4, 5, usart1_rx);
DMA_STREAM_DECLARE(2, 4, 6, sdio);
DMA_STREAM_DECLARE(2, 4, 7, usart1_tx);
DMA_STREAM_DECLARE(2, 5, 1, usart6_rx);
DMA_STREAM_DECLARE(2, 5, 2, usart6_rx);
DMA_STREAM_DECLARE(2, 5, 3, spi4_rx);
DMA_STREAM_DECLARE(2, 5, 4, spi4_tx);
DMA_STREAM_DECLARE(2, 5, 5, spi5_tx);
DMA_STREAM_DECLARE(2, 5, 6, usart6_tx);
DMA_STREAM_DECLARE(2, 5, 7, usart6_tx);
DMA_STREAM_DECLARE(2, 6, 0, tim1_trig);
DMA_STREAM_DECLARE(2, 6, 1, tim1_ch1);
DMA_STREAM_DECLARE(2, 6, 2, tim1_ch2);
DMA_STREAM_DECLARE(2, 6, 3, tim1_ch1);
DMA_STREAM_DECLARE(2, 6, 4, tim1_ch4);
DMA_STREAM_DECLARE(2, 6, 4, tim1_trig);
DMA_STREAM_DECLARE(2, 6, 4, tim1_com);
DMA_STREAM_DECLARE(2, 6, 5, tim1_up);
DMA_STREAM_DECLARE(2, 6, 6, tim1_ch3);
DMA_STREAM_DECLARE(2, 7, 1, tim8_up);
DMA_STREAM_DECLARE(2, 7, 2, tim8_ch1);
DMA_STREAM_DECLARE(2, 7, 3, tim8_ch2);
DMA_STREAM_DECLARE(2, 7, 4, tim8_ch3);
DMA_STREAM_DECLARE(2, 7, 5, spi5_rx);
DMA_STREAM_DECLARE(2, 7, 6, spi5_tx);
DMA_STREAM_DECLARE(2, 7, 7, tim8_ch4);
DMA_STREAM_DECLARE(2, 7, 7, tim8_trig);
DMA_STREAM_DECLARE(2, 7, 7, tim8_com);

#define SPI_CFG_DECLARE(n) \
    extern struct stm32_spi_cfg spi ## n ## _cfg;

SPI_CFG_DECLARE(1);
SPI_CFG_DECLARE(2);
SPI_CFG_DECLARE(3);
SPI_CFG_DECLARE(4);
SPI_CFG_DECLARE(5);

#define I2S_PIN_DECLARE(n, port, pin) \
    extern const struct stm32_pin_cfg I2S ## n ## _P ## port ## pin;
#define I2S_CK_PIN_DECLARE(n, port, pin) \
    extern const struct stm32_pin_cfg I2S ## n ## _CK_P ## port ## pin;
#define I2S_WS_PIN_DECLARE(n, port, pin) \
    extern const struct stm32_pin_cfg I2S ## n ## _WS_P ## port ## pin;
#define I2S_SD_PIN_DECLARE(n, port, pin) \
    extern const struct stm32_pin_cfg I2S ## n ## _SD_P ## port ## pin;

/* I2S1 Possible CK pins */
I2S_CK_PIN_DECLARE(1, A, 5)
I2S_CK_PIN_DECLARE(1, B, 3)
/* I2S1 possible WS pins */
I2S_WS_PIN_DECLARE(1, A, 4)
I2S_WS_PIN_DECLARE(1, A, 15)
/* I2S1 possible SD pins */
I2S_SD_PIN_DECLARE(1, B, 5)
I2S_SD_PIN_DECLARE(1, A, 7)
/* I2S2 Possible CKIN pins */
I2S_PIN_DECLARE(2, A, 2)
I2S_PIN_DECLARE(2, B, 11)
I2S_PIN_DECLARE(2, C, 9)
/* I2S2 Possible MCK pins */
I2S_PIN_DECLARE(2, A, 3)
I2S_PIN_DECLARE(2, A, 6)
I2S_PIN_DECLARE(2, C, 6)
/* I2S2 Possible CK pins */
I2S_CK_PIN_DECLARE(2, B, 10)
I2S_CK_PIN_DECLARE(2, B, 13)
I2S_CK_PIN_DECLARE(2, C, 7)
I2S_CK_PIN_DECLARE(2, D, 3)
/* I2S2 possible WS pins */
I2S_WS_PIN_DECLARE(2, B, 9)
I2S_WS_PIN_DECLARE(2, B, 12)
/* I2S2 possible SD pins */
I2S_SD_PIN_DECLARE(2, B, 15)
I2S_SD_PIN_DECLARE(2, C, 3)
/* I2S2_ext possible SD pins */
I2S_SD_PIN_DECLARE(2, B, 14)
I2S_SD_PIN_DECLARE(2, C, 2)
/* I2S3 possible CK pins */
I2S_CK_PIN_DECLARE(3, B, 3)
I2S_CK_PIN_DECLARE(3, C, 10)
/* I2S3 possible WS pins */
I2S_WS_PIN_DECLARE(3, A, 4)
I2S_WS_PIN_DECLARE(3, A, 15)
/* I2S3 possible SD pins */
I2S_SD_PIN_DECLARE(3, B, 5)
I2S_SD_PIN_DECLARE(3, C, 12)
I2S_SD_PIN_DECLARE(3, D, 6)
/* I2S3_ext possible SD pins */
I2S_SD_PIN_DECLARE(3, B, 4)
I2S_SD_PIN_DECLARE(3, C, 11)
/* I2S3 possible MCK pins */
I2S_PIN_DECLARE(3, B, 10)
I2S_PIN_DECLARE(3, C, 7)
/* I2S4 possible CK pins */
I2S_CK_PIN_DECLARE(4, E, 2)
I2S_CK_PIN_DECLARE(4, E, 12)
I2S_CK_PIN_DECLARE(4, B, 13)
/* I2S4 possible WS pins */
I2S_WS_PIN_DECLARE(4, B, 12)
I2S_WS_PIN_DECLARE(4, E, 4)
I2S_WS_PIN_DECLARE(4, E, 11)
/* I2S4 possible SD pins */
I2S_SD_PIN_DECLARE(4, A, 1)
I2S_SD_PIN_DECLARE(4, E, 6)
I2S_SD_PIN_DECLARE(4, E, 14)
/* I2S5 possible CK pins */
I2S_CK_PIN_DECLARE(5, B, 0)
I2S_CK_PIN_DECLARE(5, E, 2)
I2S_CK_PIN_DECLARE(5, E, 12)
/* I2S5 possible WS pins */
I2S_WS_PIN_DECLARE(5, B, 1)
I2S_WS_PIN_DECLARE(5, E, 4)
I2S_WS_PIN_DECLARE(5, E, 11)
/* I2S5 possible SD pins */
I2S_SD_PIN_DECLARE(5, A, 10)
I2S_SD_PIN_DECLARE(5, B, 8)
I2S_SD_PIN_DECLARE(5, E, 6)
I2S_SD_PIN_DECLARE(5, E, 14)

#endif /* _I2S_STM32_H */
