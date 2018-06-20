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
#include <errno.h>
#include <assert.h>
#include "os/mynewt.h"
#include <mcu/cmsis_nvic.h>
#include <hal/hal_spi.h>
#include "mcu/nrf52_hal.h"
#include "nrf.h"

#ifndef min
#define min(a, b) ((a)<(b)?(a):(b))
#endif

/* IRQ handler type */
typedef void (*nrf52_spi_irq_handler_t)(void);

/* XXX:
 * 1) what about stats?
 * 2) Dealing with errors (OVERFLOW, OVERREAD)
 * 3) Dont think I need dummy_rx as I can set master RX maxcnt to zero.
 */

/* The maximum number of SPI interfaces we will allow */
#define NRF52_HAL_SPI_MAX (3)

/* Used to disable all interrupts */
#define NRF_SPI_IRQ_DISABLE_ALL 0xFFFFFFFF

/*
 *  Slave states
 *
 *  IDLE: Slave not ready to be used. If master attempts to access
 *        slave it will receive the default character
 *  ACQ_SEM: Slave is attempting to acquire semaphore.
 *  READY: Slave is ready for master to send it data
 *
 */
#define HAL_SPI_SLAVE_STATE_IDLE        (0)
#define HAL_SPI_SLAVE_STATE_ACQ_SEM     (1)
#define HAL_SPI_SLAVE_STATE_READY       (2)

struct nrf52_hal_spi
{
    uint8_t spi_type;
    uint8_t spi_xfr_flag;   /* Master only */
    uint8_t dummy_rx;       /* Master only */
    uint8_t slave_state;    /* Slave only */
    uint16_t nhs_buflen;
    uint16_t nhs_bytes_txd;
    struct hal_spi_settings spi_cfg; /* Slave and master */

    /* Pointer to HW registers */
    union {
        NRF_SPIM_Type *spim;
        NRF_SPIS_Type *spis;
    } nhs_spi;

    /* IRQ number */
    IRQn_Type irq_num;

    /* Pointers to tx/rx buffers */
    uint8_t *nhs_txbuf;
    uint8_t *nhs_rxbuf;

    /* Callback and arguments */
    hal_spi_txrx_cb txrx_cb_func;
    void            *txrx_cb_arg;
};

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
struct nrf52_hal_spi nrf52_hal_spi0;
#endif
#if MYNEWT_VAL(SPI_1_MASTER)  || MYNEWT_VAL(SPI_1_SLAVE)
struct nrf52_hal_spi nrf52_hal_spi1;
#endif
#if MYNEWT_VAL(SPI_2_MASTER)  || MYNEWT_VAL(SPI_2_SLAVE)
struct nrf52_hal_spi nrf52_hal_spi2;
#endif

static const struct nrf52_hal_spi *nrf52_hal_spis[NRF52_HAL_SPI_MAX] = {
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
    &nrf52_hal_spi0,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_1_MASTER)  || MYNEWT_VAL(SPI_1_SLAVE)
    &nrf52_hal_spi1,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_2_MASTER)  || MYNEWT_VAL(SPI_2_SLAVE)
    &nrf52_hal_spi2,
#else
    NULL,
#endif
};

#define NRF52_HAL_SPI_RESOLVE(__n, __v)                     \
    if ((__n) >= NRF52_HAL_SPI_MAX) {                       \
        rc = EINVAL;                                        \
        goto err;                                           \
    }                                                       \
    (__v) = (struct nrf52_hal_spi *) nrf52_hal_spis[(__n)]; \
    if ((__v) == NULL) {                                    \
        rc = EINVAL;                                        \
        goto err;                                           \
    }

#if (MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_2_MASTER))
static void
nrf52_irqm_handler(struct nrf52_hal_spi *spi)
{
    uint8_t xfr_bytes;
    uint16_t len;
    NRF_SPIM_Type *spim;

    spim = spi->nhs_spi.spim;
    if (spim->EVENTS_END) {
        spim->EVENTS_END = 0;

        /* Should not occur but if no transfer just leave  */
        if (spi->spi_xfr_flag == 0) {
            return;
        }

        /* Are there more bytes to send? */
        xfr_bytes = spim->TXD.AMOUNT;
        spi->nhs_bytes_txd += xfr_bytes;
        if (spi->nhs_bytes_txd < spi->nhs_buflen) {
            spi->nhs_txbuf += xfr_bytes;
            len = spi->nhs_buflen - spi->nhs_bytes_txd;
            len = min(255, len);
            spim->TXD.PTR = (uint32_t)spi->nhs_txbuf;
            spim->TXD.MAXCNT = (uint8_t)len;

            /* If no rxbuf, we need to set rxbuf and maxcnt to 1 */
            if (spi->nhs_rxbuf) {
                spi->nhs_rxbuf += xfr_bytes;
                spim->RXD.PTR    = (uint32_t)spi->nhs_rxbuf;
                spim->RXD.MAXCNT = (uint8_t)len;
            }
            spim->TASKS_START = 1;
        } else {
            if (spi->txrx_cb_func) {
                spi->txrx_cb_func(spi->txrx_cb_arg, spi->nhs_buflen);

            }
            spi->spi_xfr_flag = 0;
            spim->INTENCLR = SPIM_INTENSET_END_Msk;
        }
    }
}
#endif

#if (MYNEWT_VAL(SPI_0_SLAVE)  || MYNEWT_VAL(SPI_1_SLAVE) || MYNEWT_VAL(SPI_2_SLAVE))
static void
nrf52_irqs_handler(struct nrf52_hal_spi *spi)
{
    uint8_t xfr_len;
    NRF_SPIS_Type *spis;

    spis = spi->nhs_spi.spis;

    /* Semaphore acquired event */
    if (spis->EVENTS_ACQUIRED) {
        spis->EVENTS_ACQUIRED = 0;

        if (spi->slave_state == HAL_SPI_SLAVE_STATE_ACQ_SEM) {
            if (spi->nhs_txbuf == NULL) {
                spis->TXD.PTR = 0;
                spis->TXD.MAXCNT = 0;
            } else {
                spis->TXD.PTR = (uint32_t)spi->nhs_txbuf;
                spis->TXD.MAXCNT = spi->nhs_buflen;
            }

            if (spi->nhs_rxbuf == NULL) {
                spis->RXD.PTR = 0;
                spis->RXD.MAXCNT = 0;
            } else {
                spis->RXD.PTR = (uint32_t)spi->nhs_rxbuf;
                spis->RXD.MAXCNT = spi->nhs_buflen;
            }
            spis->TASKS_RELEASE = 1;
            spi->slave_state = HAL_SPI_SLAVE_STATE_READY;
        }
    }

    /* SPI transaction complete */
    if (spis->EVENTS_END) {
        spis->EVENTS_END = 0;
        if (spi->slave_state == HAL_SPI_SLAVE_STATE_READY) {
            if (spi->txrx_cb_func) {
                /* Get transfer length */
                if (spi->nhs_txbuf == NULL) {
                    xfr_len = spis->RXD.AMOUNT;
                } else {
                    xfr_len = spis->TXD.AMOUNT;
                }
                spi->txrx_cb_func(spi->txrx_cb_arg, xfr_len);
            }
            spi->slave_state = HAL_SPI_SLAVE_STATE_IDLE;
        }
    }

}
#endif

/* Interrupt handlers for SPI ports */
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
void
nrf52_spi0_irq_handler(void)
{
    os_trace_isr_enter();
    if (nrf52_hal_spi0.spi_type == HAL_SPI_TYPE_MASTER) {
#if MYNEWT_VAL(SPI_0_MASTER)
        nrf52_irqm_handler(&nrf52_hal_spi0);
#endif
    } else {
#if MYNEWT_VAL(SPI_0_SLAVE)
        nrf52_irqs_handler(&nrf52_hal_spi0);
#endif
    }
    os_trace_isr_exit();
}
#endif

#if MYNEWT_VAL(SPI_1_MASTER)  || MYNEWT_VAL(SPI_1_SLAVE)
void
nrf52_spi1_irq_handler(void)
{
    os_trace_isr_enter();
    if (nrf52_hal_spi1.spi_type == HAL_SPI_TYPE_MASTER) {
#if MYNEWT_VAL(SPI_1_MASTER)
        nrf52_irqm_handler(&nrf52_hal_spi1);
#endif
    } else {
#if MYNEWT_VAL(SPI_1_SLAVE)
        nrf52_irqs_handler(&nrf52_hal_spi1);
#endif
    }
    os_trace_isr_exit();
}
#endif

#if MYNEWT_VAL(SPI_2_MASTER)  || MYNEWT_VAL(SPI_2_SLAVE)
void
nrf52_spi2_irq_handler(void)
{
    os_trace_isr_enter();
    if (nrf52_hal_spi2.spi_type == HAL_SPI_TYPE_MASTER) {
#if MYNEWT_VAL(SPI_2_MASTER)
        nrf52_irqm_handler(&nrf52_hal_spi2);
#endif
    } else {
#if MYNEWT_VAL(SPI_2_SLAVE)
        nrf52_irqs_handler(&nrf52_hal_spi2);
#endif
    }
    os_trace_isr_exit();
}
#endif

static void
hal_spi_stop_transfer(NRF_SPIM_Type *spim)
{
    spim->TASKS_STOP = 1;
    while (!spim->EVENTS_STOPPED) {}
    spim->EVENTS_STOPPED = 0;
}

static int
hal_spi_config_master(struct nrf52_hal_spi *spi,
                      struct hal_spi_settings *settings)
{
    int rc;
    uint32_t nrf_config;
    uint32_t frequency;
    NRF_SPIM_Type *spim;

    spim = spi->nhs_spi.spim;
    memcpy(&spi->spi_cfg, settings, sizeof(*settings));

    /* Only 8-bit word sizes supported. */
    rc = 0;
    switch (settings->word_size) {
        case HAL_SPI_WORD_SIZE_8BIT:
            break;
        default:
            rc = EINVAL;
            break;
    }

    switch (settings->data_mode) {
        case HAL_SPI_MODE0:
            nrf_config = (SPIM_CONFIG_CPOL_ActiveHigh << SPIM_CONFIG_CPOL_Pos) |
                         (SPIM_CONFIG_CPHA_Leading << SPIM_CONFIG_CPHA_Pos);
            break;
        case HAL_SPI_MODE1:
            nrf_config = (SPIM_CONFIG_CPOL_ActiveHigh << SPIM_CONFIG_CPOL_Pos) |
                         (SPIM_CONFIG_CPHA_Trailing << SPIM_CONFIG_CPHA_Pos);
            break;
        case HAL_SPI_MODE2:
            nrf_config = (SPIM_CONFIG_CPOL_ActiveLow << SPIM_CONFIG_CPOL_Pos) |
                         (SPIM_CONFIG_CPHA_Leading << SPIM_CONFIG_CPHA_Pos);
            break;
        case HAL_SPI_MODE3:
            nrf_config = (SPIM_CONFIG_CPOL_ActiveLow << SPIM_CONFIG_CPOL_Pos) |
                         (SPIM_CONFIG_CPHA_Trailing << SPIM_CONFIG_CPHA_Pos);
            break;
        default:
            nrf_config = 0;
            rc = EINVAL;
            break;
    }

    /* NOTE: msb first is 0 so no check done */
    if (settings->data_order == HAL_SPI_LSB_FIRST) {
        nrf_config |= SPIM_CONFIG_ORDER_LsbFirst;
    }
    spim->CONFIG = nrf_config;

    switch (settings->baudrate) {
        case 125:
            frequency = SPIM_FREQUENCY_FREQUENCY_K125;
            break;
        case 250:
            frequency = SPIM_FREQUENCY_FREQUENCY_K250;
            break;
        case 500:
            frequency = SPIM_FREQUENCY_FREQUENCY_K500;
            break;
        case 1000:
            frequency = SPIM_FREQUENCY_FREQUENCY_M1;
            break;
        case 2000:
            frequency = SPIM_FREQUENCY_FREQUENCY_M2;
            break;
        case 4000:
            frequency = SPIM_FREQUENCY_FREQUENCY_M4;
            break;
        case 8000:
            frequency = SPIM_FREQUENCY_FREQUENCY_M8;
            break;
        default:
            frequency = 0;
            rc = EINVAL;
            break;
    }
    spim->FREQUENCY = frequency;

    return rc;
}

static int
hal_spi_config_slave(struct nrf52_hal_spi *spi,
                     struct hal_spi_settings *settings)
{
    int rc;
    uint32_t nrf_config;
    NRF_SPIS_Type *spis;

    spis = spi->nhs_spi.spis;

    rc = 0;
    switch (settings->data_mode) {
        case HAL_SPI_MODE0:
            nrf_config = (SPIS_CONFIG_CPOL_ActiveHigh << SPIS_CONFIG_CPOL_Pos) |
                         (SPIS_CONFIG_CPHA_Leading << SPIS_CONFIG_CPHA_Pos);
            break;
        case HAL_SPI_MODE1:
            nrf_config = (SPIS_CONFIG_CPOL_ActiveHigh << SPIS_CONFIG_CPOL_Pos) |
                         (SPIS_CONFIG_CPHA_Trailing << SPIS_CONFIG_CPHA_Pos);
            break;
        case HAL_SPI_MODE2:
            nrf_config = (SPIS_CONFIG_CPOL_ActiveLow << SPIS_CONFIG_CPOL_Pos) |
                         (SPIS_CONFIG_CPHA_Leading << SPIS_CONFIG_CPHA_Pos);
            break;
        case HAL_SPI_MODE3:
            nrf_config = (SPIS_CONFIG_CPOL_ActiveLow << SPIS_CONFIG_CPOL_Pos) |
                         (SPIS_CONFIG_CPHA_Trailing << SPIS_CONFIG_CPHA_Pos);
            break;
        default:
            nrf_config = 0;
            rc = EINVAL;
            break;
    }

    if (settings->data_order == HAL_SPI_LSB_FIRST) {
        nrf_config |= SPIS_CONFIG_ORDER_LsbFirst;
    }
    spis->CONFIG = nrf_config;

    /* Only 8-bit word sizes supported. */
    switch (settings->word_size) {
        case HAL_SPI_WORD_SIZE_8BIT:
            break;
        default:
            rc = EINVAL;
            break;
    }

    return rc;
}

static int
hal_spi_init_master(struct nrf52_hal_spi *spi,
                    struct nrf52_hal_spi_cfg *cfg,
                    nrf52_spi_irq_handler_t handler)
{
    NRF_SPIM_Type *spim;
    NRF_GPIO_Type *port;
    uint32_t pin;

    /* Configure SCK */
    port = HAL_GPIO_PORT(cfg->sck_pin);
    pin = HAL_GPIO_INDEX(cfg->sck_pin);
    if (spi->spi_cfg.data_mode <= HAL_SPI_MODE1) {
        port->OUTCLR = (1UL << pin);
    } else {
        port->OUTSET = (1UL << pin);
    }
    port->PIN_CNF[pin] =
        (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos);

    /*  Configure MOSI */
    port = HAL_GPIO_PORT(cfg->mosi_pin);
    pin = HAL_GPIO_INDEX(cfg->mosi_pin);
    port->OUTCLR = (1UL << pin);
    port->PIN_CNF[pin] =
        ((uint32_t)GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
        ((uint32_t)GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

    /* Configure MISO */
    port = HAL_GPIO_PORT(cfg->miso_pin);
    pin = HAL_GPIO_INDEX(cfg->miso_pin);
    port->PIN_CNF[pin] =
        ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
        ((uint32_t)GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos);

    spim = (NRF_SPIM_Type *)spi->nhs_spi.spim;
    spim->PSEL.SCK = cfg->sck_pin;
    spim->PSEL.MOSI = cfg->mosi_pin;
    spim->PSEL.MISO = cfg->miso_pin;

    spim->INTENCLR = NRF_SPI_IRQ_DISABLE_ALL;
    NVIC_SetVector(spi->irq_num, (uint32_t)handler);
    NVIC_SetPriority(spi->irq_num, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_ClearPendingIRQ(spi->irq_num);
    NVIC_EnableIRQ(spi->irq_num);

    return 0;
}

static int
hal_spi_init_slave(struct nrf52_hal_spi *spi,
                   struct nrf52_hal_spi_cfg *cfg,
                   nrf52_spi_irq_handler_t handler)
{
    NRF_SPIS_Type *spis;
    NRF_GPIO_Type *port;
    uint32_t pin;

    /* NOTE: making this pin an input is correct! See datasheet */
    port = HAL_GPIO_PORT(cfg->miso_pin);
    pin = HAL_GPIO_INDEX(cfg->miso_pin);
    port->PIN_CNF[pin] =
        ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
        ((uint32_t)GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos);

    port = HAL_GPIO_PORT(cfg->mosi_pin);
    pin = HAL_GPIO_INDEX(cfg->mosi_pin);
    port->PIN_CNF[pin] =
        ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
        ((uint32_t)GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos);

    port = HAL_GPIO_PORT(cfg->ss_pin);
    pin = HAL_GPIO_INDEX(cfg->ss_pin);
    port->PIN_CNF[pin] =
        ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
        ((uint32_t)GPIO_PIN_CNF_PULL_Pullup  << GPIO_PIN_CNF_PULL_Pos) |
        ((uint32_t)GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos);

    port = HAL_GPIO_PORT(cfg->sck_pin);
    pin = HAL_GPIO_INDEX(cfg->sck_pin);
    port->PIN_CNF[pin] =
        ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
        ((uint32_t)GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos);

    spis = (NRF_SPIS_Type *)spi->nhs_spi.spis;
    spis->PSEL.SCK  = cfg->sck_pin;
    spis->PSEL.MOSI = cfg->mosi_pin;
    spis->PSEL.MISO = cfg->miso_pin;
    spis->PSEL.CSN  = cfg->ss_pin;

    /* Disable interrupt and clear any interrupt events */
    spis->INTENCLR = SPIS_INTENSET_ACQUIRED_Msk | SPIS_INTENSET_END_Msk;
    spis->EVENTS_END = 0;
    spis->EVENTS_ACQUIRED = 0;

    /* Enable END_ACQUIRE shortcut. */
    spis->SHORTS = SPIS_SHORTS_END_ACQUIRE_Msk;

    /* Set interrupt vector and enable IRQ */
    NVIC_SetVector(spi->irq_num, (uint32_t)handler);
    NVIC_SetPriority(spi->irq_num, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_ClearPendingIRQ(spi->irq_num);
    NVIC_EnableIRQ(spi->irq_num);

    return 0;
}

/**
 * Initialize the SPI, given by spi_num.
 *
 * @param spi_num The number of the SPI to initialize
 * @param cfg HW/MCU specific configuration,
 *            passed to the underlying implementation, providing extra
 *            configuration.
 * @param spi_type SPI type (master or slave)
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_init(int spi_num, void *cfg, uint8_t spi_type)
{
    int rc;
    struct nrf52_hal_spi *spi;
    nrf52_spi_irq_handler_t irq_handler;

    NRF52_HAL_SPI_RESOLVE(spi_num, spi);

    /* Check for valid arguments */
    rc = EINVAL;
    if (cfg == NULL) {
        goto err;
    }

    if ((spi_type != HAL_SPI_TYPE_MASTER) && (spi_type != HAL_SPI_TYPE_SLAVE)) {
        goto err;
    }

    irq_handler = NULL;
    spi->spi_type  = spi_type;
    if (spi_num == 0) {
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
        spi->irq_num = SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQn;
        irq_handler = nrf52_spi0_irq_handler;
        if (spi_type == HAL_SPI_TYPE_MASTER) {
#if MYNEWT_VAL(SPI_0_MASTER)
            spi->nhs_spi.spim = NRF_SPIM0;
#else
            assert(0);
#endif
        } else {
#if MYNEWT_VAL(SPI_0_SLAVE)
            spi->nhs_spi.spis = NRF_SPIS0;
#else
            assert(0);
#endif
        }
#else
        goto err;
#endif
    } else if (spi_num == 1) {
#if MYNEWT_VAL(SPI_1_MASTER)  || MYNEWT_VAL(SPI_1_SLAVE)
        spi->irq_num = SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQn;
        irq_handler = nrf52_spi1_irq_handler;
        if (spi_type == HAL_SPI_TYPE_MASTER) {
#if MYNEWT_VAL(SPI_1_MASTER)
            spi->nhs_spi.spim = NRF_SPIM1;
#else
            assert(0);
#endif
        } else {
#if MYNEWT_VAL(SPI_1_SLAVE)
            spi->nhs_spi.spis = NRF_SPIS1;
#else
            assert(0);
#endif
        }
#else
        goto err;
#endif
    } else if (spi_num == 2) {
#if MYNEWT_VAL(SPI_2_MASTER)  || MYNEWT_VAL(SPI_2_SLAVE)
        spi->irq_num = SPIM2_SPIS2_SPI2_IRQn;
        irq_handler = nrf52_spi2_irq_handler;
        if (spi_type == HAL_SPI_TYPE_MASTER) {
#if MYNEWT_VAL(SPI_2_MASTER)
            spi->nhs_spi.spim = NRF_SPIM2;
#else
            assert(0);
#endif
        } else {
#if MYNEWT_VAL(SPI_2_SLAVE)
            spi->nhs_spi.spis = NRF_SPIS2;
#else
            assert(0);
#endif
        }
#else
        goto err;
#endif
    } else {
        goto err;
    }

    if (spi_type == HAL_SPI_TYPE_MASTER) {
        rc = hal_spi_init_master(spi, (struct nrf52_hal_spi_cfg *)cfg,
                                 irq_handler);
    } else {
        rc = hal_spi_init_slave(spi, (struct nrf52_hal_spi_cfg *)cfg,
                                irq_handler);
    }

err:
    return (rc);
}

/**
 * Configure the spi. Must be called after the spi is initialized (after
 * hal_spi_init is called) and when the spi is disabled (user must call
 * hal_spi_disable if the spi has been enabled through hal_spi_enable prior
 * to calling this function). Can also be used to reconfigure an initialized
 * SPI (assuming it is disabled as described previously).
 *
 * @param spi_num The number of the SPI to configure.
 * @param psettings The settings to configure this SPI with
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_config(int spi_num, struct hal_spi_settings *settings)
{
    int rc;
    struct nrf52_hal_spi *spi;
    NRF_SPIM_Type *spim;

    NRF52_HAL_SPI_RESOLVE(spi_num, spi);

    /*
     * This looks odd, but the ENABLE register is in the same location for
     * SPIM, SPI and SPIS
     */
    spim = spi->nhs_spi.spim;
    if (spim->ENABLE != 0) {
        return -1;
    }

    if (spi->spi_type  == HAL_SPI_TYPE_MASTER) {
        rc = hal_spi_config_master(spi, settings);
    } else {
        rc = hal_spi_config_slave(spi, settings);
    }

err:
    return (rc);
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
    int rc;
    NRF_SPIS_Type *spis;
    NRF_SPI_Type *nrf_spi;
    struct nrf52_hal_spi *spi;

    NRF52_HAL_SPI_RESOLVE(spi_num, spi);

    if (spi->spi_type  == HAL_SPI_TYPE_MASTER) {
        /* For now, enable this in normal SPI mode (not spim) */
        nrf_spi = (NRF_SPI_Type *)spi->nhs_spi.spim;
        nrf_spi->ENABLE = (SPI_ENABLE_ENABLE_Enabled << SPI_ENABLE_ENABLE_Pos);
    } else {
        if (spi->txrx_cb_func == NULL) {
            rc = EINVAL;
            goto err;
        }

        spis = spi->nhs_spi.spis;
        spis->EVENTS_END = 0;
        spis->EVENTS_ACQUIRED = 0;
        spis->INTENSET = SPIS_INTENSET_END_Msk | SPIS_INTENSET_ACQUIRED_Msk;
        spis->ENABLE = (SPIS_ENABLE_ENABLE_Enabled << SPIS_ENABLE_ENABLE_Pos);
    }
    rc = 0;

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
    int rc;
    NRF_SPIS_Type *spis;
    NRF_SPIM_Type *spim;
    struct nrf52_hal_spi *spi;

    NRF52_HAL_SPI_RESOLVE(spi_num, spi);

    if (spi->spi_type  == HAL_SPI_TYPE_MASTER) {
        spim = spi->nhs_spi.spim;
        spim->INTENCLR = NRF_SPI_IRQ_DISABLE_ALL;

        if (spi->spi_xfr_flag) {
            hal_spi_stop_transfer(spim);
            spi->spi_xfr_flag = 0;
        }
        spim->ENABLE = 0;
    } else {
        spis = spi->nhs_spi.spis;
        spis->INTENCLR = NRF_SPI_IRQ_DISABLE_ALL;
        spis->EVENTS_END = 0;
        spis->EVENTS_ACQUIRED = 0;
        spis->ENABLE = 0;
        spi->slave_state = HAL_SPI_SLAVE_STATE_IDLE;
    }

    spi->nhs_txbuf = NULL;
    spi->nhs_rxbuf = NULL;
    spi->nhs_buflen = 0;
    spi->nhs_bytes_txd = 0;

    rc = 0;

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
    uint16_t retval;
    NRF_SPI_Type *spi;
    struct nrf52_hal_spi *hal_spi;

    NRF52_HAL_SPI_RESOLVE(spi_num, hal_spi);

    if (hal_spi->spi_type  == HAL_SPI_TYPE_MASTER) {
        spi = (NRF_SPI_Type *)hal_spi->nhs_spi.spim;
        spi->EVENTS_READY = 0;
        spi->TXD = (uint8_t)val;
        while (!spi->EVENTS_READY) {}
        spi->EVENTS_READY = 0;
        retval = (uint16_t)spi->RXD;
    } else {
        retval = 0xFFFF;
    }

    return retval;

err:
    return rc;
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
    NRF_SPIM_Type *spim;
    struct nrf52_hal_spi *spi;

    NRF52_HAL_SPI_RESOLVE(spi_num, spi);

    /*
     * This looks odd, but the ENABLE register is in the same location for
     * SPIM, SPI and SPIS
     */
    spim = spi->nhs_spi.spim;
    if (spim->ENABLE != 0) {
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
    int i;
    int rc;
    int txcnt;
    uint32_t enabled;
    uint8_t *txd, *rxd;
    uint8_t rxval;
    NRF_SPI_Type *spi;
    NRF_SPIM_Type *spim;
    struct nrf52_hal_spi *hal_spi;

    rc = EINVAL;
    if (!len) {
        goto err;
    }

    NRF52_HAL_SPI_RESOLVE(spi_num, hal_spi);

    if (hal_spi->spi_type  == HAL_SPI_TYPE_MASTER) {
        /* Must have a txbuf for master! */
        if (txbuf == NULL) {
            goto err;
        }

        /*
         * If SPIM is enabled, we want to stop, disable, then enable
         * the legacy SPI interface.
         */
        spim = hal_spi->nhs_spi.spim;
        enabled = spim->ENABLE;
        if (enabled == SPIM_ENABLE_ENABLE_Enabled) {
            spim->INTENCLR = NRF_SPI_IRQ_DISABLE_ALL;
            hal_spi_stop_transfer(spim);
            spim->ENABLE = 0;
            enabled = 0;
        }

        spi = (NRF_SPI_Type *)spim;
        if (enabled == 0) {
            spi->ENABLE = (SPI_ENABLE_ENABLE_Enabled << SPI_ENABLE_ENABLE_Pos);
        }

        while (spi->EVENTS_READY) {
            rxval = (uint8_t)spi->RXD;
            spi->EVENTS_READY = 0;
        }
        txd = (uint8_t *)txbuf;
        spi->TXD = *txd;

        txcnt = len - 1;
        rxd = (uint8_t *)rxbuf;
        for (i = 0; i < len; ++i) {
            if (txcnt) {
                ++txd;
                spi->TXD = *txd;
                --txcnt;
            }
            while (!spi->EVENTS_READY) {}
            spi->EVENTS_READY = 0;
            rxval = (uint8_t)spi->RXD;
            if (rxbuf) {
                *rxd = rxval;
                ++rxd;
            }
        }
        return 0;
    }

err:
    return rc;
}

/**
 * Non-blocking interface to send a buffer and store received values. Can be
 * used for both master and slave SPI types. The user must configure the
 * callback (using hal_spi_set_txrx_cb); the txrx callback is executed at
 * interrupt context when the buffer is sent.
 *
 * The transmit and receive buffers are either arrays of 8-bit (uint8_t)
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
 *             The txbuf parameter cannot be NULL
 *     SLAVE: Slave "preloads" the data to be sent to the master (values
 *            stored in txbuf) and places received data from master in rxbuf
 *            (if not NULL). The txrx callback occurs when len values are
 *            transferred or master de-asserts chip select. If txbuf is NULL,
 *            the slave transfers its default byte. Both rxbuf and txbuf cannot
 *            be NULL.
 *
 * @param spi_num   SPI interface to use
 * @param txbuf     Pointer to buffer where values to transmit are stored.
 * @param rxbuf     Pointer to buffer to store values received from peer.
 * @param cnt       Number of 8-bit or 16-bit values to be transferred.
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_txrx_noblock(int spi_num, void *txbuf, void *rxbuf, int len)
{
    int rc;
    NRF_SPIM_Type *spim;
    struct nrf52_hal_spi *spi;

    rc = EINVAL;
    NRF52_HAL_SPI_RESOLVE(spi_num, spi);

    if ((spi->txrx_cb_func == NULL) || (len == 0)) {
        goto err;
    }

    if (spi->spi_type  == HAL_SPI_TYPE_MASTER) {
        /* Must have a txbuf for master! */
        if (txbuf == NULL) {
            goto err;
        }

        /* Not allowed if transfer in progress */
        if (spi->spi_xfr_flag) {
            rc = -1;
            goto err;
        }
        spim = spi->nhs_spi.spim;
        spim->INTENCLR = SPIM_INTENCLR_END_Msk;
        spi->spi_xfr_flag = 1;

        /* Must be enabled for SPIM as opposed to SPI */
        if (spim->ENABLE != SPIM_ENABLE_ENABLE_Enabled) {
            spim->ENABLE = 0;
            spim->ENABLE = (SPIM_ENABLE_ENABLE_Enabled << SPIM_ENABLE_ENABLE_Pos);
        }

        /* Set internal data structure information */
        spi->nhs_bytes_txd = 0;
        spi->nhs_buflen = len;
        spi->nhs_txbuf = txbuf;

        /* Set chip registers */
        spim->TXD.PTR = (uint32_t)txbuf;
        spim->TXD.MAXCNT = len;

        /* If no rxbuf, we need to set rxbuf and maxcnt to 1 */
        spi->nhs_rxbuf = rxbuf;
        if (rxbuf == NULL) {
            spim->RXD.PTR = (uint32_t)&spi->dummy_rx;
            spim->RXD.MAXCNT = 1;
        } else {
            spim->RXD.PTR = (uint32_t)rxbuf;
            spim->RXD.MAXCNT = len;
        }

        spim->EVENTS_END = 0;
        spim->EVENTS_STOPPED = 0;
        spim->TASKS_START = 1;
        spim->INTENSET = SPIM_INTENSET_END_Msk;
    } else {
        /* Must have txbuf or rxbuf */
        if ((txbuf == NULL) && (rxbuf == NULL)) {
            goto err;
        }

        /* XXX: what to do here? */
        if (len > 255) {
            goto err;
        }

        /*
         * Ready the slave for a transfer. Do not allow this to be called
         * if the slave has already been readied or is requesting the
         * semaphore
         */
        if (spi->slave_state != HAL_SPI_SLAVE_STATE_IDLE) {
            rc = -1;
            goto err;
        }

        spi->nhs_rxbuf = rxbuf;
        spi->nhs_txbuf = txbuf;
        spi->nhs_buflen = len;
        spi->slave_state = HAL_SPI_SLAVE_STATE_ACQ_SEM;
        spi->nhs_spi.spis->TASKS_ACQUIRE = 1;
    }
    return 0;

err:
    return rc;
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
    NRF_SPIS_Type *spis;
    struct nrf52_hal_spi *spi;

    NRF52_HAL_SPI_RESOLVE(spi_num, spi);
    if (spi->spi_type  == HAL_SPI_TYPE_SLAVE) {
        spis = spi->nhs_spi.spis;
        spis->DEF = (uint8_t)val;
        spis->ORC = (uint8_t)val;
        rc = 0;
    } else {
        rc = EINVAL;
    }

err:
    return rc;
}

/**
 * This aborts the current transfer but keeps the spi enabled.
 *
 * @param spi_num   SPI interface on which transfer should be aborted.
 *
 * @return int 0 on success, non-zero error code on failure.
 *
 * NOTE: does not return an error if no transfer was in progress.
 */
int
hal_spi_abort(int spi_num)
{
    int rc;
    NRF_SPIM_Type *spim;
    struct nrf52_hal_spi *spi;

    NRF52_HAL_SPI_RESOLVE(spi_num, spi);

    rc = 0;
    if (spi->spi_type  == HAL_SPI_TYPE_MASTER) {
        spim = spi->nhs_spi.spim;
        if (spi->spi_xfr_flag) {
            spim->INTENCLR = NRF_SPI_IRQ_DISABLE_ALL;
            hal_spi_stop_transfer(spim);
            spi->spi_xfr_flag = 0;
            spim->INTENSET = SPIM_INTENSET_END_Msk;
        }
    } else {
        /* Only way I can see doing this is to disable, then re-enable */
        hal_spi_disable(spi_num);
        hal_spi_enable(spi_num);
    }

err:
    return rc;
}
