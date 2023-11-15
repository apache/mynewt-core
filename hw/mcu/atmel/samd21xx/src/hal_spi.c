/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <errno.h>

#include "syscfg/syscfg.h"
#include "hal/hal_spi.h"
#include "compiler.h"
#include "port.h"
#include "mcu/hal_spi.h"
#include "mcu/samd21.h"
#include "sercom.h"
#include "spi.h"
#include "spi_interrupt.h"
#include "samd21_priv.h"

#define SAMD21_SPI_FLAG_MASTER      (0x1)
#define SAMD21_SPI_FLAG_ENABLED     (0x2)
#define SAMD21_SPI_FLAG_XFER        (0x4)

struct samd21_hal_spi {
    struct spi_module module;
    const struct samd21_spi_config *pconfig;
    uint8_t flags;

    hal_spi_txrx_cb txrx_cb;
    void *txrx_cb_arg;
};

#define HAL_SAMD21_SPI_MAX (6)

#if MYNEWT_VAL(SPI_0)
static struct samd21_hal_spi samd21_hal_spi0;
#endif
#if MYNEWT_VAL(SPI_1)
static struct samd21_hal_spi samd21_hal_spi1;
#endif
#if MYNEWT_VAL(SPI_2)
static struct samd21_hal_spi samd21_hal_spi2;
#endif
#if MYNEWT_VAL(SPI_3)
static struct samd21_hal_spi samd21_hal_spi3;
#endif
#if MYNEWT_VAL(SPI_4)
static struct samd21_hal_spi samd21_hal_spi4;
#endif
#if MYNEWT_VAL(SPI_5)
static struct samd21_hal_spi samd21_hal_spi5;
#endif

static struct samd21_hal_spi *samd21_hal_spis[HAL_SAMD21_SPI_MAX] = {
#if MYNEWT_VAL(SPI_0)
    &samd21_hal_spi0,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_1)
    &samd21_hal_spi1,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_2)
    &samd21_hal_spi2,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_3)
    &samd21_hal_spi3,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_4)
    &samd21_hal_spi4,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_5)
    &samd21_hal_spi5,
#else
    NULL,
#endif
};

static int
samd21_hal_spi_rc_from_status(enum status_code status)
{
    switch (status) {
    case STATUS_OK:
        return 0;

    case STATUS_ERR_INVALID_ARG:
        return EINVAL;

    case STATUS_ERR_TIMEOUT:
    case STATUS_ERR_DENIED:
    case STATUS_ERR_OVERFLOW:
    default:
        return EIO;
    }
}

static struct samd21_hal_spi *
samd21_hal_spi_resolve(int spi_num)
{
    struct samd21_hal_spi *spi;

    if (spi_num >= HAL_SAMD21_SPI_MAX) {
        spi = NULL;
    } else {
        spi = samd21_hal_spis[spi_num];
    }

    return spi;
}

static struct samd21_hal_spi *
samd21_hal_spi_resolve_module(const struct spi_module *module)
{
    struct samd21_hal_spi *spi;
    int i;

    for (i = 0; i < HAL_SAMD21_SPI_MAX; i++) {
        spi = samd21_hal_spis[i];
        if (&spi->module == module) {
            return spi;
        }
    }

    return NULL;
}

int
hal_spi_init(int spi_num, void *cfg, uint8_t spi_type)
{
    struct samd21_hal_spi *spi;

    if (cfg == NULL) {
        return EINVAL;
    }

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }
    memset(spi, 0, sizeof *spi);

    spi->module.hw = samd21_sercom(spi_num);
    if (spi->module.hw == NULL) {
        return EINVAL;
    }

    switch (spi_type) {
    case HAL_SPI_TYPE_MASTER:
        spi->flags |= SAMD21_SPI_FLAG_MASTER;
        break;

    case HAL_SPI_TYPE_SLAVE:
        break;

    default:
        return EINVAL;
    }

    spi->pconfig = cfg;

    return 0;
}

static int
samd21_spi_config(struct samd21_hal_spi *spi,
                  struct hal_spi_settings *settings)
{
    struct spi_config cfg;
    int rc;

    spi_get_config_defaults(&cfg);

    cfg.pinmux_pad0 = spi->pconfig->pad0_pinmux;
    cfg.pinmux_pad1 = spi->pconfig->pad1_pinmux;
    cfg.pinmux_pad2 = spi->pconfig->pad2_pinmux;
    cfg.pinmux_pad3 = spi->pconfig->pad3_pinmux;

    cfg.mux_setting = spi->pconfig->dopo << SERCOM_SPI_CTRLA_DOPO_Pos |
                      spi->pconfig->dipo << SERCOM_SPI_CTRLA_DIPO_Pos;


    /* apply the hal_settings */
    switch (settings->word_size) {
    case HAL_SPI_WORD_SIZE_8BIT:
        cfg.character_size = SPI_CHARACTER_SIZE_8BIT;
        break;
    case HAL_SPI_WORD_SIZE_9BIT:
        cfg.character_size = SPI_CHARACTER_SIZE_9BIT;
        break;
    default:
        return EINVAL;
    }

    switch (settings->data_order) {
    case HAL_SPI_LSB_FIRST:
        cfg.data_order = SPI_DATA_ORDER_LSB;
        break;
    case HAL_SPI_MSB_FIRST:
        cfg.data_order = SPI_DATA_ORDER_MSB;
        break;
    default:
        return EINVAL;
    }

    switch (settings->data_mode) {
    case HAL_SPI_MODE0:
        cfg.transfer_mode = SPI_TRANSFER_MODE_0;
        break;
    case HAL_SPI_MODE1:
        cfg.transfer_mode = SPI_TRANSFER_MODE_1;
        break;
    case HAL_SPI_MODE2:
        cfg.transfer_mode = SPI_TRANSFER_MODE_2;
        break;
    case HAL_SPI_MODE3:
        cfg.transfer_mode = SPI_TRANSFER_MODE_3;
        break;
    default:
        return EINVAL;
    }

    if (spi->flags & SAMD21_SPI_FLAG_MASTER) {
        cfg.mode = SPI_MODE_MASTER;
        cfg.mode_specific.master.baudrate = settings->baudrate;
    } else {
        cfg.mode = SPI_MODE_SLAVE;
        cfg.mode_specific.slave.frame_format = SPI_FRAME_FORMAT_SPI_FRAME;
        cfg.mode_specific.slave.preload_enable = true;
    }

    rc = spi_init(&spi->module, spi->module.hw, &cfg);
    if (rc != STATUS_OK) {
        return EIO;
    }

    return 0;
}

int
hal_spi_config(int spi_num, struct hal_spi_settings *settings)
{
    struct samd21_hal_spi *spi;
    int rc;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    if (spi->flags & SAMD21_SPI_FLAG_ENABLED) {
        return EACCES;
    }

    rc = samd21_spi_config(spi, settings);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
hal_spi_enable(int spi_num)
{
    struct samd21_hal_spi *spi;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    /* Configure and initialize software device instance of peripheral slave */
    spi_enable(&spi->module);
    spi->flags |= SAMD21_SPI_FLAG_ENABLED;

    return 0;
}

int
hal_spi_disable(int spi_num)
{
    struct samd21_hal_spi *spi;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    spi_disable(&spi->module);
    spi->flags &= ~SAMD21_SPI_FLAG_ENABLED;

    return 0;
}

uint16_t
hal_spi_tx_val(int spi_num, uint16_t tx)
{
    struct samd21_hal_spi *spi;
    enum status_code status;
    uint16_t rx;

    spi = samd21_hal_spi_resolve(spi_num);
    assert(spi != NULL);
    assert(!(spi->flags & SAMD21_SPI_FLAG_XFER));

    status = spi_transceive_wait(&spi->module, tx, &rx);
    assert(status == 0);

    return rx;
}

static void
samd21_hal_spi_cb(struct spi_module *module, enum spi_callback callback_type,
                  uint16_t xfr_count)
{
    struct samd21_hal_spi *spi;

    spi = samd21_hal_spi_resolve_module(module);
    if (spi == NULL) {
        return;
    }

    if (spi->flags & SAMD21_SPI_FLAG_XFER) {
        spi->flags &= ~SAMD21_SPI_FLAG_XFER;

        assert(spi->txrx_cb != NULL);
        spi->txrx_cb(spi->txrx_cb_arg, xfr_count);
    }
}

int
hal_spi_set_txrx_cb(int spi_num, hal_spi_txrx_cb txrx_cb, void *arg)
{
    struct samd21_hal_spi *spi;
    enum spi_callback type;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    if (spi->flags & SAMD21_SPI_FLAG_XFER) {
        return EACCES;
    }

    for (type = SPI_CALLBACK_BUFFER_TRANSMITTED;
         type != SPI_CALLBACK_N;
         type++) {

        if (txrx_cb == NULL) {
            spi_disable_callback(&spi->module, type);
        } else {
            spi_register_callback(&spi->module, samd21_hal_spi_cb, type);
            spi_enable_callback(&spi->module, type);
        }
    }

    spi->txrx_cb = txrx_cb;
    spi->txrx_cb_arg = arg;

    return 0;
}

static int
samd21_hal_spi_txrx_blocking(struct samd21_hal_spi *spi, void *txbuf,
                             void *rxbuf, uint16_t len)
{
    enum status_code status;

    spi->flags |= SAMD21_SPI_FLAG_XFER;
    status = spi_transceive_buffer_wait(&spi->module, txbuf, rxbuf, len);
    spi->flags &= ~SAMD21_SPI_FLAG_XFER;

    return samd21_hal_spi_rc_from_status(status);
}

static int
samd21_hal_spi_txrx_nonblocking(struct samd21_hal_spi *spi, void *txbuf,
                                void *rxbuf, uint16_t len)
{
    enum status_code status;

    spi->flags |= SAMD21_SPI_FLAG_XFER;

    status = spi_transceive_buffer_job(&spi->module, txbuf, rxbuf, len);

    return samd21_hal_spi_rc_from_status(status);
}

int
hal_spi_txrx(int spi_num, void *txbuf, void *rxbuf, int len)
{
    struct samd21_hal_spi *spi;
    int rc;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    if (len <= 0 || len > UINT16_MAX) {
        return EINVAL;
    }

    if (spi->flags & SAMD21_SPI_FLAG_XFER) {
        return EALREADY;
    }

    if (spi->flags & SAMD21_SPI_FLAG_MASTER) {
        if (txbuf == NULL) {
            return EINVAL;
        }
    } else {
        if (txbuf == NULL && rxbuf == NULL) {
            return EINVAL;
        }
    }

    if (spi->txrx_cb == NULL) {
        rc = samd21_hal_spi_txrx_blocking(spi, txbuf, rxbuf, len);
    } else {
        rc = samd21_hal_spi_txrx_nonblocking(spi, txbuf, rxbuf, len);
    }
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * Not supported by the Atmel SPI API.
 */
int
hal_spi_slave_set_def_tx_val(int spi_num, uint16_t val)
{
    struct samd21_hal_spi *spi;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    spi_set_dummy(&spi->module, val);

    return 0;
}

int
hal_spi_abort(int spi_num)
{
    struct samd21_hal_spi *spi;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    spi_abort_job(&spi->module);
    spi->flags &= ~SAMD21_SPI_FLAG_XFER;

    return 0;
}
