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
#include <assert.h>
#include <hal/hal_spi.h>
#include <bsp/cmsis_nvic.h>
#include <nrf.h>
#include <nrf_spi.h>
#include <nrf_spis.h>
#include <nrf_drv_spi.h>
#include <nrf_drv_spis.h>
#include <nrf_drv_common.h>
#include <app_util_platform.h>

/* XXX:
 * 1) what about stats?
 * 2) Dealing with errors (OVERFLOW, OVERREAD)
 * 3) Dont think I need dummy_rx as I can set master RX maxcnt to zero.
 */

/* The maximum number of SPI interfaces we will allow */
#define NRF51_HAL_SPI_MAX (2)

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

struct nrf51_hal_spi {
    uint8_t spi_xfr_flag;   /* Master only */
    uint8_t slave_state;    /* Slave only */
    uint16_t nhs_buflen;
    uint16_t nhs_rxd_bytes;
    uint16_t nhs_txd_bytes;
    struct hal_spi_settings spi_cfg; /* Slave and master */
    union {
        nrf_drv_spi_t   spim;
        nrf_drv_spis_t  spis;
    } nhs_spi;

    uint8_t *nhs_txbuf;
    uint8_t *nhs_rxbuf;
};

#if SPI0_ENABLED
struct nrf51_hal_spi nrf51_hal_spi0;
#endif
#if SPI1_ENABLED || SPIS1_ENABLED
struct nrf51_hal_spi nrf51_hal_spi1;
#endif

struct nrf51_hal_spi *nrf51_hal_spis[NRF51_HAL_SPI_MAX] = {
#if SPI0_ENABLED
    &nrf51_hal_spi0,
#else
    NULL,
#endif
#if SPI1_ENABLED || SPIS1_ENABLED
    &nrf51_hal_spi1
#else
    NULL
#endif
};

#if SPI0_ENABLED
nrf_drv_spi_t inst_spi0_m = NRF_DRV_SPI_INSTANCE(0);
#endif
#if SPI1_ENABLED
nrf_drv_spi_t inst_spi1_m = NRF_DRV_SPI_INSTANCE(1);
#endif
#if SPIS1_ENABLED
nrf_drv_spis_t inst_spi1_s = NRF_DRV_SPIS_INSTANCE(1);
#endif

#define NRF51_HAL_SPI_RESOLVE(__n, __v)         \
    if ((__n) >= NRF51_HAL_SPI_MAX) {           \
        rc = EINVAL;                            \
        goto err;                               \
    }                                           \
    (__v) = nrf51_hal_spis[(__n)];              \
    if ((__v) == NULL) {                        \
        rc = EINVAL;                            \
        goto err;                               \
    }

#if (SPI0_ENABLED || SPI1_ENABLED)
static void
nrf51_irqm_handler(struct nrf51_hal_spi *spi)
{
    NRF_SPI_Type *p_spi;

    p_spi = (NRF_SPI_Type *)spi->nhs_spi.spim.p_registers;
    if (nrf_spi_event_check(p_spi, NRF_SPI_EVENT_READY)) {
        nrf_spi_event_clear(p_spi, NRF_SPI_EVENT_READY);

        if (spi->spi_xfr_flag == 0) {
            return;
        }

        if (spi->nhs_rxbuf) {
            spi->nhs_rxbuf[spi->nhs_rxd_bytes] = nrf_spi_rxd_get(p_spi);
        }
        ++spi->nhs_rxd_bytes;
        if (spi->nhs_rxd_bytes == spi->nhs_buflen) {
            if (spi->spi_cfg.txrx_cb_func) {
                spi->spi_cfg.txrx_cb_func(spi->spi_cfg.txrx_cb_arg,
                                          spi->nhs_buflen);
            }
            spi->spi_xfr_flag = 0;
        }
        if (spi->nhs_txd_bytes != spi->nhs_buflen) {
            nrf_spi_txd_set(p_spi, spi->nhs_txbuf[spi->nhs_txd_bytes]);
            ++spi->nhs_txd_bytes;
        }
    }
}
#endif

#if (SPIS1_ENABLED)
static void
nrf51_irqs_handler(struct nrf51_hal_spi *spi)
{
    uint8_t xfr_len;
    NRF_SPIS_Type *p_spis;

    p_spis = spi->nhs_spi.spis.p_reg;

    /* Semaphore acquired event */
    if (nrf_spis_event_check(p_spis, NRF_SPIS_EVENT_ACQUIRED)) {
        nrf_spis_event_clear(p_spis, NRF_SPIS_EVENT_ACQUIRED);

        if (spi->slave_state == HAL_SPI_SLAVE_STATE_ACQ_SEM) {
            if (spi->nhs_txbuf == NULL) {
                nrf_spis_tx_buffer_set(p_spis, 0, 0);
            } else {
                nrf_spis_tx_buffer_set(p_spis, spi->nhs_txbuf, spi->nhs_buflen);
            }

            if (spi->nhs_rxbuf == NULL) {
                nrf_spis_rx_buffer_set(p_spis, 0, 0);
            } else {
                nrf_spis_rx_buffer_set(p_spis, spi->nhs_rxbuf, spi->nhs_buflen);
            }
            nrf_spis_task_trigger(p_spis, NRF_SPIS_TASK_RELEASE);
            spi->slave_state = HAL_SPI_SLAVE_STATE_READY;
        }
    }

    /* SPI transaction complete */
    if (nrf_spis_event_check(p_spis, NRF_SPIS_EVENT_END)) {
        nrf_spis_event_clear(p_spis, NRF_SPIS_EVENT_END);
        if (spi->slave_state == HAL_SPI_SLAVE_STATE_READY) {
            if (spi->spi_cfg.txrx_cb_func) {
                /* Get transfer length */
                if (spi->nhs_txbuf == NULL) {
                    xfr_len = nrf_spis_rx_amount_get(p_spis);
                } else {
                    xfr_len = nrf_spis_tx_amount_get(p_spis);
                }
                spi->spi_cfg.txrx_cb_func(spi->spi_cfg.txrx_cb_arg, xfr_len);
            }
            spi->slave_state = HAL_SPI_SLAVE_STATE_IDLE;
        }
    }

}
#endif

/* Interrupt handlers for SPI ports */
#if SPI0_ENABLED
void
nrf51_spi0_irq_handler(void)
{
    if (nrf51_hal_spi0.spi_cfg.spi_type == HAL_SPI_TYPE_MASTER) {
        nrf51_irqm_handler(&nrf51_hal_spi0);
    } else {
        assert(0);
    }
}
#endif

#if SPI1_ENABLED || SPIS1_ENABLED
void
nrf51_spi1_irq_handler(void)
{
    if (nrf51_hal_spi1.spi_cfg.spi_type == HAL_SPI_TYPE_MASTER) {
#if SPI1_ENABLED
        nrf51_irqm_handler(&nrf51_hal_spi1);
#else
        assert(0);
#endif
    } else {
#if SPIS1_ENABLED
        nrf51_irqs_handler(&nrf51_hal_spi1);
#else
        assert(0);
#endif
    }
}
#endif

static int
hal_spi_config_master(struct nrf51_hal_spi *spi,
                      struct hal_spi_settings *settings)
{
    int rc;
    NRF_SPI_Type *p_spi;
    nrf_spi_frequency_t frequency;
    nrf_spi_mode_t spi_mode;
    nrf_spi_bit_order_t bit_order;

    p_spi = (NRF_SPI_Type *)spi->nhs_spi.spim.p_registers;
    memcpy(&spi->spi_cfg, settings, sizeof(*settings));

    /* Only 8-bit word sizes supported. */
    switch (settings->word_size) {
        case HAL_SPI_WORD_SIZE_8BIT:
            break;
        default:
            rc = EINVAL;
            goto err;
    }

    switch (settings->data_mode) {
        case HAL_SPI_MODE0:
            spi_mode = NRF_SPI_MODE_0;
            break;
        case HAL_SPI_MODE1:
            spi_mode = NRF_SPI_MODE_1;
            break;
        case HAL_SPI_MODE2:
            spi_mode = NRF_SPI_MODE_2;
            break;
        case HAL_SPI_MODE3:
            spi_mode = NRF_SPI_MODE_3;
            break;
        default:
            spi_mode = 0;
            break;
    }

    switch (settings->data_order) {
        case HAL_SPI_MSB_FIRST:
            bit_order = NRF_SPI_BIT_ORDER_MSB_FIRST;
            break;
        case HAL_SPI_LSB_FIRST:
            bit_order = NRF_SPI_BIT_ORDER_LSB_FIRST;
            break;
        default:
            bit_order = 0;
            break;
    }
    nrf_spi_configure(p_spi, spi_mode, bit_order);

    switch (settings->baudrate) {
        case 125:
            frequency = NRF_SPI_FREQ_125K;
            break;
        case 250:
            frequency = NRF_SPI_FREQ_250K;
            break;
        case 500:
            frequency = NRF_SPI_FREQ_500K;
            break;
        case 1000:
            frequency = NRF_SPI_FREQ_1M;
            break;
        case 2000:
            frequency = NRF_SPI_FREQ_2M;
            break;
        case 4000:
            frequency = NRF_SPI_FREQ_4M;
            break;
        case 8000:
            frequency = NRF_SPI_FREQ_8M;
            break;
        default:
            rc = EINVAL;
            goto err;
    }
    nrf_spi_frequency_set(p_spi, frequency);

    return (0);
err:
    return (rc);
}

static int
hal_spi_config_slave(struct nrf51_hal_spi *spi,
                     struct hal_spi_settings *settings)
{
    int rc;
    NRF_SPIS_Type *p_spis;
    nrf_spis_mode_t spi_mode;
    nrf_spis_bit_order_t bit_order;

    p_spis = spi->nhs_spi.spis.p_reg;

    spi_mode = 0;
    switch (settings->data_mode) {
        case HAL_SPI_MODE0:
            spi_mode = NRF_SPIS_MODE_0;
            break;
        case HAL_SPI_MODE1:
            spi_mode = NRF_SPIS_MODE_1;
            break;
        case HAL_SPI_MODE2:
            spi_mode = NRF_SPIS_MODE_2;
            break;
        case HAL_SPI_MODE3:
            spi_mode = NRF_SPIS_MODE_3;
            break;
    }

    bit_order = 0;
    switch (settings->data_order) {
        case HAL_SPI_MSB_FIRST:
            bit_order = NRF_SPIS_BIT_ORDER_MSB_FIRST;
            break;
        case HAL_SPI_LSB_FIRST:
            bit_order = NRF_SPIS_BIT_ORDER_LSB_FIRST;
            break;
    }
    nrf_spis_configure(p_spis, spi_mode, bit_order);

    /* Only 8-bit word sizes supported. */
    switch (settings->word_size) {
        case HAL_SPI_WORD_SIZE_8BIT:
            break;
        default:
            rc = EINVAL;
            goto err;
    }
    return (0);

err:
    return (rc);
}

static int
hal_spi_init_master(nrf_drv_spi_t *p_instance,
                    nrf_drv_spi_config_t *p_config,
                    nrf_drv_irq_handler_t handler)
{
    uint32_t mosi_pin;
    uint32_t miso_pin;
    NRF_SPI_Type *p_spi;

#if PERIPHERAL_RESOURCE_SHARING_ENABLED
    if (nrf_drv_common_per_res_acquire(p_instance->p_registers,
                                       handler) != NRF_SUCCESS) {
        return NRF_ERROR_BUSY;
    }
#endif

    /* Configure pins used by the peripheral:
       - SCK - output with initial value corresponding with the SPI mode used:
       0 - for modes 0 and 1 (CPOL = 0), 1 - for modes 2 and 3 (CPOL = 1);
       according to the reference manual guidelines this pin and its input
       buffer must always be connected for the SPI to work.
    */
    if (p_config->mode <= NRF_DRV_SPI_MODE_1) {
        nrf_gpio_pin_clear(p_config->sck_pin);
    } else {
        nrf_gpio_pin_set(p_config->sck_pin);
    }

    NRF_GPIO->PIN_CNF[p_config->sck_pin] =
        (GPIO_PIN_CNF_DIR_Output        << GPIO_PIN_CNF_DIR_Pos)
      | (GPIO_PIN_CNF_INPUT_Connect     << GPIO_PIN_CNF_INPUT_Pos)
      | (GPIO_PIN_CNF_PULL_Disabled     << GPIO_PIN_CNF_PULL_Pos)
      | (GPIO_PIN_CNF_DRIVE_S0S1        << GPIO_PIN_CNF_DRIVE_Pos)
      | (GPIO_PIN_CNF_SENSE_Disabled    << GPIO_PIN_CNF_SENSE_Pos);

    /* - MOSI (optional) - output with initial value 0 */
    if (p_config->mosi_pin != NRF_DRV_SPI_PIN_NOT_USED) {
        mosi_pin = p_config->mosi_pin;
        nrf_gpio_pin_clear(mosi_pin);
        nrf_gpio_cfg_output(mosi_pin);
    } else {
        mosi_pin = NRF_SPI_PIN_NOT_CONNECTED;
    }

    /* - MISO (optional) - input*/
    if (p_config->miso_pin != NRF_DRV_SPI_PIN_NOT_USED) {
        miso_pin = p_config->miso_pin;
        nrf_gpio_cfg_input(miso_pin, NRF_GPIO_PIN_NOPULL);
    } else {
        miso_pin = NRF_SPI_PIN_NOT_CONNECTED;
    }
    p_spi = (NRF_SPI_Type *)p_instance->p_registers;
    nrf_spi_pins_set(p_spi, p_config->sck_pin, mosi_pin, miso_pin);
    nrf_spi_frequency_set(p_spi, (nrf_spi_frequency_t)p_config->frequency);
    nrf_spi_configure(p_spi ,(nrf_spi_mode_t)p_config->mode,
                      (nrf_spi_bit_order_t)p_config->bit_order);
    nrf_spi_int_disable(p_spi, NRF_SPI_INT_READY_MASK);
    NVIC_SetVector(p_instance->irq, (uint32_t)handler);
    nrf_drv_common_irq_enable(p_instance->irq, p_config->irq_priority);

    return NRF_SUCCESS;
}

static int
hal_spi_init_slave(nrf_drv_spis_t *p_instance,
                   nrf_drv_spis_config_t *p_config,
                   nrf_drv_irq_handler_t handler)
{
    uint32_t mosi_pin;
    uint32_t miso_pin;
    NRF_SPIS_Type *p_spis;

#if PERIPHERAL_RESOURCE_SHARING_ENABLED
    if (nrf_drv_common_per_res_acquire(p_instance->p_reg,
                                       handler) != NRF_SUCCESS) {
        return NRF_ERROR_BUSY;
    }
#endif

    if ((uint32_t)p_config->mode > (uint32_t)NRF_DRV_SPIS_MODE_3) {
        return NRF_ERROR_INVALID_PARAM;
    }

    if (p_config->miso_pin != NRF_DRV_SPIS_PIN_NOT_USED) {
        nrf_gpio_cfg(p_config->miso_pin,
                    NRF_GPIO_PIN_DIR_INPUT,
                    NRF_GPIO_PIN_INPUT_CONNECT,
                    NRF_GPIO_PIN_NOPULL,
                    p_config->miso_drive,
                    NRF_GPIO_PIN_NOSENSE);
        miso_pin = p_config->miso_pin;
    } else {
        miso_pin = NRF_SPIS_PIN_NOT_CONNECTED;
    }

    if (p_config->mosi_pin != NRF_DRV_SPIS_PIN_NOT_USED) {
        nrf_gpio_cfg(p_config->mosi_pin,
                     NRF_GPIO_PIN_DIR_INPUT,
                     NRF_GPIO_PIN_INPUT_CONNECT,
                     NRF_GPIO_PIN_NOPULL,
                     NRF_GPIO_PIN_S0S1,
                     NRF_GPIO_PIN_NOSENSE);
        mosi_pin = p_config->mosi_pin;
    } else {
        mosi_pin = NRF_SPIS_PIN_NOT_CONNECTED;
    }

    nrf_gpio_cfg(p_config->csn_pin,
                 NRF_GPIO_PIN_DIR_INPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 p_config->csn_pullup,
                 NRF_GPIO_PIN_S0S1,
                 NRF_GPIO_PIN_NOSENSE);

    nrf_gpio_cfg(p_config->sck_pin,
                 NRF_GPIO_PIN_DIR_INPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_S0S1,
                 NRF_GPIO_PIN_NOSENSE);

    p_spis = (NRF_SPIS_Type *)p_instance->p_reg;
    nrf_spis_pins_set(p_spis, p_config->sck_pin, mosi_pin, miso_pin, p_config->csn_pin);
    nrf_spis_configure(p_spis, (nrf_spis_mode_t) p_config->mode,
                               (nrf_spis_bit_order_t) p_config->bit_order);

    /* Configure DEF and ORC characters. */
    nrf_spis_def_set(p_spis, p_config->def);
    nrf_spis_orc_set(p_spis, p_config->orc);

    /* Disable interrupt and clear any interrupt events */
    nrf_spis_int_disable(p_spis, NRF_SPIS_INT_ACQUIRED_MASK | NRF_SPIS_INT_END_MASK);
    nrf_spis_event_clear(p_spis, NRF_SPIS_EVENT_END);
    nrf_spis_event_clear(p_spis, NRF_SPIS_EVENT_ACQUIRED);

    /* Enable END_ACQUIRE shortcut. */
    nrf_spis_shorts_enable(p_spis, NRF_SPIS_SHORT_END_ACQUIRE);
    NVIC_SetVector(p_instance->irq, (uint32_t)handler);
    nrf_drv_common_irq_enable(p_instance->irq, p_config->irq_priority);

    return NRF_SUCCESS;
}

static void
hal_spi_master_send_first(NRF_SPI_Type *p_spi, uint8_t txval)
{
    while (nrf_spi_event_check(p_spi, NRF_SPI_EVENT_READY)) {
        (void)nrf_spi_rxd_get(p_spi);
        nrf_spi_event_clear(p_spi, NRF_SPI_EVENT_READY);
    }
    nrf_spi_txd_set(p_spi, txval);
}

/**
 * Initialize the SPI interface
 *
 *
 * @param spi_num
 * @param cfg
 * @param spi_type
 *
 * @return int
 */
int
hal_spi_init(int spi_num, void *cfg, uint8_t spi_type)
{
    int rc;
    struct nrf51_hal_spi *spi;
    nrf_drv_irq_handler_t irq_handler;

    NRF51_HAL_SPI_RESOLVE(spi_num, spi);

    /* Check for valid arguments */
    rc = EINVAL;
    if (cfg == NULL) {
        goto err;
    }

    if ((spi_type != HAL_SPI_TYPE_MASTER) && (spi_type != HAL_SPI_TYPE_SLAVE)) {
        goto err;
    }

    irq_handler = NULL;
    spi->spi_cfg.spi_type  = spi_type;
    if (spi_num == 0) {
#if SPI0_ENABLED
        irq_handler = nrf51_spi0_irq_handler;
        if (spi_type == HAL_SPI_TYPE_MASTER) {
            memcpy(&spi->nhs_spi.spim, &inst_spi0_m, sizeof(inst_spi0_m));
        } else {
            assert(0);
        }
#else
        goto err;
#endif
    } else if (spi_num == 1) {
#if SPI1_ENABLED || SPIS1_ENABLED
        irq_handler = nrf51_spi1_irq_handler;
        if (spi_type == HAL_SPI_TYPE_MASTER) {
#if SPI1_ENABLED
            memcpy(&spi->nhs_spi.spim, &inst_spi1_m, sizeof(inst_spi0_m));
#else
            assert(0);
#endif
        } else {
#if SPIS1_ENABLED
            memcpy(&spi->nhs_spi.spis, &inst_spi1_s, sizeof(inst_spi1_s));
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
        rc = hal_spi_init_master(&spi->nhs_spi.spim,
                                 (nrf_drv_spi_config_t *)cfg,
                                 irq_handler);


    } else {
        rc = hal_spi_init_slave(&spi->nhs_spi.spis,
                                (nrf_drv_spis_config_t *)cfg,
                                irq_handler);
    }

err:
    return (rc);
}

int
hal_spi_config(int spi_num, struct hal_spi_settings *settings)
{
    int rc;
    struct nrf51_hal_spi *spi;

    NRF51_HAL_SPI_RESOLVE(spi_num, spi);

    spi->spi_cfg.txrx_cb_func = settings->txrx_cb_func;
    spi->spi_cfg.txrx_cb_arg = settings->txrx_cb_arg;

    if (spi->spi_cfg.spi_type  == HAL_SPI_TYPE_MASTER) {
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
    NRF_SPIS_Type *p_spis;
    NRF_SPI_Type *p_spi;
    struct nrf51_hal_spi *spi;

    NRF51_HAL_SPI_RESOLVE(spi_num, spi);

    if (spi->spi_cfg.spi_type  == HAL_SPI_TYPE_MASTER) {
        p_spi = (NRF_SPI_Type *)spi->nhs_spi.spim.p_registers;
        /* We need to enable this in blocking or non-blocking mode */
        if (spi->spi_cfg.txrx_cb_func) {
            nrf_spi_event_clear(p_spi, NRF_SPI_INT_READY_MASK);
            nrf_spi_int_enable(p_spi, NRF_SPI_INT_READY_MASK);
        }
        nrf_spi_enable(p_spi);
    } else {
        if (spi->spi_cfg.txrx_cb_func == NULL) {
            rc = EINVAL;
            goto err;
        }

        p_spis = spi->nhs_spi.spis.p_reg;
        nrf_spis_event_clear(p_spis, NRF_SPIS_EVENT_END);
        nrf_spis_event_clear(p_spis, NRF_SPIS_EVENT_ACQUIRED);
        nrf_spis_int_enable(p_spis, NRF_SPIS_INT_ACQUIRED_MASK | NRF_SPIS_INT_END_MASK);
        nrf_spis_enable(p_spis);
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
    NRF_SPIS_Type *p_spis;
    NRF_SPI_Type *p_spi;
    struct nrf51_hal_spi *spi;

    NRF51_HAL_SPI_RESOLVE(spi_num, spi);

    if (spi->spi_cfg.spi_type  == HAL_SPI_TYPE_MASTER) {
        p_spi = (NRF_SPI_Type *)spi->nhs_spi.spim.p_registers;
        nrf_spi_int_disable(p_spi, NRF_SPI_INT_READY_MASK);
        spi->spi_xfr_flag = 0;
        nrf_spi_disable(p_spi);
    } else {
        p_spis = spi->nhs_spi.spis.p_reg;
        nrf_spis_int_disable(p_spis, NRF_SPI_IRQ_DISABLE_ALL);
        nrf_spis_event_clear(p_spis, NRF_SPIS_EVENT_END);
        nrf_spis_event_clear(p_spis, NRF_SPIS_EVENT_ACQUIRED);
        nrf_spis_disable(p_spis);
        spi->slave_state = HAL_SPI_SLAVE_STATE_IDLE;
        spi->nhs_txbuf = NULL;
        spi->nhs_rxbuf = NULL;
    }
    rc = 0;

err:
    return rc;
}

/**
 * Blocking call to send a value on the SPI. Returns the value received from the
 * SPI.
 *
 * MASTER: Sends the value and returns the received value from the slave.
 * SLAVE: Sets the value to send to the master when the master transfers a
 *        value. This value will be sent until another call to hal_spi_tx_val()
 *        is made. The return code is ignored for a slave. 
 *
 * @param spi_num
 * @param val
 *
 * @return uint16_t Value received on SPI interface from slave. Returns 0xFFFF
 * if called when SPI configured as a Slave.
 */
uint16_t hal_spi_tx_val(int spi_num, uint16_t val)
{
    int rc;
    uint16_t retval;
    NRF_SPI_Type *p_spi;
    struct nrf51_hal_spi *spi;

    NRF51_HAL_SPI_RESOLVE(spi_num, spi);

    if (spi->spi_cfg.spi_type  == HAL_SPI_TYPE_MASTER) {
        p_spi = (NRF_SPI_Type *) spi->nhs_spi.spim.p_registers;
        nrf_spi_event_clear(p_spi, NRF_SPI_EVENT_READY);
        nrf_spi_txd_set(p_spi, (uint8_t) val);
        while (!nrf_spi_event_check(p_spi, NRF_SPI_EVENT_READY)) {}
        nrf_spi_event_clear(p_spi, NRF_SPI_EVENT_READY);
        retval = (uint16_t)nrf_spi_rxd_get(p_spi);
    } else {
        retval = 0xFFFF;
    }

    return retval;

err:
    return rc;
}

/**
 * Sets the txrx callback (executed at interrupt context) when the
 * buffer is transferred by the master or the slave using the hal_spi_rxtx API.
 * This callback is also called when the SPI is a slave and chip select is
 * de-asserted and there is data available in the receive buffer.
 *
 * If the callback is NULL, the SPI will be in blocking mode; otherwise it is
 * in non-blocking mode.
 *
 * Cannot be called when the SPI is enabled.
 *
 * @param spi_num   SPI interface on which to set callback
 * @param txrx_cb   Pointer to callback function. NULL to set into blocking mode
 * @param arg       Argument passed to callback function.
 *
 * @return int 0 on success, -1 if spi is already enabled.
 */
int
hal_spi_set_txrx_cb(int spi_num, hal_spi_txrx_cb txrx_cb, void *arg)
{
    int rc;
    NRF_SPI_Type *p_spi;
    struct nrf51_hal_spi *spi;

    NRF51_HAL_SPI_RESOLVE(spi_num, spi);

    /*
     * This looks odd, but the ENABLE register is in the same location for
     * SPIM, SPI and SPIS
     */
    p_spi = (NRF_SPI_Type *)spi->nhs_spi.spim.p_registers;
    if (p_spi->ENABLE != 0) {
        rc = -1;
    } else {
        spi->spi_cfg.txrx_cb_func = txrx_cb;
        spi->spi_cfg.txrx_cb_arg = arg;
        rc = 0;
    }

err:
    return rc;
}

/**
 * Send a buffer and also store the received values. This call can be either
 * blocking or non-blocking for the master; it is always non-blocking for slave.
 * In non-blocking mode, the txrx callback is executed at interrupt context when
 * the buffer is sent.
 *     MASTER: master sends all the values in the buffer and stores the
 *             stores the values in the receive buffer if rxbuf is not NULL.
 *             The txbuf parameter cannot be NULL.
 *     SLAVE: Slave preloads the data to be sent to the master (values
 *            stored in txbuf) and places received data from master in rxbuf (if
 *            not NULL).  The txrx callback when len values are transferred or
 *            master de-asserts chip select. If txbuf is NULL, the slave
 *            transfers its default byte. Both rxbuf and txbuf cannot be NULL.
 *
 * @param spi_num   SPI interface to use
 * @param txbuf     Pointer to buffer where values to transmit are stored.
 * @param rxbuf     Pointer to buffer to store values received from peer. Can
 *                  be NULL.
 * @param len       Number of values to be transferred.
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_txrx(int spi_num, void *txbuf, void *rxbuf, int len)
{
    int i;
    int rc;
    int txcnt;
    uint8_t *txd, *rxd;
    uint8_t rxval;
    NRF_SPI_Type *p_spi;
    struct nrf51_hal_spi *spi;

    rc = EINVAL;
    if (!len || (len > 255)) {
        goto err;
    }

    NRF51_HAL_SPI_RESOLVE(spi_num, spi);

    if (spi->spi_cfg.spi_type  == HAL_SPI_TYPE_MASTER) {
        p_spi = (NRF_SPI_Type *) spi->nhs_spi.spim.p_registers;
        if (spi->spi_cfg.txrx_cb_func) {
            /* Must have a txbuf for master! */
            if (txbuf == NULL) {
                goto err;
            }

            /* Not allowed if transfer in progress */
            if (spi->spi_xfr_flag) {
                rc = -1;
                goto err;
            }
            spi->spi_xfr_flag = 1;

            spi->nhs_buflen = (uint16_t)len;
            spi->nhs_txbuf = txbuf;
            spi->nhs_rxbuf = rxbuf;
            spi->nhs_rxd_bytes = 0;
            txd = (uint8_t *)txbuf;
            hal_spi_master_send_first(p_spi, txd[0]);
            spi->nhs_txd_bytes = 1;
            if (len > 1) {
                nrf_spi_txd_set(p_spi, txd[1]);
                ++spi->nhs_txd_bytes;
            }
            nrf_spi_int_enable(p_spi, NRF_SPI_INT_READY_MASK);
        } else {
            /* Blocking spi transfer */
            txd = (uint8_t *)txbuf;
            hal_spi_master_send_first(p_spi, txd[0]);
            txcnt = len - 1;
            rxd = (uint8_t *)rxbuf;
            for (i = 0; i < len; ++i) {
                if (txcnt) {
                    ++txd;
                    nrf_spi_txd_set(p_spi, *txd);
                    --txcnt;
                }
                while (!nrf_spi_event_check(p_spi, NRF_SPI_EVENT_READY)) {}
                nrf_spi_event_clear(p_spi, NRF_SPI_EVENT_READY);
                rxval = nrf_spi_rxd_get(p_spi);
                if (rxbuf) {
                    *rxd = rxval;
                    ++rxd;
                }
            }
        }
    } else {
        /* Must have txbuf and rxbuf */
        if ((txbuf == NULL) && (rxbuf == NULL)) {
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
        nrf_spis_task_trigger(spi->nhs_spi.spis.p_reg, NRF_SPIS_TASK_ACQUIRE);
    }
    return 0;

err:
    return rc;
}

/**
 * Sets the default value transferred by the slave.
 *
 * @param spi_num SPI interface to use
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_slave_set_def_tx_val(int spi_num, uint16_t val)
{
    int rc;
    NRF_SPIS_Type *p_spis;
    struct nrf51_hal_spi *spi;

    NRF51_HAL_SPI_RESOLVE(spi_num, spi);
    if (spi->spi_cfg.spi_type  == HAL_SPI_TYPE_SLAVE) {
        p_spis = spi->nhs_spi.spis.p_reg;
        nrf_spis_def_set(p_spis, (uint8_t) val);
        rc = 0;
    } else {
        rc = EINVAL;
    }

err:
    return rc;
}

/**
 * This aborts the current transfer but keeps the spi enabled. Should only
 * be used when the SPI is in non-blocking mode.
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
    NRF_SPI_Type *p_spi;
    struct nrf51_hal_spi *spi;

    NRF51_HAL_SPI_RESOLVE(spi_num, spi);

    rc = 0;
    if (spi->spi_cfg.txrx_cb_func == NULL) {
        goto err;
    }

    if (spi->spi_cfg.spi_type  == HAL_SPI_TYPE_MASTER) {
        p_spi = (NRF_SPI_Type *)spi->nhs_spi.spim.p_registers;
        if (spi->spi_xfr_flag) {
            nrf_spi_int_disable(p_spi, NRF_SPI_INT_READY_MASK);
            nrf_spi_disable(p_spi);
            nrf_spi_event_clear(p_spi, NRF_SPI_EVENT_READY);
            spi->spi_xfr_flag = 0;
            nrf_spi_int_enable(p_spi, NRF_SPI_INT_READY_MASK);
        }
    } else {
        /* Only way I can see doing this is to disable, then re-enable */
        hal_spi_disable(spi_num);
        hal_spi_enable(spi_num);
    }

err:
    return rc;
}
