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
#include "syscfg/syscfg.h"
#include "os/mynewt.h"
#include "mcu/mcu.h"
#include "mcu/kinetis_hal.h"
#include "hal/hal_spi.h"

#include <fsl_dspi.h>
#include <fsl_port.h>
#include <fsl_clock.h>

/* The maximum number of SPI interfaces we will allow */
#define NXP_HAL_SPI_MAX 3

enum spi_type_t {
    TYPE_MASTER = HAL_SPI_TYPE_MASTER,
    TYPE_SLAVE = HAL_SPI_TYPE_SLAVE
};

struct nxp_hal_spi {
    SPI_Type *dev;
    uint32_t clk_pin;
    uint32_t pcs_pin;
    uint32_t sout_pin;
    uint32_t sin_pin;
    PORT_Type *port;
    port_mux_t mux;
    IRQn_Type irqn;
    void (*irq_handler)(void);
    hal_spi_txrx_cb txrx_cb;
    void *txrx_cb_arg;
    bool enabled;
    enum spi_type_t type;
};

struct nxp_spi_master {
    struct nxp_hal_spi hal_spi;
    dspi_master_config_t config;
    dspi_master_handle_t handle;
};

struct nxp_spi_slave {
    struct nxp_hal_spi hal_spi;
    dspi_slave_config_t config;
    dspi_slave_handle_t handle;
};

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
static void spi0_irq(void);
#endif
#if MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
static void spi1_irq(void);
#endif
#if MYNEWT_VAL(SPI_2_MASTER) || MYNEWT_VAL(SPI_2_SLAVE)
static void spi2_irq(void);
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
struct nxp_spi_master hal_spi0 = {
    .hal_spi = {
        .dev = SPI0,
        .clk_pin = MYNEWT_VAL(SPI_0_MASTER_PIN_SCK),
        .pcs_pin = 0, /* unused */
        .sout_pin = MYNEWT_VAL(SPI_0_MASTER_PIN_MOSI),
        .sin_pin = MYNEWT_VAL(SPI_0_MASTER_PIN_MISO),
        .port = MYNEWT_VAL(SPI_0_MASTER_PORT),
        .mux = MYNEWT_VAL(SPI_0_MASTER_MUX),
        .irqn = SPI0_IRQn,
        .irq_handler = spi0_irq,
        .txrx_cb = NULL,
        .txrx_cb_arg = NULL,
        .enabled = false,
        .type = HAL_SPI_TYPE_MASTER,
    },
    .config = {0},
    .handle = {0},
};
#elif MYNEWT_VAL(SPI_0_SLAVE)
struct nxp_spi_slave hal_spi0 = {
    .hal_spi = {
        .dev = SPI0,
        .clk_pin = MYNEWT_VAL(SPI_0_SLAVE_PIN_SCK),
        .pcs_pin = MYNEWT_VAL(SPI_0_SLAVE_PIN_SS),
        .sout_pin = MYNEWT_VAL(SPI_0_SLAVE_PIN_MISO),
        .sin_pin = MYNEWT_VAL(SPI_0_SLAVE_PIN_MOSI),
        .port = MYNEWT_VAL(SPI_0_SLAVE_PORT),
        .mux = MYNEWT_VAL(SPI_0_SLAVE_MUX),
        .irqn = SPI0_IRQn,
        .irq_handler = spi0_irq,
        .txrx_cb = NULL,
        .txrx_cb_arg = NULL,
        .enabled = false,
        .type = HAL_SPI_TYPE_SLAVE,
    },
    .config = {0},
    .handle = {0},
};
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
struct nxp_spi_master hal_spi1 = {
    .hal_spi = {
        .dev = SPI1,
        .clk_pin = MYNEWT_VAL(SPI_1_MASTER_PIN_SCK),
        .pcs_pin = 0, /* unused */
        .sout_pin = MYNEWT_VAL(SPI_1_MASTER_PIN_MOSI),
        .sin_pin = MYNEWT_VAL(SPI_1_MASTER_PIN_MISO),
        .port = MYNEWT_VAL(SPI_1_MASTER_PORT),
        .mux = MYNEWT_VAL(SPI_1_MASTER_MUX),
        .irqn = SPI1_IRQn,
        .irq_handler = spi1_irq,
        .txrx_cb = NULL,
        .txrx_cb_arg = NULL,
        .enabled = false,
        .type = HAL_SPI_TYPE_MASTER,
    },
    .config = {0},
    .handle = {0},
};
#elif MYNEWT_VAL(SPI_1_SLAVE)
struct nxp_spi_slave hal_spi1 = {
    .hal_spi = {
        .dev = SPI1,
        .clk_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_SCK),
        .pcs_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_SS),
        .sout_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_MISO),
        .sin_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_MOSI),
        .port = MYNEWT_VAL(SPI_1_SLAVE_PORT),
        .mux = MYNEWT_VAL(SPI_1_SLAVE_MUX),
        .irqn = SPI1_IRQn,
        .irq_handler = spi1_irq,
        .txrx_cb = NULL,
        .txrx_cb_arg = NULL,
        .enabled = false,
        .type = HAL_SPI_TYPE_SLAVE,
    },
    .config = {0},
    .handle = {0},
};
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
struct nxp_spi_master hal_spi2 = {
    .hal_spi = {
        .dev = SPI2,
        .clk_pin = MYNEWT_VAL(SPI_1_MASTER_PIN_SCK),
        .pcs_pin = 0, /* unused */
        .sout_pin = MYNEWT_VAL(SPI_1_MASTER_PIN_MOSI),
        .sin_pin = MYNEWT_VAL(SPI_1_MASTER_PIN_MISO),
        .port = MYNEWT_VAL(SPI_1_MASTER_PORT),
        .mux = MYNEWT_VAL(SPI_1_MASTER_MUX),
        .irqn = SPI2_IRQn,
        .irq_handler = spi2_irq,
        .txrx_cb = NULL,
        .txrx_cb_arg = NULL,
        .enabled = false,
        .type = HAL_SPI_TYPE_MASTER,
    },
    .config = {0},
    .handle = {0},
};
#elif MYNEWT_VAL(SPI_2_SLAVE)
struct nxp_spi_slave hal_spi2 = {
    .hal_spi = {
        .dev = SPI2,
        .clk_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_SCK),
        .pcs_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_SS),
        .sout_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_MISO),
        .sin_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_MOSI),
        .port = MYNEWT_VAL(SPI_1_SLAVE_PORT),
        .mux = MYNEWT_VAL(SPI_1_SLAVE_MUX),
        .irqn = SPI2_IRQn,
        .irq_handler = spi2_irq,
        .txrx_cb = NULL,
        .txrx_cb_arg = NULL,
        .enabled = false,
        .type = HAL_SPI_TYPE_SLAVE,
    },
    .config = {0},
    .handle = {0},
};
#endif

static struct nxp_hal_spi *spi_modules[NXP_HAL_SPI_MAX] = {
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
    (struct nxp_hal_spi *) &hal_spi0,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
    (struct nxp_hal_spi *) &hal_spi1,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_2_MASTER) || MYNEWT_VAL(SPI_2_SLAVE)
    (struct nxp_hal_spi *) &hal_spi2
#else
    NULL
#endif
};

static struct nxp_hal_spi *
hal_spi_resolve(int spi_num)
{
    if (spi_num >= NXP_HAL_SPI_MAX) {
        return NULL;
    }
    return spi_modules[spi_num];
}

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
static void
spi0_irq(void)
{
#if MYNEWT_VAL(SPI_0_MASTER)
    DSPI_MasterTransferHandleIRQ(hal_spi0.hal_spi.dev, &hal_spi0.handle);
#elif MYNEWT_VAL(SPI_0_SLAVE)
    DSPI_SlaveTransferHandleIRQ(hal_spi0.hal_spi.dev, &hal_spi0.handle);
#endif
}
#endif

#if MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
static void
spi1_irq(void)
{
#if MYNEWT_VAL(SPI_1_MASTER)
    DSPI_MasterTransferHandleIRQ(hal_spi1.hal_spi.dev, &hal_spi1.handle);
#elif MYNEWT_VAL(SPI_1_SLAVE)
    DSPI_SlaveTransferHandleIRQ(hal_spi1.hal_spi.dev, &hal_spi1.handle);
#endif
}
#endif

#if MYNEWT_VAL(SPI_2_MASTER) || MYNEWT_VAL(SPI_2_SLAVE)
static void
spi2_irq(void)
{
#if MYNEWT_VAL(SPI_2_MASTER)
    DSPI_MasterTransferHandleIRQ(hal_spi2.hal_spi.dev, &hal_spi2.handle);
#elif MYNEWT_VAL(SPI_2_SLAVE)
    DSPI_SlaveTransferHandleIRQ(hal_spi2.hal_spi.dev, &hal_spi2.handle);
#endif
}
#endif

static void
hal_spi_slave_xfer_cb(SPI_Type *base, dspi_slave_handle_t *handle, status_t status, void *userData)
{
    struct nxp_hal_spi *spi = (struct nxp_hal_spi *) userData;

    if (status == kStatus_Success) {
        if (spi->txrx_cb) {
            spi->txrx_cb(spi->txrx_cb_arg, handle->totalByteCount);
        }
    }
}

static void
hal_spi_master_xfer_cb(SPI_Type *base, dspi_master_handle_t *handle, status_t status, void *userData)
{
    struct nxp_hal_spi *spi = (struct nxp_hal_spi *) userData;

    if (status == kStatus_Success) {
        if (spi->txrx_cb) {
            spi->txrx_cb(spi->txrx_cb_arg, handle->totalByteCount);
        }
    }
}

static int
hal_spi_init_master(struct nxp_hal_spi *spi,
                    const struct nxp_hal_spi_cfg *cfg)
{
    struct nxp_spi_master *master;

    if (spi->type == HAL_SPI_TYPE_MASTER) {
        master = (struct nxp_spi_master *) spi;
        PORT_SetPinMux(spi->port, spi->clk_pin, spi->mux);
        PORT_SetPinMux(spi->port, spi->sin_pin, spi->mux);
        PORT_SetPinMux(spi->port, spi->sout_pin, spi->mux);
        DSPI_MasterGetDefaultConfig(&master->config);
        return 0;
    }

    return EINVAL;
}

static int
hal_spi_init_slave(struct nxp_hal_spi *spi,
                   const struct nxp_hal_spi_cfg *cfg)
{
    struct nxp_spi_slave *slave;

    if (spi->type == HAL_SPI_TYPE_SLAVE) {
        slave = (struct nxp_spi_slave *) spi;
        PORT_SetPinMux(spi->port, spi->clk_pin, spi->mux);
        PORT_SetPinMux(spi->port, spi->sin_pin, spi->mux);
        PORT_SetPinMux(spi->port, spi->sout_pin, spi->mux);
        PORT_SetPinMux(spi->port, spi->pcs_pin, spi->mux);
        DSPI_SlaveGetDefaultConfig(&slave->config);
        return 0;
    }

    return EINVAL;
}


int
hal_spi_init(int spi_num, void *cfg, uint8_t spi_type)
{
    struct nxp_hal_spi *spi;

    /* cfg isn't implemented, change pin usage using mynewt vals for now. */
    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return EINVAL;
    }

    if (spi_type != spi->type) {
        return EINVAL;
    }

    if (spi_type == HAL_SPI_TYPE_MASTER) {
        return hal_spi_init_master(spi, cfg);
    } else {
        return hal_spi_init_slave(spi, cfg);
    }
    return EINVAL;
}

int
hal_spi_init_hw(uint8_t spi_num,
                uint8_t spi_type,
                const struct hal_spi_hw_settings *cfg)
{
    struct nxp_hal_spi_cfg hal_cfg;
    hal_cfg.clk_pin = (uint8_t)cfg->pin_sck;
    if (spi_type == HAL_SPI_TYPE_MASTER) {
        hal_cfg.sout_pin = (uint8_t)cfg->pin_mosi;
        hal_cfg.sin_pin = (uint8_t)cfg->pin_miso;
    } else {
        hal_cfg.sin_pin = (uint8_t)cfg->pin_mosi;
        hal_cfg.sout_pin = (uint8_t)cfg->pin_miso;
    }
    hal_cfg.pcs_pin = (uint8_t)cfg->pin_ss;
    return hal_spi_init(spi_num, &hal_cfg, spi_type);
}

int
hal_spi_config(int spi_num, struct hal_spi_settings *settings)
{
    struct nxp_hal_spi *spi;
    struct nxp_spi_master *master;
    struct nxp_spi_slave *slave;
    dspi_clock_polarity_t cpol;
    dspi_clock_phase_t cpha;

    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return EINVAL;
    }

    if (!settings) {
        return EINVAL;
    }

    switch (settings->data_mode) {
    case HAL_SPI_MODE0:
        cpol = kDSPI_ClockPolarityActiveHigh;
        cpha = kDSPI_ClockPhaseFirstEdge;
        break;
    case HAL_SPI_MODE1:
        cpol = kDSPI_ClockPolarityActiveHigh;
        cpha = kDSPI_ClockPhaseSecondEdge;
        break;
    case HAL_SPI_MODE2:
        cpol = kDSPI_ClockPolarityActiveLow;
        cpha = kDSPI_ClockPhaseFirstEdge;
        break;
    case HAL_SPI_MODE3:
        cpol = kDSPI_ClockPolarityActiveLow;
        cpha = kDSPI_ClockPhaseSecondEdge;
        break;
    default:
        return EINVAL;
    }

    if (spi->type == HAL_SPI_TYPE_MASTER) {
        master = (struct nxp_spi_master *) spi;

        master->config.ctarConfig.baudRate = settings->baudrate;
        master->config.ctarConfig.pcsToSckDelayInNanoSec =
            1000000000U / master->config.ctarConfig.baudRate;
        master->config.ctarConfig.lastSckToPcsDelayInNanoSec =
            1000000000U / master->config.ctarConfig.baudRate;
        master->config.ctarConfig.betweenTransferDelayInNanoSec =
            1000000000U / master->config.ctarConfig.baudRate;
        master->config.ctarConfig.direction =
            (settings->data_order == HAL_SPI_MSB_FIRST) ?
            kDSPI_MsbFirst :
            kDSPI_LsbFirst;
        master->config.ctarConfig.bitsPerFrame =
            (settings->word_size == HAL_SPI_WORD_SIZE_8BIT) ? 8 : 9;
        master->config.ctarConfig.cpol = cpol;
        master->config.ctarConfig.cpha = cpha;
    } else {
        slave = (struct nxp_spi_slave *) spi;
        slave->config.ctarConfig.bitsPerFrame =
            (settings->word_size == HAL_SPI_WORD_SIZE_8BIT) ? 8 : 9;
        slave->config.ctarConfig.cpol = cpol;
        slave->config.ctarConfig.cpha = cpha;
    }
    return 0;
}

int
hal_spi_enable(int spi_num)
{
    struct nxp_hal_spi *spi;
    struct nxp_spi_master *master;
    struct nxp_spi_slave *slave;
    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return EINVAL;
    }
    if (spi->enabled) {
        return 0;
    }

    if (spi->type == HAL_SPI_TYPE_MASTER) {
        master = (struct nxp_spi_master *) spi;
        DSPI_MasterInit(spi->dev,
                        &master->config,
                        CLOCK_GetFreq(kCLOCK_BusClk));
    } else {
        slave = (struct nxp_spi_slave *) spi;
        DSPI_SlaveInit(spi->dev, &slave->config);
    }

    spi->enabled = true;
    NVIC_ClearPendingIRQ(spi->irqn);
    NVIC_SetVector(spi->irqn, (uint32_t) spi->irq_handler);
    NVIC_EnableIRQ(spi->irqn);
    return 0;
}

int
hal_spi_disable(int spi_num)
{
    struct nxp_hal_spi *spi;
    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return EINVAL;
    }
    if (!spi->enabled) {
        return 0;
    }

    DSPI_Deinit(spi->dev);

    spi->enabled = false;
    NVIC_ClearPendingIRQ(spi->irqn);
    NVIC_DisableIRQ(spi->irqn);
    return 0;
}

uint16_t
hal_spi_tx_val(int spi_num, uint16_t val)
{
    struct nxp_hal_spi *spi;
    dspi_transfer_t xfer;
    uint16_t retval = 0;
    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return EINVAL;
    }

    if (spi->type == HAL_SPI_TYPE_MASTER) {
        xfer.txData = (uint8_t *) &val;
        xfer.rxData = (uint8_t *) &retval;
        xfer.dataSize = 1;
        xfer.configFlags = kDSPI_MasterCtar0;
        DSPI_MasterTransferBlocking(spi->dev, &xfer);
        return retval;
    }
    return 0xFFFF; /* Invalid API. */
}

int
hal_spi_txrx(int spi_num, void *txbuf, void *rxbuf, int len)
{
    struct nxp_hal_spi *spi;
    dspi_transfer_t xfer;
    status_t rc = -1;
    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return EINVAL;
    }

    if (spi->type == HAL_SPI_TYPE_MASTER) {
        xfer.txData = (uint8_t *) txbuf;
        xfer.rxData = (uint8_t *) rxbuf;
        xfer.dataSize = len;
        xfer.configFlags = kDSPI_MasterCtar0;
        rc = DSPI_MasterTransferBlocking(spi->dev, &xfer);
    }
    return (rc == kStatus_Success) ? 0 : rc;
}

int
hal_spi_set_txrx_cb(int spi_num, hal_spi_txrx_cb txrx_cb, void *arg)
{
    struct nxp_hal_spi *spi;
    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return EINVAL;
    }

    spi->txrx_cb = txrx_cb;
    spi->txrx_cb_arg = arg;
    return 0;
}

int
hal_spi_txrx_noblock(int spi_num, void *txbuf, void *rxbuf, int len)
{
    struct nxp_hal_spi *spi;
    struct nxp_spi_master *master;
    struct nxp_spi_slave *slave;
    dspi_transfer_t xfer;
    status_t rc;
    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return EINVAL;
    }

    if (spi->type == HAL_SPI_TYPE_MASTER) {
        master = (struct nxp_spi_master *) spi;
        DSPI_MasterTransferCreateHandle(spi->dev,
                                        &master->handle,
                                        hal_spi_master_xfer_cb,
                                        spi);
        xfer.configFlags = kDSPI_MasterCtar0;
        xfer.txData = (uint8_t *) txbuf;
        xfer.rxData = (uint8_t *) rxbuf;
        xfer.dataSize = len;
        rc = DSPI_MasterTransferNonBlocking(spi->dev, &master->handle, &xfer);
    } else {
        slave = (struct nxp_spi_slave *) spi;
        DSPI_SlaveTransferCreateHandle(spi->dev,
                                       &slave->handle,
                                       hal_spi_slave_xfer_cb,
                                       spi);
        xfer.configFlags = kDSPI_SlaveCtar0;
        xfer.txData = (uint8_t *) txbuf;
        xfer.rxData = (uint8_t *) rxbuf;
        xfer.dataSize = len;
        rc = DSPI_SlaveTransferNonBlocking(spi->dev, &slave->handle, &xfer);

    }
    return (rc == kStatus_Success) ? 0 : rc;
}

int
hal_spi_slave_set_def_tx_val(int spi_num, uint16_t val)
{
    struct nxp_hal_spi *spi;
    uint8_t val8;
    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return EINVAL;
    }

    if (spi->type == HAL_SPI_TYPE_SLAVE) {
        val8 = (uint8_t) val | 0xff;
        DSPI_SetDummyData(spi->dev, val8);
        return 0;
    }

    return EINVAL;
}

int
hal_spi_abort(int spi_num)
{
    struct nxp_hal_spi *spi;
    struct nxp_spi_master *master;
    struct nxp_spi_slave *slave;
    spi = hal_spi_resolve(spi_num);
    if (!spi) {
        return EINVAL;
    }

    if (spi->type == HAL_SPI_TYPE_MASTER) {
        master = (struct nxp_spi_master *) spi;
        DSPI_MasterTransferAbort(spi->dev, &master->handle);
    } else {
        slave = (struct nxp_spi_slave *) spi;
        DSPI_SlaveTransferAbort(spi->dev, &slave->handle);
    }
    return 0;
}
