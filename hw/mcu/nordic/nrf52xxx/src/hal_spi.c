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

#include <string.h>
#include <errno.h>

#include <nrf.h>
#include <nrf_drv_spi.h>
#include <app_util_platform.h>

/*
 * XXX: Should handle 9-bit SPI values in SW.
 */

#define NRF52_HAL_SPI_MAX (2)

struct nrf52_hal_spi {
    nrf_drv_spi_t nhs_spi;
};

#if SPI0_ENABLED
struct nrf52_hal_spi nrf52_hal_spi0;
#endif
#if SPI1_ENABLED
struct nrf52_hal_spi nrf52_hal_spi1;
#endif

struct nrf52_hal_spi *nrf52_hal_spis[NRF52_HAL_SPI_MAX] = {
#if SPI0_ENABLED
    &nrf52_hal_spi0,
#else
    NULL,
#endif
#if SPI1_ENABLED
    &nrf52_hal_spi1
#else
    NULL
#endif
};

#if SPI0_ENABLED
nrf_drv_spi_config_t cfg_spi0 = NRF_DRV_SPI_DEFAULT_CONFIG(0);
#endif
#if SPI1_ENABLED
nrf_drv_spi_config_t cfg_spi1 = NRF_DRV_SPI_DEFAULT_CONFIG(1);
#endif

#define NRF52_HAL_SPI_RESOLVE(__n, __v)         \
    if ((__n) >= NRF52_HAL_SPI_MAX) {           \
        rc = EINVAL;                            \
        goto err;                               \
    }                                           \
    (__v) = nrf52_hal_spis[(__n)];              \
    if ((__v) == NULL) {                        \
        rc = EINVAL;                            \
        goto err;                               \
    }

int
hal_spi_init(uint8_t spi_num, void *cfg)
{
    return (0);
}

int
hal_spi_config(uint8_t spi_num, struct hal_spi_settings *settings)
{
    struct nrf52_hal_spi *spi;
    nrf_drv_spi_config_t cfg;
    int rc;

    NRF52_HAL_SPI_RESOLVE(spi_num, spi);

    if (spi_num == 0) {
#if SPI0_ENABLED
        memcpy(&cfg, &cfg_spi0, sizeof(cfg_spi0));
#else
        rc = EINVAL;
        goto err;
#endif
    } else if (spi_num == 1) {
#if SPI1_ENABLED
        memcpy(&cfg, &cfg_spi1, sizeof(cfg_spi1));
#else
        rc = EINVAL;
        goto err;
#endif
    } else {
        rc = EINVAL;
        goto err;
    }

    switch (settings->data_mode) {
        case HAL_SPI_MODE0:
            cfg.mode = NRF_DRV_SPI_MODE_0;
            break;
        case HAL_SPI_MODE1:
            cfg.mode = NRF_DRV_SPI_MODE_1;
            break;
        case HAL_SPI_MODE2:
            cfg.mode = NRF_DRV_SPI_MODE_2;
            break;
        case HAL_SPI_MODE3:
            cfg.mode = NRF_DRV_SPI_MODE_3;
            break;
    }

    switch (settings->data_order) {
        case HAL_SPI_MSB_FIRST:
            cfg.bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST;
            break;
        case HAL_SPI_LSB_FIRST:
            cfg.bit_order = NRF_DRV_SPI_BIT_ORDER_LSB_FIRST;
            break;
    }

    /* Only 8-bit word sizes supported. */
    switch (settings->word_size) {
        case HAL_SPI_WORD_SIZE_8BIT:
            break;
        default:
            rc = EINVAL;
            goto err;
    }

    switch (settings->baudrate) {
        case 125:
            cfg.frequency = NRF_DRV_SPI_FREQ_125K;
            break;
        case 250:
            cfg.frequency = NRF_DRV_SPI_FREQ_250K;
            break;
        case 500:
            cfg.frequency = NRF_DRV_SPI_FREQ_500K;
            break;
        case 1000:
            cfg.frequency = NRF_DRV_SPI_FREQ_1M;
            break;
        case 2000:
            cfg.frequency = NRF_DRV_SPI_FREQ_2M;
            break;
        case 4000:
            cfg.frequency = NRF_DRV_SPI_FREQ_4M;
            break;
        case 8000:
            cfg.frequency = NRF_DRV_SPI_FREQ_8M;
            break;
        default:
            rc = EINVAL;
            goto err;
    }

    rc = nrf_drv_spi_init(&spi->nhs_spi, &cfg, NULL);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
hal_spi_master_transfer(uint8_t spi_num, uint16_t tx)
{
    struct nrf52_hal_spi *spi;
    uint8_t b;
    uint8_t rsp;
    int rc;

    NRF52_HAL_SPI_RESOLVE(spi_num, spi);

    /* Only 8-bit transfers supported, this is checked in
     * nrf52_hal_spi_config().
     */
    b = (uint8_t) tx;

    rc = nrf_drv_spi_transfer(&spi->nhs_spi, &b, sizeof(b), &rsp, sizeof(rsp));
    if (rc != 0) {
        goto err;
    }

    return (rsp);
err:
    return (rc);
}

