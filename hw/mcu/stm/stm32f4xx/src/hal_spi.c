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
#include "bsp/cmsis_nvic.h"
#include "console/console.h"

#define STM32F4_HAL_SPI_TIMEOUT (1000)

#define STM32F4_HAL_SPI_MAX (6)

struct stm32f4_hal_spi {
    SPI_HandleTypeDef handle;

    /* Callback and arguments */
    hal_spi_txrx_cb txrx_cb_func;
    void            *txrx_cb_arg;
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

static struct stm32f4_hal_spi *stm32f4_hal_spis[STM32F4_HAL_SPI_MAX] = {
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

#define STM32F4_HAL_SPI_RESOLVE(__n, __v)                        \
    if ((__n) >= STM32F4_HAL_SPI_MAX) {                          \
        rc = EINVAL;                                             \
        goto err;                                                \
    }                                                            \
    (__v) = (struct stm32f4_hal_spi *) stm32f4_hal_spis[(__n)];  \
    if ((__v) == NULL) {                                         \
        rc = EINVAL;                                             \
        goto err;                                                \
    }

static IRQn_Type
stm32f4_resolve_spi_irq(SPI_HandleTypeDef *hspi)
{
    uintptr_t spi = (uintptr_t)hspi->Instance;

    switch(spi) {
        case (uintptr_t)SPI1:
            return SPI1_IRQn;
        case (uintptr_t)SPI2:
            return SPI2_IRQn;
        case (uintptr_t)SPI3:
            return SPI3_IRQn;
#ifdef SPI4
        case (uintptr_t)SPI4:
            return SPI4_IRQn;
#endif
#ifdef SPI5
        case (uintptr_t)SPI5:
            return SPI5_IRQn;
#endif
#ifdef SPI6
        case (uintptr_t)SPI6:
            return SPI6_IRQn;
#endif
        default:
            assert(0);
    }
}

static void
spi1_irq_handler(void)
{
    HAL_SPI_IRQHandler(&stm32f4_hal_spis[0]->handle);
    if (stm32f4_hal_spis[0]->txrx_cb_func) {
        stm32f4_hal_spis[0]->txrx_cb_func(stm32f4_hal_spis[0]->txrx_cb_arg, 1);
    }
}

static void
spi2_irq_handler(void)
{
    HAL_SPI_IRQHandler(&stm32f4_hal_spis[1]->handle);
    if (stm32f4_hal_spis[1]->txrx_cb_func) {
        stm32f4_hal_spis[1]->txrx_cb_func(stm32f4_hal_spis[1]->txrx_cb_arg, 1);
    }
}

static void
spi3_irq_handler(void)
{
    HAL_SPI_IRQHandler(&stm32f4_hal_spis[2]->handle);
    if (stm32f4_hal_spis[2]->txrx_cb_func) {
        stm32f4_hal_spis[2]->txrx_cb_func(stm32f4_hal_spis[2]->txrx_cb_arg, 1);
    }
}

#ifdef SPI4
static void
spi4_irq_handler(void)
{
    HAL_SPI_IRQHandler(&stm32f4_hal_spis[3]->handle);
    if (stm32f4_hal_spis[3]->txrx_cb_func) {
        stm32f4_hal_spis[3]->txrx_cb_func(stm32f4_hal_spis[3]->txrx_cb_arg, 1);
    }
}
#endif

#ifdef SPI5
static void
spi5_irq_handler(void)
{
    HAL_SPI_IRQHandler(&stm32f4_hal_spis[4]->handle);
    if (stm32f4_hal_spis[4]->txrx_cb_func) {
        stm32f4_hal_spis[4]->txrx_cb_func(stm32f4_hal_spis[4]->txrx_cb_arg, 1);
    }
}
#endif


#ifdef SPI6
static void
spi6_irq_handler(void)
{
    HAL_SPI_IRQHandler(&stm32f4_hal_spis[5]->handle);

    if (stm32f4_hal_spis[5]->txrx_cb_func) {
        stm32f4_hal_spis[5]->txrx_cb_func(stm32f4_hal_spis[5]->txrx_cb_arg, 1);

    }
}
#endif

uint32_t
stm32f4_resolve_spi_irq_handler(SPI_HandleTypeDef *hspi)
{
    switch((uintptr_t)hspi->Instance) {
        /* DMA2 */
        case (uintptr_t)SPI1:
            return (uint32_t)&spi1_irq_handler;
        case (uintptr_t)SPI2:
            return (uint32_t)&spi2_irq_handler;
        case (uintptr_t)SPI3:
            return (uint32_t)&spi3_irq_handler;
#ifdef SPI4
        case (uintptr_t)SPI4:
            return (uint32_t)&spi4_irq_handler;
#endif
#ifdef SPI5
        case (uintptr_t)SPI5:
            return (uint32_t)&spi5_irq_handler;
#endif
#ifdef SPI6
        case (uintptr_t)SPI6:
            return (uint32_t)&spi6_irq_handler;
#endif
        default:
            assert(0);
    }
}

int
hal_spi_init(int spi_num, void *usercfg, uint8_t spi_type)
{
    struct stm32f4_hal_spi *spi;
    struct stm32f4_hal_spi_cfg *cfg;
    SPI_InitTypeDef *init;
    GPIO_InitTypeDef pcf;
    int rc;

    /* Check for valid arguments */
    rc = EINVAL;
    if (usercfg == NULL) {
        goto err;
    }

    if ((spi_type != HAL_SPI_TYPE_MASTER) && (spi_type != HAL_SPI_TYPE_SLAVE)) {
        goto err;
    }

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
            (&spi->handle)->Instance = SPI1;
            break;
        case 1:
            __HAL_RCC_SPI2_CLK_ENABLE();
            pcf.Alternate = GPIO_AF5_SPI2;
            (&spi->handle)->Instance = SPI2;
            break;
        case 2:
            __HAL_RCC_SPI3_CLK_ENABLE();
            pcf.Alternate = GPIO_AF6_SPI3;
            (&spi->handle)->Instance = SPI3;
            break;
#ifdef SPI4
        case 3:
            __HAL_RCC_SPI4_CLK_ENABLE();
            pcf.Alternate = GPIO_AF5_SPI4;
            (&spi->handle)->Instance = SPI4;
            break;
#endif
#ifdef SPI5
        case 4:
            __HAL_RCC_SPI5_CLK_ENABLE();
            pcf.Alternate = GPIO_AF5_SPI5;
            (&spi->handle)->Instance = SPI5;
            break;
#endif
#ifdef SPI6
        case 5:
            __HAL_RCC_SPI6_CLK_ENABLE();
            pcf.Alternate = GPIO_AF5_SPI6;
            (&spi->handle)->Instance = SPI6;
            break;
#endif
       default:
            assert(0);
            rc = EINVAL;
            goto err;
    }

    rc = hal_gpio_init_stm(cfg->sck_pin, &pcf);
    if (rc != 0) {
        goto err;
    }

    //rc = hal_gpio_init_out(cfg->sck_pin, 0);
    //if (rc != 0) {
    //    goto err;
    //}

    rc = hal_gpio_init_stm(cfg->miso_pin, &pcf);
    if (rc != 0) {
        goto err;
    }

    //rc = hal_gpio_init_in(cfg->miso_pin, 0);
    //if (rc != 0) {
    //    goto err;
    //}

    rc = hal_gpio_init_stm(cfg->mosi_pin, &pcf);
    if (rc != 0) {
        goto err;
    }

    // rc = hal_gpio_init_out(cfg->mosi_pin, 0);
    //if (rc != 0) {
    //    goto err;
    //}

    NVIC_SetPriority(stm32f4_resolve_spi_irq(&spi->handle),
                     NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
    NVIC_SetVector(stm32f4_resolve_spi_irq(&spi->handle),
                   stm32f4_resolve_spi_irq_handler(&spi->handle));
    NVIC_EnableIRQ(stm32f4_resolve_spi_irq(&spi->handle));

    console_printf("miso:%u, mosi:%u, sck:%u\n", cfg->mosi_pin, cfg->miso_pin, cfg->sck_pin);

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

/**
 * Sets the txrx callback (executed at interrupt context) when the
 * buffer is transferred by the master or the slave using the non-blocking API.
 * Cannot be called when the spi is enabled. This callback will also be called
 * when chip select is de-asserted on the slave.
 *
 * NOTE: This callback is only used for the non-blocking interface and must
 * be called prior to using the non-blocking API.
 *
 * @param spi_num   SPI interface on which to set callback
 * @param txrx      Callback function
 * @param arg       Argument to be passed to callback function
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_set_txrx_cb(int spi_num, hal_spi_txrx_cb txrx_cb, void *arg)
{
    int rc;
    struct stm32f4_hal_spi *spi;

    STM32F4_HAL_SPI_RESOLVE(spi_num, spi);

    /* Return indicating error if not enabled */
    if (((&spi->handle)->Instance->CR1 & SPI_CR1_SPE) == 0) {
        rc = -1;
    } else {
        spi->txrx_cb_func = txrx_cb;
        spi->txrx_cb_arg = arg;
        rc = 0;
    }

err:
    return rc;
}

/**
 * Enables the SPI. This does not start a transmit or receive operation;
 * it is used for power mgmt. Cannot be called when a SPI transfer is in
 * progress.
 *
 * @param spi_num
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_enable(int spi_num)
{
    struct stm32f4_hal_spi *spi;
    int rc;

    rc = 0;
    STM32F4_HAL_SPI_RESOLVE(spi_num, spi);

    __HAL_SPI_ENABLE(&spi->handle);

err:
    return rc;
}

/**
 * Disables the SPI. Used for power mgmt. It will halt any current SPI transfers
 * in progress.
 *
 * @param spi_num
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_disable(int spi_num)
{
    struct stm32f4_hal_spi *spi;
    int rc;

    rc = 0;
    STM32F4_HAL_SPI_RESOLVE(spi_num, spi);

    __HAL_SPI_DISABLE(&spi->handle);

err:
    return rc;
}

int
hal_spi_config(int spi_num, struct hal_spi_settings *settings)
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
hal_spi_txrx_noblock(int spi_num, void *txbuf, void *rxbuf, int len)
{
    struct stm32f4_hal_spi *spi;
    int rc;

    STM32F4_HAL_SPI_RESOLVE(spi_num, spi);

    rc = EINVAL;
    rc = HAL_SPI_TransmitReceive_IT(&spi->handle, (uint8_t *) &txbuf,
                                    (uint8_t *) &rxbuf, len);
err:
    return (rc);
}

/**
 * Sets the default value transferred by the slave. Not valid for master
 *
 * @param spi_num SPI interface to use
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_slave_set_def_tx_val(int spi_num, uint16_t val)
{
    int rc;

    rc = EINVAL;
    return rc;
}

/**
 * Blocking call to send a value on the SPI. Returns the value received from the
 * SPI slave.
 *
 * MASTER: Sends the value and returns the received value from the slave.
 * SLAVE: Invalid API. Returns 0xFFFF
 *
 * @param spi_num   Spi interface to use
 * @param val       Value to send
 *
 * @return uint16_t Value received on SPI interface from slave. Returns 0xFFFF
 * if called when the SPI is configured to be a slave
 */
uint16_t hal_spi_tx_val(int spi_num, uint16_t val)
{
    int rc;
    struct stm32f4_hal_spi *spi;
    uint16_t retval;

    STM32F4_HAL_SPI_RESOLVE(spi_num, spi);

    rc = HAL_SPI_TransmitReceive(&spi->handle,(uint8_t *)&val,
                                 (uint8_t *)&retval, 2,
                                 STM32F4_HAL_SPI_TIMEOUT);
    if (rc != HAL_OK) {
        retval = 0xFFFF;
    }

err:
    return retval;
}

/**
 * Blocking interface to send a buffer and store the received values from the
 * slave. The transmit and receive buffers are either arrays of 8-bit (uint8_t)
 * values or 16-bit values depending on whether the spi is configured for 8 bit
 * data or more than 8 bits per value. The 'cnt' parameter is the number of
 * 8-bit or 16-bit values. Thus, if 'cnt' is 10, txbuf/rxbuf would point to an
 * array of size 10 (in bytes) if the SPI is using 8-bit data; otherwise
 * txbuf/rxbuf would point to an array of size 20 bytes (ten, uint16_t values).
 *
 * NOTE: these buffers are in the native endian-ness of the platform.
 *
 *     MASTER: master sends all the values in the buffer and stores the
 *             stores the values in the receive buffer if rxbuf is not NULL.
 *             The txbuf parameter cannot be NULL.
 *     SLAVE: cannot be called for a slave; returns -1
 *
 * @param spi_num   SPI interface to use
 * @param txbuf     Pointer to buffer where values to transmit are stored.
 * @param rxbuf     Pointer to buffer to store values received from peer.
 * @param cnt       Number of 8-bit or 16-bit values to be transferred.
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_txrx(int spi_num, void *txbuf, void *rxbuf, int len)
{
    int rc;
    struct stm32f4_hal_spi *spi;

    rc = EINVAL;
    if (!len) {
        goto err;
    }

    STM32F4_HAL_SPI_RESOLVE(spi_num, spi);

    rc = HAL_SPI_TransmitReceive(&spi->handle, (uint8_t *)txbuf,
                                 (uint8_t *)rxbuf, len,
                                 STM32F4_HAL_SPI_TIMEOUT);
    if (rc != HAL_OK) {
        rc = EINVAL;
        goto err;
    }
    rc = 0;
err:
    return rc;
}
