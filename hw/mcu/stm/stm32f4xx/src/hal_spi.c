/**
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

#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>

#include <string.h>
#include <errno.h>

#include <assert.h>

#include "stm32f4xx.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_spi.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_gpio_ex.h"
#include "stm32f4xx_hal_rcc.h"
#include "mcu/stm32f4xx_mynewt_hal.h"

#define STM32F4_HAL_SPI_TIMEOUT (1000)

#define STM32F4_HAL_SPI_MAX (6)

struct stm32f4_hal_spi {
    SPI_HandleTypeDef handle;
};

#ifdef SPI1
struct stm32f4_hal_spi stm32f4_hal_spi1;
#else
#define __HAL_RCC_SPI1_CLK_ENABLE()
#endif
#ifdef SPI2
struct stm32f4_hal_spi stm32f4_hal_spi2;
#else
#define __HAL_RCC_SPI2_CLK_ENABLE()
#endif
#ifdef SPI3
struct stm32f4_hal_spi stm32f4_hal_spi3;
#else
#define __HAL_RCC_SPI3_CLK_ENABLE()
#endif
#ifdef SPI4
struct stm32f4_hal_spi stm32f4_hal_spi4;
#else
#undef __HAL_RCC_SPI4_CLK_ENABLE
#define __HAL_RCC_SPI4_CLK_ENABLE()
#endif
#ifdef SPI5
struct stm32f4_hal_spi stm32f4_hal_spi5;
#else
#define __HAL_RCC_SPI5_CLK_ENABLE()
#endif
#ifdef SPI6
struct stm32f4_hal_spi stm32f4_hal_spi6;
#else
#define __HAL_RCC_SPI6_CLK_ENABLE()
#endif

struct stm32f4_hal_spi *stm32f4_hal_spis[STM32F4_HAL_SPI_MAX] = {
#ifdef SPI1
    &stm32f4_hal_spi1,
#else
    NULL,
#endif
#ifdef SPI2
    &stm32f4_hal_spi2,
#else
    NULL,
#endif
#ifdef SPI3
    &stm32f4_hal_spi3,
#else
    NULL,
#endif
#ifdef SPI4
    &stm32f4_hal_spi4,
#else
    NULL,
#endif
#ifdef SPI5
    &stm32f4_hal_spi5,
#else
    NULL,
#endif
#ifdef SPI6
    &stm32f4_hal_spi6,
#else
    NULL,
#endif
};

#define STM32F4_HAL_SPI_RESOLVE(__n, __v)       \
    if ((__n) >= STM32F4_HAL_SPI_MAX) {         \
        rc = EINVAL;                            \
        goto err;                               \
    }                                           \
    (__v) = stm32f4_hal_spis[(__n)];            \
    if ((__v) == NULL) {                        \
        rc = EINVAL;                            \
        goto err;                               \
    }

int
hal_spi_init(uint8_t spi_num, void *usercfg)
{
    struct stm32f4_hal_spi *spi;
    struct stm32f4_hal_spi_cfg *cfg;
    SPI_InitTypeDef *init;
    GPIO_InitTypeDef pcf;
    int rc;

    /* Allow user to specify default init settings for the SPI.
     * This can be done in BSP, so that only the generic SPI settings
     * are passed to the user configure() call.
     */
    STM32F4_HAL_SPI_RESOLVE(spi_num, spi);

    cfg = (struct stm32f4_hal_spi_cfg *) usercfg;

    init = (SPI_InitTypeDef *) cfg->spi_settings;
    if (init != NULL) {
        memcpy(&spi->handle.Init, init, sizeof(SPI_InitTypeDef));
    } else {
        /* Provide default initialization of SPI */
    }

    pcf.Mode = GPIO_MODE_AF_PP;
    pcf.Pull = GPIO_NOPULL;
    pcf.Speed = GPIO_SPEED_FREQ_HIGH;

    /* Enable the clocks for this SPI */
    switch (spi_num) {
        case 0:
            __HAL_RCC_SPI1_CLK_ENABLE();
            pcf.Alternate = GPIO_AF5_SPI1;
            break;
        case 1:
            __HAL_RCC_SPI2_CLK_ENABLE();
            pcf.Alternate = GPIO_AF5_SPI2;
            break;
        case 2:
            __HAL_RCC_SPI3_CLK_ENABLE();
            pcf.Alternate = GPIO_AF6_SPI3;
            break;
        case 3:
            __HAL_RCC_SPI4_CLK_ENABLE();
#ifdef SPI4
            pcf.Alternate = GPIO_AF5_SPI4;
#endif
            break;
        case 4:
            __HAL_RCC_SPI5_CLK_ENABLE();
#ifdef SPI5
            pcf.Alternate = GPIO_AF5_SPI5;
#endif
            break;
        case 5:
            __HAL_RCC_SPI6_CLK_ENABLE();
#ifdef SPI6
            pcf.Alternate = GPIO_AF5_SPI6;
#endif
            break;
    }

    rc = hal_gpio_init_stm(cfg->sck_pin, &pcf);
    if (rc != 0) {
        goto err;
    }

    rc = hal_gpio_init_stm(cfg->miso_pin, &pcf);
    if (rc != 0) {
        goto err;
    }

    rc = hal_gpio_init_stm(cfg->mosi_pin, &pcf);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int
stm32f4_spi_resolve_prescaler(uint8_t spi_num, uint32_t baudrate, uint32_t *prescaler)
{
    uint32_t candidate_br;
    uint32_t apbfreq;
    int i;

    /* SPIx {1,4,5,6} use PCLK2 on the STM32F4, otherwise use PCKL1.
     * The numbers in the switch below are offset by 1, because the HALs index
     * SPI ports from 0.
     */
    switch (spi_num) {
        case 0:
        case 3:
        case 4:
        case 5:
            apbfreq = HAL_RCC_GetPCLK2Freq();
            break;
        default:
            apbfreq = HAL_RCC_GetPCLK1Freq();
            break;
    }

    /* Calculate best-fit prescaler: divide the clock by each subsequent prescalar until
     * we reach the highest prescalar that generates at _most_ the baudrate.
     */
    *prescaler = SPI_BAUDRATEPRESCALER_256;
    for (i = 0; i < 8; i++) {
        candidate_br = apbfreq >> (i + 1);
        if (candidate_br <= baudrate) {
            *prescaler = i << 3;
            break;
        }
    }

    return (0);
}

int
hal_spi_config(uint8_t spi_num, struct hal_spi_settings *settings)
{
    struct stm32f4_hal_spi *spi;
    SPI_InitTypeDef *init;
    uint32_t prescaler;
    int rc;

    STM32F4_HAL_SPI_RESOLVE(spi_num, spi);

    init = &spi->handle.Init;

    switch (settings->data_mode) {
        case HAL_SPI_MODE0:
            init->CLKPolarity = SPI_POLARITY_HIGH;
            init->CLKPhase = SPI_PHASE_1EDGE;
            break;
        case HAL_SPI_MODE1:
            init->CLKPolarity = SPI_POLARITY_HIGH;
            init->CLKPhase = SPI_PHASE_2EDGE;
            break;
        case HAL_SPI_MODE2:
            init->CLKPolarity = SPI_POLARITY_LOW;
            init->CLKPhase = SPI_PHASE_1EDGE;
            break;
        case HAL_SPI_MODE3:
            init->CLKPolarity = SPI_POLARITY_LOW;
            init->CLKPhase = SPI_PHASE_2EDGE;
            break;
        default:
            rc = EINVAL;
            goto err;
    }

    switch (settings->data_order) {
        case HAL_SPI_MSB_FIRST:
            init->FirstBit = SPI_FIRSTBIT_MSB;
            break;
        case HAL_SPI_LSB_FIRST:
            init->FirstBit = SPI_FIRSTBIT_LSB;
            break;
        default:
            rc = EINVAL;
            goto err;
    }

    switch (settings->word_size) {
        case HAL_SPI_WORD_SIZE_8BIT:
            init->DataSize = SPI_DATASIZE_8BIT;
            break;
        case HAL_SPI_WORD_SIZE_9BIT:
            init->DataSize = SPI_DATASIZE_16BIT;
            break;
        default:
            rc = EINVAL;
            goto err;
    }

    rc = stm32f4_spi_resolve_prescaler(spi_num, settings->baudrate, &prescaler);
    if (rc != 0) {
        goto err;
    }

    init->BaudRatePrescaler = prescaler;

    /* Disable, Init, Enable */
    __HAL_SPI_DISABLE(&spi->handle);

    rc = HAL_SPI_Init(&spi->handle);
    if (rc != 0) {
        goto err;
    }

    __HAL_SPI_ENABLE(&spi->handle);

    return (0);
err:
    return (rc);
}

int
hal_spi_master_transfer(uint8_t spi_num, uint16_t tx)
{
    struct stm32f4_hal_spi *spi;
    uint16_t rx;
    int rc;

    STM32F4_HAL_SPI_RESOLVE(spi_num, spi);

    rx = 0;
    rc = HAL_SPI_TransmitReceive(&spi->handle, (uint8_t *) &tx, (uint8_t *) &rx,
            sizeof(uint16_t), STM32F4_HAL_SPI_TIMEOUT);
    if (rc != 0) {
        goto err;
    }

    return (rx);
err:
    return (rc);
}

