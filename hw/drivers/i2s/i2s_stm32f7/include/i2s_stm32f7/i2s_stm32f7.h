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

#include <stm32f7xx_hal.h>
#include <i2s_stm32f7/stm32_pin_cfg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct i2s;
struct i2s_cfg;
struct stm32_spi_cfg;

struct stm32_i2s_pins {
    stm32_pin_cfg_t ck_pin;
    stm32_pin_cfg_t ws_pin;
    stm32_pin_cfg_t sd_pin;
    stm32_pin_cfg_t mck_pin;
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
    struct stm32_i2s_pins pins;
};

#define SPI_CFG(n) &(spi ## n ## _cfg)

struct stm32_i2s {
    I2S_HandleTypeDef hi2s;
    DMA_HandleTypeDef *hdma_spi;

    struct i2s *i2s;
    struct i2s_sample_buffer *dma_buffers[2];
    uint8_t dma_buffer_count;
};

#define DMA_CFG(dma, ch, st, name) &(name ## _stream ## st ## _channel ## ch)

#define DMA_STREAM_DECLARE(dma, ch, st, name) \
    extern const struct stm32_dma_cfg name ## _stream ## st ## _channel ## ch

DMA_STREAM_DECLARE(1, 0, 0, spi3_rx);
DMA_STREAM_DECLARE(1, 0, 1, spdifrx_dt);
DMA_STREAM_DECLARE(1, 0, 2, spi3_rx);
DMA_STREAM_DECLARE(1, 0, 3, spi2_rx);
DMA_STREAM_DECLARE(1, 0, 4, spi2_tx);
DMA_STREAM_DECLARE(1, 0, 5, spi3_tx);
DMA_STREAM_DECLARE(1, 0, 6, spdifrx_cs);
DMA_STREAM_DECLARE(1, 0, 7, spi3_tx);

DMA_STREAM_DECLARE(1, 1, 0, i2c1_rx);
DMA_STREAM_DECLARE(1, 1, 1, i2c3_rx);
DMA_STREAM_DECLARE(1, 1, 2, tim7_up);
DMA_STREAM_DECLARE(1, 1, 4, tim7_up);
DMA_STREAM_DECLARE(1, 1, 5, i2c1_rx);
DMA_STREAM_DECLARE(1, 1, 6, i2c1_tx);
DMA_STREAM_DECLARE(1, 1, 7, i2c1_tx);

DMA_STREAM_DECLARE(1, 2, 0, tim4_ch1);
DMA_STREAM_DECLARE(1, 2, 2, i2c4_rx);
DMA_STREAM_DECLARE(1, 2, 3, tim4_ch2);
DMA_STREAM_DECLARE(1, 2, 5, i2c4_rx);
DMA_STREAM_DECLARE(1, 2, 6, tim4_up);
DMA_STREAM_DECLARE(1, 2, 7, tim4_ch3);

DMA_STREAM_DECLARE(1, 3, 1, tim2_up);
DMA_STREAM_DECLARE(1, 3, 1, tim2_ch3);
DMA_STREAM_DECLARE(1, 3, 2, i2c3_rx);
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

DMA_STREAM_DECLARE(1, 5, 0, uart8_tx);
DMA_STREAM_DECLARE(1, 5, 1, uart7_tx);
DMA_STREAM_DECLARE(1, 5, 2, tim3_ch4);
DMA_STREAM_DECLARE(1, 5, 2, tim3_up);
DMA_STREAM_DECLARE(1, 5, 3, uart7_rx);
DMA_STREAM_DECLARE(1, 5, 4, tim3_ch1);
DMA_STREAM_DECLARE(1, 5, 4, tim3_trig);
DMA_STREAM_DECLARE(1, 5, 5, tim3_ch2);
DMA_STREAM_DECLARE(1, 5, 6, uart8_rx);
DMA_STREAM_DECLARE(1, 5, 7, tim3_ch3);

DMA_STREAM_DECLARE(1, 6, 0, tim5_ch3);
DMA_STREAM_DECLARE(1, 6, 0, tim5_up);
DMA_STREAM_DECLARE(1, 6, 1, tim5_ch4);
DMA_STREAM_DECLARE(1, 6, 1, tim5_trig);
DMA_STREAM_DECLARE(1, 6, 2, tim5_ch1);
DMA_STREAM_DECLARE(1, 6, 2, tim3_up);
DMA_STREAM_DECLARE(1, 6, 3, tim5_ch4);
DMA_STREAM_DECLARE(1, 6, 3, tim5_trig);
DMA_STREAM_DECLARE(1, 6, 4, tim5_ch2);
DMA_STREAM_DECLARE(1, 6, 6, tim5_up);

DMA_STREAM_DECLARE(1, 7, 1, tim6_up);
DMA_STREAM_DECLARE(1, 7, 2, i2c2_rx);
DMA_STREAM_DECLARE(1, 7, 3, i2c2_rx);
DMA_STREAM_DECLARE(1, 7, 4, usart3_tx);
DMA_STREAM_DECLARE(1, 7, 5, dac1);
DMA_STREAM_DECLARE(1, 7, 6, dac2);
DMA_STREAM_DECLARE(1, 7, 7, i2c2_tx);

DMA_STREAM_DECLARE(1, 8, 0, i2c3_tx);
DMA_STREAM_DECLARE(1, 8, 1, i2c4_rx);
DMA_STREAM_DECLARE(1, 8, 4, i2c2_tx);
DMA_STREAM_DECLARE(1, 8, 6, i2c4_tx);

DMA_STREAM_DECLARE(1, 9, 1, i2c2_rx);
DMA_STREAM_DECLARE(1, 9, 6, i2c2_tx);

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
DMA_STREAM_DECLARE(2, 0, 7, sai1_b);

DMA_STREAM_DECLARE(2, 1, 1, dcmi);
DMA_STREAM_DECLARE(2, 1, 2, adc2);
DMA_STREAM_DECLARE(2, 1, 3, adc2);
DMA_STREAM_DECLARE(2, 1, 4, sai1_b);
DMA_STREAM_DECLARE(2, 1, 5, spi6_tx);
DMA_STREAM_DECLARE(2, 1, 6, spi6_rx);
DMA_STREAM_DECLARE(2, 1, 7, dcmi);

DMA_STREAM_DECLARE(2, 2, 0, adc3);
DMA_STREAM_DECLARE(2, 2, 1, adc3);
DMA_STREAM_DECLARE(2, 2, 3, spi5_rx);
DMA_STREAM_DECLARE(2, 2, 4, spi5_tx);
DMA_STREAM_DECLARE(2, 2, 5, cryp_out);
DMA_STREAM_DECLARE(2, 2, 6, cryp_in);
DMA_STREAM_DECLARE(2, 2, 7, hash_in);

DMA_STREAM_DECLARE(2, 3, 0, spi1_rx);
DMA_STREAM_DECLARE(2, 3, 2, spi1_rx);
DMA_STREAM_DECLARE(2, 3, 3, spi1_tx);
DMA_STREAM_DECLARE(2, 3, 4, sai2_a);
DMA_STREAM_DECLARE(2, 3, 5, spi1_tx);
DMA_STREAM_DECLARE(2, 3, 6, sai2_b);
DMA_STREAM_DECLARE(2, 3, 7, quadspi);

DMA_STREAM_DECLARE(2, 4, 0, spi4_rx);
DMA_STREAM_DECLARE(2, 4, 1, spi4_tx);
DMA_STREAM_DECLARE(2, 4, 2, usart1_rx);
DMA_STREAM_DECLARE(2, 4, 3, sdmmc1);
DMA_STREAM_DECLARE(2, 4, 5, usart1_rx);
DMA_STREAM_DECLARE(2, 4, 6, sdmmc1);
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

DMA_STREAM_DECLARE(2, 8, 0, dfsdm1_flt0);
DMA_STREAM_DECLARE(2, 8, 1, dfsdm1_flt1);
DMA_STREAM_DECLARE(2, 8, 2, dfsdm1_flt2);
DMA_STREAM_DECLARE(2, 8, 3, dfsdm1_flt3);
DMA_STREAM_DECLARE(2, 8, 4, dfsdm1_flt0);
DMA_STREAM_DECLARE(2, 8, 5, dfsdm1_flt1);
DMA_STREAM_DECLARE(2, 8, 6, dfsdm1_flt2);
DMA_STREAM_DECLARE(2, 8, 7, dfsdm1_flt3);

DMA_STREAM_DECLARE(2, 9, 0, jpeg_in);
DMA_STREAM_DECLARE(2, 9, 1, jpeg_out);
DMA_STREAM_DECLARE(2, 9, 2, spi4_tx);
DMA_STREAM_DECLARE(2, 9, 3, jpeg_in);
DMA_STREAM_DECLARE(2, 9, 4, jpeg_out);
DMA_STREAM_DECLARE(2, 9, 5, spi5_rx);

DMA_STREAM_DECLARE(2, 10, 0, sai1_b);
DMA_STREAM_DECLARE(2, 10, 1, sai2_b);
DMA_STREAM_DECLARE(2, 10, 2, sai2_a);
DMA_STREAM_DECLARE(2, 10, 6, sai1_a);

DMA_STREAM_DECLARE(2, 11, 0, sdmmc2);
DMA_STREAM_DECLARE(2, 11, 2, suadspi);
DMA_STREAM_DECLARE(2, 11, 5, sdmmc2);

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
I2S_CK_PIN_DECLARE(1, A, 5);
I2S_CK_PIN_DECLARE(1, B, 3);
I2S_CK_PIN_DECLARE(1, G, 11);

/* I2S1 possible WS pins */
I2S_WS_PIN_DECLARE(1, A, 4);
I2S_WS_PIN_DECLARE(1, A, 15);
I2S_WS_PIN_DECLARE(1, G, 10);

/* I2S1 possible SD pins */
I2S_SD_PIN_DECLARE(1, B, 5);
I2S_SD_PIN_DECLARE(1, A, 7);
I2S_SD_PIN_DECLARE(1, D, 7);

/* I2S1 possible MCK pins */
I2S_SD_PIN_DECLARE(1, C, 4);

/* I2S2 Possible CKIN pins */
I2S_PIN_DECLARE(2, C, 9);

/* I2S2 Possible MCK pins */
#ifdef GPIO_AF5_SPI2
I2S_PIN_DECLARE(2, C, 6);
#endif

/* I2S2 Possible CK pins */
I2S_CK_PIN_DECLARE(2, A, 9);
I2S_CK_PIN_DECLARE(2, A, 12);
I2S_CK_PIN_DECLARE(2, B, 10);
I2S_CK_PIN_DECLARE(2, B, 13);
I2S_CK_PIN_DECLARE(2, D, 3);
I2S_CK_PIN_DECLARE(2, I, 1);

/* I2S2 possible WS pins */
I2S_WS_PIN_DECLARE(2, A, 11);
I2S_WS_PIN_DECLARE(2, B, 4);
I2S_WS_PIN_DECLARE(2, B, 9);
I2S_WS_PIN_DECLARE(2, B, 12);
I2S_WS_PIN_DECLARE(2, I, 0);

/* I2S2 possible SD pins */
I2S_SD_PIN_DECLARE(2, B, 15);
I2S_SD_PIN_DECLARE(2, C, 1);
I2S_SD_PIN_DECLARE(2, C, 3);
I2S_SD_PIN_DECLARE(2, I, 3);

/* I2S3 possible CK pins */
I2S_CK_PIN_DECLARE(3, B, 3);
I2S_CK_PIN_DECLARE(3, C, 10);

/* I2S3 possible WS pins */
I2S_WS_PIN_DECLARE(3, A, 4);
I2S_WS_PIN_DECLARE(3, A, 15);

/* I2S3 possible SD pins */
I2S_SD_PIN_DECLARE(3, B, 2);
I2S_SD_PIN_DECLARE(3, B, 5);
I2S_SD_PIN_DECLARE(3, C, 12);
#ifdef GPIO_AF5_SPI3
I2S_SD_PIN_DECLARE(3, D, 6);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _I2S_STM32_H */
