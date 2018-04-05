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

#include <string.h>
#include <assert.h>
#include "os/mynewt.h"
#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>
#include "mcu/stm32_hal.h"
#include "mcu/cmsis_nvic.h"

#define SPI_0_ENABLED (MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE))
#define SPI_1_ENABLED (MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE))
#define SPI_2_ENABLED (MYNEWT_VAL(SPI_2_MASTER) || MYNEWT_VAL(SPI_2_SLAVE))
#define SPI_3_ENABLED (MYNEWT_VAL(SPI_3_MASTER) || MYNEWT_VAL(SPI_3_SLAVE))
#define SPI_4_ENABLED (MYNEWT_VAL(SPI_4_MASTER) || MYNEWT_VAL(SPI_4_SLAVE))
#define SPI_5_ENABLED (MYNEWT_VAL(SPI_5_MASTER) || MYNEWT_VAL(SPI_5_SLAVE))

#define SPI_ENABLED (SPI_0_ENABLED || SPI_1_ENABLED || SPI_2_ENABLED || \
                     SPI_3_ENABLED || SPI_4_ENABLED || SPI_5_ENABLED)

#define STM32_HAL_SPI_TIMEOUT (1000)

#define STM32_HAL_SPI_MAX (6)

extern HAL_StatusTypeDef HAL_SPI_QueueTransmit(SPI_HandleTypeDef *hspi,
        uint8_t *pData, uint16_t Size);

extern HAL_StatusTypeDef HAL_SPI_Slave_Queue_TransmitReceive(
        SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData,
        uint16_t Size);

extern HAL_StatusTypeDef HAL_SPI_Transmit_IT_Custom(SPI_HandleTypeDef *hspi,
        uint8_t *pData, uint16_t Size);

extern HAL_StatusTypeDef HAL_SPI_TransmitReceive_IT_Custom(
        SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData,
        uint16_t Size);

struct stm32_hal_spi {
    SPI_HandleTypeDef handle;
    uint8_t slave:1;                  /* slave or master? */
    uint8_t tx_in_prog:1;             /* slave: tx'ing user data not def */
    uint8_t selected:1;               /* slave: if we see SS */
    uint8_t def_char[4];              /* slave: default data to tx */
    struct stm32_hal_spi_cfg *cfg;
    hal_spi_txrx_cb txrx_cb_func;     /* callback function */
    void            *txrx_cb_arg;     /* callback arguments */
};

static struct stm32_spi_stat {
    uint32_t irq;
    uint32_t ss_irq;
    uint32_t tx;
    uint32_t eovf;
    uint32_t emodf;
    uint32_t efre;
} spi_stat;

#if SPI_ENABLED
static void spi_irq_handler(struct stm32_hal_spi *spi);
#endif

#if SPI_0_ENABLED
struct stm32_hal_spi stm32_hal_spi0;
#endif
#if SPI_1_ENABLED
struct stm32_hal_spi stm32_hal_spi1;
#endif
#if SPI_2_ENABLED
struct stm32_hal_spi stm32_hal_spi2;
#endif
#if SPI_3_ENABLED
struct stm32_hal_spi stm32_hal_spi3;
#endif
#if SPI_4_ENABLED
struct stm32_hal_spi stm32_hal_spi4;
#endif
#if SPI_5_ENABLED
struct stm32_hal_spi stm32_hal_spi5;
#endif

static struct stm32_hal_spi *stm32_hal_spis[STM32_HAL_SPI_MAX] = {
#if SPI_0_ENABLED
    &stm32_hal_spi0,
#else
    NULL,
#endif
#if SPI_1_ENABLED
    &stm32_hal_spi1,
#else
    NULL,
#endif
#if SPI_2_ENABLED
    &stm32_hal_spi2,
#else
    NULL,
#endif
#if SPI_3_ENABLED
    &stm32_hal_spi3,
#else
    NULL,
#endif
#if SPI_4_ENABLED
    &stm32_hal_spi4,
#else
    NULL,
#endif
#if SPI_5_ENABLED
    &stm32_hal_spi5,
#else
    NULL,
#endif
};

#define STM32_HAL_SPI_RESOLVE(__n, __v)                          \
    if ((__n) >= STM32_HAL_SPI_MAX) {                            \
        rc = -1;                                                 \
        goto err;                                                \
    }                                                            \
    (__v) = (struct stm32_hal_spi *) stm32_hal_spis[(__n)];      \
    if ((__v) == NULL) {                                         \
        rc = -1;                                                 \
        goto err;                                                \
    }

static IRQn_Type
stm32_resolve_spi_irq(SPI_HandleTypeDef *hspi)
{
    uintptr_t spi = (uintptr_t)hspi->Instance;

    switch (spi) {
#if SPI_0_ENABLED
    case (uintptr_t)SPI1:
        return SPI1_IRQn;
#endif
#if SPI_1_ENABLED
    case (uintptr_t)SPI2:
        return SPI2_IRQn;
#endif
#if SPI_2_ENABLED
    case (uintptr_t)SPI3:
        return SPI3_IRQn;
#endif
#if SPI_3_ENABLED
    case (uintptr_t)SPI4:
        return SPI4_IRQn;
#endif
#if SPI_4_ENABLED
    case (uintptr_t)SPI5:
        return SPI5_IRQn;
#endif
#if SPI_5_ENABLED
    case (uintptr_t)SPI6:
        return SPI6_IRQn;
#endif
    default:
        assert(0);
    }
}

/*
 * SPI master IRQ handler.
 */
#if SPI_ENABLED
static void
spim_irq_handler(struct stm32_hal_spi *spi)
{
    if (spi->handle.TxXferCount == 0 && spi->handle.RxXferCount == 0) {
        if (spi->txrx_cb_func) {
            spi->txrx_cb_func(spi->txrx_cb_arg, spi->handle.TxXferSize);
        }
    }
}
#endif

/*
 * SPI slave IRQ handler.
 */
#if SPI_ENABLED
static void
spis_irq_handler(struct stm32_hal_spi *spi)
{
    if (spi->tx_in_prog) {
        if (spi->handle.TxXferCount == 0 && spi->handle.RxXferCount == 0) {
            /*
             * If finished with data tx, start transmitting default char
             */
            spi->tx_in_prog = 0;

            HAL_SPI_Transmit_IT_Custom(&spi->handle, spi->def_char, 2);

            if (spi->txrx_cb_func) {
                spi->txrx_cb_func(spi->txrx_cb_arg, spi->handle.TxXferSize);
            }
        }
    } else {
        /*
         * Reset TX pointer within default data.
         */
        spi->handle.pTxBuffPtr = spi->def_char;
        spi->handle.TxXferCount = 2;
    }
}
#endif

/*
 * Common IRQ handler for both master and slave.
 */
#if SPI_ENABLED
static void
spi_irq_handler(struct stm32_hal_spi *spi)
{
    uint32_t err;

    spi_stat.irq++;

    HAL_SPI_IRQHandler(&spi->handle);
    err = spi->handle.ErrorCode;
    if (err) {
        if (err & HAL_SPI_ERROR_OVR) {
            spi_stat.eovf++;
        }
        if (err & HAL_SPI_ERROR_MODF) {
            spi_stat.emodf++;
        }
        if (err & HAL_SPI_ERROR_FRE) {
            spi_stat.efre++;
        }
        spi->handle.ErrorCode = 0;
    }
    if (!spi->slave) {
        spim_irq_handler(spi);
    } else {
        spis_irq_handler(spi);
    }
}
#endif

/*
 * GPIO interrupt when slave gets selected/deselected.
 */
static void
spi_ss_isr(void *arg)
{
    struct stm32_hal_spi *spi = (struct stm32_hal_spi *)arg;
    int ss;
    int len;
    uint16_t reg;

    spi_stat.ss_irq++;
    ss = hal_gpio_read(spi->cfg->ss_pin);
    if (ss == 0 && !spi->selected) {
        /*
         * We're now seeing chip select. Enable SPI, SPI interrupts.
         */
        if (spi->tx_in_prog) {
            __HAL_SPI_ENABLE_IT(&spi->handle,
              SPI_IT_RXNE | SPI_IT_TXE | SPI_IT_ERR);
        } else {
            __HAL_SPI_ENABLE_IT(&spi->handle, SPI_IT_TXE | SPI_IT_ERR);
        }
        reg = spi->handle.Instance->CR1;
        reg &= ~SPI_CR1_SSI;
        reg |= SPI_CR1_SPE;
        spi->handle.Instance->CR1 = reg;
        spi->selected = 1;
    }
    if (ss == 1 && spi->selected) {
        /*
         * Chip select done. Check whether there's pending data to RX.
         */
        if (spi->handle.Instance->SR & SPI_SR_RXNE && spi->handle.RxISR) {
            spi->handle.RxISR(&spi->handle);
        }

        /*
         * Disable SPI.
         */
        reg = spi->handle.Instance->CR1;
        reg &= ~SPI_CR1_SPE;
        reg |= SPI_CR1_SSI;
        spi->handle.Instance->CR1 = reg;

        __HAL_SPI_DISABLE_IT(&spi->handle, SPI_IT_RXNE|SPI_IT_TXE|SPI_IT_ERR);

        len = spi->handle.RxXferSize - spi->handle.RxXferCount;
        if (len) {
            /*
             * If some data was clocked out, reset to start sending
             * default data and call callback, if user was waiting for
             * data.
             */

            spi->handle.State = HAL_SPI_STATE_READY;

            HAL_SPI_QueueTransmit(&spi->handle, spi->def_char, 2);

            if (spi->tx_in_prog) {
                spi->tx_in_prog = 0;

                if (spi->txrx_cb_func) {
                    spi->txrx_cb_func(spi->txrx_cb_arg, len);
                }
            }
        }
        spi->selected = 0;
    }
}

#if SPI_0_ENABLED
static void
spi1_irq_handler(void)
{
    spi_irq_handler(stm32_hal_spis[0]);
}
#endif

#if SPI_1_ENABLED
static void
spi2_irq_handler(void)
{
    spi_irq_handler(stm32_hal_spis[1]);
}
#endif

#if SPI_2_ENABLED
static void
spi3_irq_handler(void)
{
    spi_irq_handler(stm32_hal_spis[2]);
}
#endif

#if SPI_3_ENABLED
static void
spi4_irq_handler(void)
{
    spi_irq_handler(stm32_hal_spis[3]);
}
#endif

#if SPI_4_ENABLED
static void
spi5_irq_handler(void)
{
    spi_irq_handler(stm32_hal_spis[4]);
}
#endif


#if SPI_5_ENABLED
static void
spi6_irq_handler(void)
{
    spi_irq_handler(stm32_hal_spis[5]);
}
#endif

uint32_t
stm32_resolve_spi_irq_handler(SPI_HandleTypeDef *hspi)
{
    switch((uintptr_t)hspi->Instance) {
#if SPI_0_ENABLED
    case (uintptr_t)SPI1:
        return (uint32_t)&spi1_irq_handler;
#endif
#if SPI_1_ENABLED
    case (uintptr_t)SPI2:
        return (uint32_t)&spi2_irq_handler;
#endif
#if SPI_2_ENABLED
    case (uintptr_t)SPI3:
        return (uint32_t)&spi3_irq_handler;
#endif
#if SPI_3_ENABLED
    case (uintptr_t)SPI4:
        return (uint32_t)&spi4_irq_handler;
#endif
#if SPI_4_ENABLED
    case (uintptr_t)SPI5:
        return (uint32_t)&spi5_irq_handler;
#endif
#if SPI_5_ENABLED
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
    struct stm32_hal_spi *spi;
    int rc;

    /* Check for valid arguments */
    rc = -1;
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
    STM32_HAL_SPI_RESOLVE(spi_num, spi);

    spi->cfg = usercfg;
    spi->slave = (spi_type == HAL_SPI_TYPE_SLAVE);

    return (0);
err:
    return (rc);
}

static int
stm32_spi_resolve_prescaler(uint8_t spi_num, uint32_t baudrate, uint32_t *prescaler)
{
    uint32_t candidate_br;
    uint32_t apbfreq;
    int i;

    /* SPIx {1,4,5,6} use PCLK2 on the STM32F4/F7, otherwise use PCKL1.
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

    if (baudrate == 0) {
        *prescaler = 0;
        return 0;
    }

    /* Calculate best-fit prescaler: divide the clock by each subsequent
     * prescaler until we reach the highest prescaler that generates at
     * _most_ the baudrate.
     */
    *prescaler = SPI_BAUDRATEPRESCALER_256;
    for (i = 0; i < 8; i++) {
        candidate_br = apbfreq >> (i + 1);
        if (candidate_br <= baudrate) {
            *prescaler = i << SPI_CR1_BR_Pos;
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
    struct stm32_hal_spi *spi;
    int rc = 0;
    int sr;

    STM32_HAL_SPI_RESOLVE(spi_num, spi);

    __HAL_DISABLE_INTERRUPTS(sr);
    spi->txrx_cb_func = txrx_cb;
    spi->txrx_cb_arg = arg;
    __HAL_ENABLE_INTERRUPTS(sr);
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
    struct stm32_hal_spi *spi;
    int rc;

    rc = 0;
    STM32_HAL_SPI_RESOLVE(spi_num, spi);

    /* XXX power up */
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
    struct stm32_hal_spi *spi;
    int rc;

    rc = 0;
    STM32_HAL_SPI_RESOLVE(spi_num, spi);

    /* XXX power down */
err:
    return rc;
}

int
hal_spi_config(int spi_num, struct hal_spi_settings *settings)
{
    struct stm32_hal_spi *spi;
    struct stm32_hal_spi_cfg *cfg;
    SPI_InitTypeDef *init;
    GPIO_InitTypeDef gpio;
    uint32_t gpio_speed;
    IRQn_Type irq;
    uint32_t prescaler;
    int rc;
    int sr;

    __HAL_DISABLE_INTERRUPTS(sr);
    STM32_HAL_SPI_RESOLVE(spi_num, spi);

    init = &spi->handle.Init;

    cfg = spi->cfg;

    if (!spi->slave) {
        spi->handle.Init.NSS = SPI_NSS_HARD_OUTPUT;
        spi->handle.Init.Mode = SPI_MODE_MASTER;
    } else {
        spi->handle.Init.NSS = SPI_NSS_SOFT;
        spi->handle.Init.Mode = SPI_MODE_SLAVE;
    }

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;

    /* TODO: also VERY_HIGH for STM32L1x */
    if (settings->baudrate <= 2000) {
        gpio_speed = GPIO_SPEED_FREQ_LOW;
    } else if (settings->baudrate <= 12500) {
        gpio_speed = GPIO_SPEED_FREQ_MEDIUM;
    } else {
        gpio_speed = GPIO_SPEED_FREQ_HIGH;
    }

    /* Enable the clocks for this SPI */
    switch (spi_num) {
#if SPI_0_ENABLED
    case 0:
        __HAL_RCC_SPI1_CLK_ENABLE();
#if !defined(STM32F1)
        gpio.Alternate = GPIO_AF5_SPI1;
#endif
        spi->handle.Instance = SPI1;
        break;
#endif
#if SPI_1_ENABLED
    case 1:
        __HAL_RCC_SPI2_CLK_ENABLE();
#if !defined(STM32F1)
        gpio.Alternate = GPIO_AF5_SPI2;
#endif
        spi->handle.Instance = SPI2;
        break;
#endif
#if SPI_2_ENABLED
    case 2:
        __HAL_RCC_SPI3_CLK_ENABLE();
#if !defined(STM32F1)
        gpio.Alternate = GPIO_AF6_SPI3;
#endif
        spi->handle.Instance = SPI3;
        break;
#endif
#if SPI_3_ENABLED
    case 3:
        __HAL_RCC_SPI4_CLK_ENABLE();
#if !defined(STM32F1)
        gpio.Alternate = GPIO_AF5_SPI4;
#endif
        spi->handle.Instance = SPI4;
        break;
#endif
#if SPI_4_ENABLED
    case 4:
        __HAL_RCC_SPI5_CLK_ENABLE();
#if !defined(STM32F1)
        gpio.Alternate = GPIO_AF5_SPI5;
#endif
        spi->handle.Instance = SPI5;
        break;
#endif
#if SPI_5_ENABLED
    case 5:
        __HAL_RCC_SPI6_CLK_ENABLE();
#if !defined(STM32F1)
        gpio.Alternate = GPIO_AF5_SPI6;
#endif
        spi->handle.Instance = SPI6;
        break;
#endif
   default:
        assert(0);
        rc = -1;
        goto err;
    }

    if (!spi->slave) {
        if (settings->data_mode == HAL_SPI_MODE2 ||
          settings->data_mode == HAL_SPI_MODE3) {
            gpio.Pull = GPIO_PULLUP;
        } else {
            gpio.Pull = GPIO_PULLDOWN;
        }
    }

    /* NOTE: Errata ES0125: STM32L100x6/8/B, STM32L151x6/8/B and
     * STM32L152x6/8/B ultra-low-power device limitations.
     *
     * 2.6.6 - Corrupted last bit of data and or CRC, received in Master
     * mode with delayed SCK feedback
     *
     * This driver is always using very high speed for SCK on STM32L1x
     */

#if defined(STM32L152xC)
    if (!spi->slave) {
        gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    } else {
        gpio.Speed = gpio_speed;
    }
#else
    gpio.Speed = gpio_speed;
#endif

    rc = hal_gpio_init_stm(cfg->sck_pin, &gpio);
    if (rc != 0) {
        goto err;
    }

#if defined(STM32L152xC)
    if (!spi->slave) {
        gpio.Speed = gpio_speed;
    }
#endif

    if (!spi->slave) {
        gpio.Pull = GPIO_NOPULL;
    } else {
        gpio.Mode = GPIO_MODE_AF_OD;
    }

    rc = hal_gpio_init_stm(cfg->mosi_pin, &gpio);
    if (rc != 0) {
        goto err;
    }
    if (!spi->slave) {
        gpio.Mode = GPIO_MODE_AF_OD;
    } else {
        gpio.Mode = GPIO_MODE_AF_PP;
    }
    rc = hal_gpio_init_stm(cfg->miso_pin, &gpio);
    if (rc != 0) {
        goto err;
    }

    switch (settings->data_mode) {
    case HAL_SPI_MODE0:
        init->CLKPolarity = SPI_POLARITY_LOW;
        init->CLKPhase = SPI_PHASE_1EDGE;
        break;
    case HAL_SPI_MODE1:
        init->CLKPolarity = SPI_POLARITY_LOW;
        init->CLKPhase = SPI_PHASE_2EDGE;
        break;
    case HAL_SPI_MODE2:
        init->CLKPolarity = SPI_POLARITY_HIGH;
        init->CLKPhase = SPI_PHASE_1EDGE;
        break;
    case HAL_SPI_MODE3:
        init->CLKPolarity = SPI_POLARITY_HIGH;
        init->CLKPhase = SPI_PHASE_2EDGE;
        break;
    default:
        rc = -1;
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
        rc = -1;
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
        rc = -1;
        goto err;
    }

    rc = stm32_spi_resolve_prescaler(spi_num, settings->baudrate * 1000, &prescaler);
    if (rc != 0) {
        goto err;
    }

    init->BaudRatePrescaler = prescaler;

    /* Add default values */
    init->Direction = SPI_DIRECTION_2LINES;
    init->TIMode = SPI_TIMODE_DISABLE;
    init->CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    init->CRCPolynomial = 1;
#ifdef SPI_NSS_PULSE_DISABLE
    init->NSSPMode = SPI_NSS_PULSE_DISABLE;
#endif

    irq = stm32_resolve_spi_irq(&spi->handle);
    NVIC_SetPriority(irq, cfg->irq_prio);
    NVIC_SetVector(irq, stm32_resolve_spi_irq_handler(&spi->handle));
    NVIC_EnableIRQ(irq);

    /* Init, Enable */
    rc = HAL_SPI_Init(&spi->handle);
    if (rc != 0) {
        goto err;
    }
    if (spi->slave) {
        hal_spi_slave_set_def_tx_val(spi_num, 0);
        rc = hal_gpio_irq_init(cfg->ss_pin, spi_ss_isr, spi, HAL_GPIO_TRIG_BOTH,
                               HAL_GPIO_PULL_UP);
        spi_ss_isr(spi);
    }
    __HAL_ENABLE_INTERRUPTS(sr);
    return (0);
err:
    __HAL_ENABLE_INTERRUPTS(sr);
    return (rc);
}

int
hal_spi_txrx_noblock(int spi_num, void *txbuf, void *rxbuf, int len)
{
    struct stm32_hal_spi *spi;
    int rc;
    int sr;

    STM32_HAL_SPI_RESOLVE(spi_num, spi);

    spi_stat.tx++;
    rc = -1;
    __HAL_DISABLE_INTERRUPTS(sr);
    if (!spi->slave) {
        rc = HAL_SPI_TransmitReceive_IT_Custom(&spi->handle, txbuf, rxbuf, len);
    } else {
        /*
         * Slave: if selected, start transmitting new data.
         * If not selected, queue it for transmission.
         */
        spi->handle.State = HAL_SPI_STATE_READY;
        if (spi->selected) {
            rc = HAL_SPI_TransmitReceive_IT_Custom(&spi->handle, txbuf, rxbuf, len);
        } else {
            rc = HAL_SPI_Slave_Queue_TransmitReceive(&spi->handle, txbuf, rxbuf,
              len);
        }
        if (rc == 0) {
            spi->tx_in_prog = 1;
        }
    }
    __HAL_ENABLE_INTERRUPTS(sr);
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
    struct stm32_hal_spi *spi;
    int rc;
    int sr;
    int i;

    STM32_HAL_SPI_RESOLVE(spi_num, spi);

    if (spi->slave) {
        rc = 0;
        __HAL_DISABLE_INTERRUPTS(sr);
        if (spi->handle.Init.DataSize == SPI_DATASIZE_8BIT) {
            for (i = 0; i < 4; i++) {
                ((uint8_t *)spi->def_char)[i] = val;
            }
        } else {
            for (i = 0; i < 2; i++) {
                ((uint16_t *)spi->def_char)[i] = val;
            }
        }
        if (!spi->tx_in_prog) {
            /*
             * Replaces the current default char in tx buffer register.
             */
            spi->handle.State = HAL_SPI_STATE_READY;
            rc = HAL_SPI_QueueTransmit(&spi->handle, spi->def_char, 2);
            assert(rc == 0);
        }
        __HAL_ENABLE_INTERRUPTS(sr);
    } else {
        rc = -1;
    }
err:
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
    struct stm32_hal_spi *spi;
    uint16_t retval;
    int len;
    int sr;

    STM32_HAL_SPI_RESOLVE(spi_num, spi);
    if (spi->slave) {
        retval = -1;
        goto err;
    }
    if (spi->handle.Init.DataSize == SPI_DATASIZE_8BIT) {
        len = sizeof(uint8_t);
    } else {
        len = sizeof(uint16_t);
    }
    __HAL_DISABLE_INTERRUPTS(sr);
    spi_stat.tx++;
    rc = HAL_SPI_TransmitReceive(&spi->handle,(uint8_t *)&val,
                                 (uint8_t *)&retval, len,
                                 STM32_HAL_SPI_TIMEOUT);
    __HAL_ENABLE_INTERRUPTS(sr);
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
    struct stm32_hal_spi *spi;
    int sr;

    rc = -1;
    if (!len) {
        goto err;
    }
    STM32_HAL_SPI_RESOLVE(spi_num, spi);
    if (spi->slave) {
        goto err;
    }
    __HAL_DISABLE_INTERRUPTS(sr);
    spi_stat.tx++;
    __HAL_SPI_ENABLE(&spi->handle);
    rc = HAL_SPI_TransmitReceive(&spi->handle, (uint8_t *)txbuf,
                                 (uint8_t *)rxbuf, len,
                                 STM32_HAL_SPI_TIMEOUT);
    __HAL_ENABLE_INTERRUPTS(sr);
    if (rc != HAL_OK) {
        rc = -1;
        goto err;
    }
    rc = 0;
err:
    return rc;
}

int
hal_spi_abort(int spi_num)
{
    int rc;
    struct stm32_hal_spi *spi;
    int sr;

    rc = 0;
    STM32_HAL_SPI_RESOLVE(spi_num, spi);
    if (spi->slave) {
        goto err;
    }
    __HAL_DISABLE_INTERRUPTS(sr);
    spi->handle.State = HAL_SPI_STATE_READY;
    __HAL_SPI_DISABLE_IT(&spi->handle, SPI_IT_TXE | SPI_IT_RXNE | SPI_IT_ERR);
    spi->handle.Instance->CR1 &= ~SPI_CR1_SPE;
    __HAL_ENABLE_INTERRUPTS(sr);
err:
    return rc;
}
