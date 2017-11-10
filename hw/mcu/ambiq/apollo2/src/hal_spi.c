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
#include <stdbool.h>
#include "syscfg/syscfg.h"
#include "hal/hal_spi.h"
#include "mcu/hal_apollo2.h"
#include "bsp/cmsis_nvic.h"
#include "defs/error.h"

#include "am_mcu_apollo.h"

/* Prevent CMSIS from breaking apollo2 macros. */
#undef GPIO
#undef IOSLAVE
#undef CLKGEN

#define SPI_0_ENABLED (MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE))
#define SPI_1_ENABLED (MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE))
#define SPI_2_ENABLED (MYNEWT_VAL(SPI_2_MASTER) || MYNEWT_VAL(SPI_2_SLAVE))
#define SPI_3_ENABLED (MYNEWT_VAL(SPI_3_MASTER) || MYNEWT_VAL(SPI_3_SLAVE))
#define SPI_4_ENABLED (MYNEWT_VAL(SPI_4_MASTER) || MYNEWT_VAL(SPI_4_SLAVE))
#define SPI_5_ENABLED (MYNEWT_VAL(SPI_5_MASTER) || MYNEWT_VAL(SPI_5_SLAVE))

#define SPI_ANY_ENABLED (       \
    SPI_0_ENABLED ||            \
    SPI_1_ENABLED ||            \
    SPI_2_ENABLED ||            \
    SPI_3_ENABLED ||            \
    SPI_4_ENABLED ||            \
    SPI_5_ENABLED)

#if SPI_ANY_ENABLED

#define SPI_N_MASTER (          \
    MYNEWT_VAL(SPI_0_MASTER) || \
    MYNEWT_VAL(SPI_1_MASTER) || \
    MYNEWT_VAL(SPI_2_MASTER) || \
    MYNEWT_VAL(SPI_3_MASTER) || \
    MYNEWT_VAL(SPI_4_MASTER) || \
    MYNEWT_VAL(SPI_5_MASTER))

#define SPI_N_SLAVE (           \
    MYNEWT_VAL(SPI_0_SLAVE) ||  \
    MYNEWT_VAL(SPI_1_SLAVE) ||  \
    MYNEWT_VAL(SPI_2_SLAVE) ||  \
    MYNEWT_VAL(SPI_3_SLAVE) ||  \
    MYNEWT_VAL(SPI_4_SLAVE) ||  \
    MYNEWT_VAL(SPI_5_SLAVE))

#define APOLLO2_SPI_MAX_CHUNK_SZ    64
#define APOLLO2_SPI_MAX_CHUNK_WORDS (APOLLO2_SPI_MAX_CHUNK_SZ / 4)
#define APOLLO2_SPI_MAX_TXR_SZ      4095

#define APOLLO2_SPI_OP_NONE         0
#define APOLLO2_SPI_OP_BLOCKING     1
#define APOLLO2_SPI_OP_NONBLOCKING  2

/* IRQ handler type */
typedef void apollo2_spi_irq_handler(void);

struct apollo2_spi {
    volatile uint8_t op;

    const uint8_t *txbuf;
    uint8_t *rxbuf;
    int buf_num_bytes;
    int buf_off;
    uint32_t interrupts;
    uint16_t buf_len;
    uint8_t prev_num_bytes;
    uint8_t spi_num;
    uint8_t spi_type;

    uint8_t enabled:1;

    hal_spi_txrx_cb txrx_cb_func;
    void *txrx_cb_arg;
};

static void apollo2_spi_service_master(struct apollo2_spi *spi,
                                       uint32_t status);

static uint32_t apollo2_spi_fifo[APOLLO2_SPI_MAX_CHUNK_WORDS];

#if SPI_0_ENABLED
static struct apollo2_spi apollo2_spi0;
#endif
#if SPI_1_ENABLED
static struct apollo2_spi apollo2_spi1;
#endif
#if SPI_2_ENABLED
static struct apollo2_spi apollo2_spi2;
#endif
#if SPI_3_ENABLED
static struct apollo2_spi apollo2_spi3;
#endif
#if SPI_4_ENABLED
static struct apollo2_spi apollo2_spi4;
#endif
#if SPI_5_ENABLED
static struct apollo2_spi apollo2_spi5;
#endif

static uint32_t
apollo2_spi_data_mode_to_ios(int spi_mode)
{
    switch (spi_mode) {
        case HAL_SPI_MODE0:     return AM_HAL_IOS_SPIMODE_0;
        case HAL_SPI_MODE1:     return AM_HAL_IOS_SPIMODE_1;
        case HAL_SPI_MODE2:     return AM_HAL_IOS_SPIMODE_2;
        case HAL_SPI_MODE3:     return AM_HAL_IOS_SPIMODE_3;
        default:                return -1;
    }
}

static struct apollo2_spi *
apollo2_spi_resolve(int spi_num)
{
    switch (spi_num) {
#if SPI_0_ENABLED
    case 0:
        return &apollo2_spi0;
#endif
#if SPI_1_ENABLED
    case 1:
        return &apollo2_spi1;
#endif
#if SPI_2_ENABLED
    case 2:
        return &apollo2_spi2;
#endif
#if SPI_3_ENABLED
    case 3:
        return &apollo2_spi3;
#endif
#if SPI_4_ENABLED
    case 4:
        return &apollo2_spi4;
#endif
#if SPI_5_ENABLED
    case 5:
        return &apollo2_spi5;
#endif
    default:
        return NULL;
    }
}

static int
apollo2_spi_fifo_count(int spi_num)
{
    return AM_BFRn(IOMSTR, spi_num, FIFOPTR, FIFOSIZ);
}

static int
apollo2_spi_fifo_space(int spi_num)
{
    return APOLLO2_SPI_MAX_CHUNK_SZ - apollo2_spi_fifo_count(spi_num);
}

static void
apollo2_spi_block_until_idle(const struct apollo2_spi *spi)
{
    while (spi->op != APOLLO2_SPI_OP_NONE) { }
}

static void
apollo2_spi_clear_ints(int spi_num)
{
    AM_REGn(IOMSTR, spi_num, INTCLR) = 0xffffffff;
}

static void
apollo2_spi_disable_ints(struct apollo2_spi *spi)
{
    /* Remember currently-enabled interrupts. */
    assert(spi->interrupts == 0);
    spi->interrupts = AM_REGn(IOMSTR, spi->spi_num, INTEN);

    /* Disable interrupts. */
    AM_REGn(IOMSTR, spi->spi_num, INTEN) = 0;
}

static void
apollo2_spi_reenable_ints(struct apollo2_spi *spi)
{
    AM_REGn(IOMSTR, spi->spi_num, INTEN) = spi->interrupts;
    spi->interrupts = 0;
}

static uint32_t
apollo2_spi_status(int spi_num)
{
    uint32_t status;

    status = AM_REGn(IOMSTR, spi_num, INTSTAT);
    apollo2_spi_clear_ints(spi_num);

    return status;
}

static void
apollo2_spi_irqh_x(int spi_num)
{
    struct apollo2_spi *spi;
    uint32_t status;

    status = apollo2_spi_status(spi_num);

    spi = apollo2_spi_resolve(spi_num);
    assert(spi != NULL);

    switch (spi->spi_type) {
    case HAL_SPI_TYPE_MASTER:
        apollo2_spi_service_master(spi, status);
        break;

    case HAL_SPI_TYPE_SLAVE:
        /* XXX: Slave unimplemented. */
        break;

    default:
        assert(0);
        break;
    }
}

#if SPI_0_ENABLED
static void apollo2_spi_irqh_0(void) { apollo2_spi_irqh_x(0); }
#endif
#if SPI_1_ENABLED
static void apollo2_spi_irqh_1(void) { apollo2_spi_irqh_x(1); }
#endif
#if SPI_2_ENABLED
static void apollo2_spi_irqh_2(void) { apollo2_spi_irqh_x(2); }
#endif
#if SPI_3_ENABLED
static void apollo2_spi_irqh_3(void) { apollo2_spi_irqh_x(3); }
#endif
#if SPI_4_ENABLED
static void apollo2_spi_irqh_4(void) { apollo2_spi_irqh_x(4); }
#endif
#if SPI_5_ENABLED
static void apollo2_spi_irqh_5(void) { apollo2_spi_irqh_x(5); }
#endif

static int
apollo2_spi_irq_info(int spi_num, int *out_irq_num,
                     apollo2_spi_irq_handler **out_irqh)
{
    switch (spi_num) {
#if SPI_0_ENABLED
    case 0:
        *out_irq_num = IOMSTR0_IRQn;
        *out_irqh = apollo2_spi_irqh_0;
        return 0;
#endif
#if SPI_1_ENABLED
    case 1:
        *out_irq_num = IOMSTR1_IRQn;
        *out_irqh = apollo2_spi_irqh_1;
        return 0;
#endif
#if SPI_2_ENABLED
    case 2:
        *out_irq_num = IOMSTR2_IRQn;
        *out_irqh = apollo2_spi_irqh_2;
        return 0;
#endif
#if SPI_3_ENABLED
    case 3:
        *out_irq_num = IOMSTR3_IRQn;
        *out_irqh = apollo2_spi_irqh_3;
        return 0;
#endif
#if SPI_4_ENABLED
    case 4:
        *out_irq_num = IOMSTR4_IRQn;
        *out_irqh = apollo2_spi_irqh_4;
        return 0;
#endif
#if SPI_5_ENABLED
    case 5:
        *out_irq_num = IOMSTR5_IRQn;
        *out_irqh = apollo2_spi_irqh_5;
        return 0;
#endif
    default:
        return SYS_EINVAL;
    }
}

static int
hal_spi_config_master(int spi_num, const struct hal_spi_settings *settings)
{
    am_hal_iom_config_t sdk_config;
    int cpol;
    int cpha;
    int rc;

    if (spi_num < 0 || spi_num >= AM_REG_IOMSTR_NUM_MODULES) {
        return SYS_EINVAL;
    }

    rc = hal_spi_data_mode_breakout(settings->data_mode, &cpol, &cpha);
    if (rc != 0) {
        return SYS_EINVAL;
    }

    am_hal_iom_pwrctrl_enable(spi_num);

    sdk_config.ui32InterfaceMode =
        AM_HAL_IOM_SPIMODE | AM_REG_IOMSTR_CFG_FULLDUP_FULLDUP;
    sdk_config.ui32ClockFrequency = settings->baudrate;
    sdk_config.bSPHA = cpha;
    sdk_config.bSPOL = cpol;
    sdk_config.ui8WriteThreshold = 4;
    sdk_config.ui8ReadThreshold = 60;
    am_hal_iom_config(spi_num, &sdk_config);

    return 0;
}

static int
hal_spi_config_slave(int spi_num, const struct hal_spi_settings *settings)
{
    uint32_t ios_data_mode;
    uint32_t cfg;

    cfg = AM_REG_IOSLAVE_FIFOCFG_ROBASE(0x78 >> 3);
    cfg |= AM_REG_IOSLAVE_FIFOCFG_FIFOBASE(0x80 >> 3);
    cfg |= AM_REG_IOSLAVE_FIFOCFG_FIFOMAX(0x100 >> 3);

    ios_data_mode = apollo2_spi_data_mode_to_ios(settings->data_mode);

    AM_REG(IOSLAVE, CFG) = ios_data_mode;
    AM_REG(IOSLAVE, FIFOCFG) = cfg;
    return 0;
}

/*  | spi:cfg   | sck   | miso  | mosi  |
 *  |-----------+-------+-------+-------|
 *  | 0:1       | 5     | 6     | 7     |
 *  | 1:1       | 8     | 9     | 10    |
 *  | 2:5       | 0     | 2     | 1     |
 *  | 2:5       | 27    | 28    | 25    |
 *  | 3:5       | 42    | 43    | 38    |
 *  | 4:5       | 39    | 40    | 44    |
 *  | 5:5       | 48    | 49    | 47    |
 */
static int
hal_spi_pin_config_master(int spi_num, const struct apollo2_spi_cfg *pins)
{
    const uint8_t miso = pins->miso_pin;
    const uint8_t mosi = pins->mosi_pin;
    const uint8_t sck = pins->sck_pin;

    switch (spi_num) {
#if SPI_0_ENABLED
    case 0:
        if (sck == 5 && miso == 6 && mosi == 7) {
            return 1;
        } else {
            return -1;
        }
#endif
#if SPI_1_ENABLED
    case 1:
        if (sck == 8 && miso == 9 && mosi == 10) {
            return 1;
        } else {
            return -1;
        }
#endif
#if SPI_2_ENABLED
    case 2:
        if (sck == 0 && miso == 2 && mosi == 1) {
            return 5;
        } else if (sck == 27 && miso == 28 && mosi == 25) {
            return 5;
        } else {
            return -1;
        }
#endif
#if SPI_3_ENABLED
    case 3:
        if (sck == 42 && miso == 43 && mosi == 38) {
            return 5;
        } else {
            return -1;
        }
#endif
#if SPI_4_ENABLED
    case 4:
        if (sck == 39 && miso == 40 && mosi == 44) {
            return 5;
        } else {
            return -1;
        }
#endif
#if SPI_5_ENABLED
    case 5:
        if (sck == 48 && miso == 49 && mosi == 47) {
            return 5;
        } else {
            return -1;
        }
#endif
    default:
        return -1;
    }
}

static int
hal_spi_pin_config(int spi_num, int master, const struct apollo2_spi_cfg *pins)
{
    if (master) {
        return hal_spi_pin_config_master(spi_num, pins);
    } else {
        return -1;
    }
}

static int
hal_spi_init_master(int spi_num, const struct apollo2_spi_cfg *cfg)
{
    apollo2_spi_irq_handler *irqh;
    struct apollo2_spi *spi;
    int pin_cfg;
    int irq_num;
    int rc;

    spi = apollo2_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    pin_cfg = hal_spi_pin_config(spi_num, 1, cfg);
    if (pin_cfg == -1) {
        return SYS_EINVAL;
    }

    am_hal_gpio_pin_config(
        cfg->sck_pin, AM_HAL_GPIO_FUNC(pin_cfg) | AM_HAL_PIN_DIR_INPUT);
    am_hal_gpio_pin_config(
        cfg->miso_pin, AM_HAL_GPIO_FUNC(pin_cfg) | AM_HAL_PIN_DIR_INPUT);
    am_hal_gpio_pin_config(
        cfg->mosi_pin, AM_HAL_GPIO_FUNC(pin_cfg));

    memset(spi, 0, sizeof *spi);
    spi->spi_num = spi_num;
    spi->spi_type = HAL_SPI_TYPE_MASTER;

    rc = apollo2_spi_irq_info(spi_num, &irq_num, &irqh);
    if (rc != 0) {
        return rc;
    }

    NVIC_SetVector(irq_num, (uint32_t)irqh);
    NVIC_SetPriority(irq_num, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_ClearPendingIRQ(irq_num);
    NVIC_EnableIRQ(irq_num);

    return 0;
}

static int
hal_spi_init_slave(int spi_num, struct apollo2_spi_cfg *cfg)
{
    return SYS_ERANGE;
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

    if (cfg == NULL) {
        return SYS_EINVAL;
    }

    switch (spi_type) {
    case HAL_SPI_TYPE_MASTER:
        rc = hal_spi_init_master(spi_num, cfg);
        if (rc != 0) {
            return rc;
        }
        break;

    case HAL_SPI_TYPE_SLAVE:
        rc = hal_spi_init_slave(spi_num, cfg);
        if (rc != 0) {
            return rc;
        }
        break;

    default:
        return SYS_EINVAL;
    }

    return 0;
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
    const struct apollo2_spi *spi;
    int rc;

    spi = apollo2_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    if (spi->spi_type == HAL_SPI_TYPE_MASTER) {
        rc = hal_spi_config_master(spi_num, settings);
    } else {
        rc = hal_spi_config_slave(spi_num, settings);
    }

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
    struct apollo2_spi *spi;

    spi = apollo2_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    if (spi->enabled) {
        return SYS_EBUSY;
    }

    switch (spi->spi_type) {
    case HAL_SPI_TYPE_MASTER:
        AM_REGn(IOMSTR, spi_num, CFG) |= AM_REG_IOMSTR_CFG_IFCEN(1);
        AM_REGn(IOMSTR, spi_num, INTEN) = 0xffffffff;

        if (spi_num == 0) {
            AM_REGn(GPIO, 0, PADKEY) = AM_REG_GPIO_PADKEY_KEYVAL;
            AM_BFW(GPIO, PADREGB, PAD5INPEN, 1);
            AM_BFW(GPIO, PADREGB, PAD6INPEN, 1);
            AM_REGn(GPIO, 0, PADKEY) = 0;
        } else {
            AM_REGn(GPIO, 0, PADKEY) = AM_REG_GPIO_PADKEY_KEYVAL;
            AM_BFW(GPIO, PADREGC, PAD8INPEN, 1);
            AM_BFW(GPIO, PADREGC, PAD9INPEN, 1);
            AM_REGn(GPIO, 0, PADKEY) = 0;
        }
        break;

    case HAL_SPI_TYPE_SLAVE:
        AM_REGn(IOSLAVE, spi_num, CFG) |= AM_REG_IOSLAVE_CFG_IFCEN(1);
        break;

    default:
        return SYS_EINVAL;
    }

    spi->enabled = 1;
    return 0;
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
    struct apollo2_spi *spi;

    spi = apollo2_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    switch (spi->spi_type) {
    case HAL_SPI_TYPE_MASTER:
        apollo2_spi_block_until_idle(spi);
        AM_REGn(IOMSTR, spi_num, CFG) &= ~AM_REG_IOMSTR_CFG_IFCEN(1);
        spi->enabled = 0;
        return 0;

    case HAL_SPI_TYPE_SLAVE:
        AM_REGn(IOSLAVE, spi_num, CFG) &= ~AM_REG_IOSLAVE_CFG_IFCEN(1);
        return 0;

    default:
        return SYS_EINVAL;
    }
}

static void
apollo2_spi_fifo_read(struct apollo2_spi *spi, void *rx_data, int num_bytes)
{
    int num_words;
    int i;

    num_words = (num_bytes + 3) / 4;
    for (i = 0; i < num_words; i++) {
        apollo2_spi_fifo[i] = AM_REGn(IOMSTR, spi->spi_num, FIFO);
    }

    if (rx_data != NULL) {
        memcpy(rx_data, apollo2_spi_fifo, num_bytes);
    }
}

static void
apollo2_spi_fifo_write(struct apollo2_spi *spi,
                       const void *tx_data, int num_bytes)
{
    uint32_t word;
    int num_words;
    int i;

    assert(num_bytes != 0);

    memcpy(apollo2_spi_fifo, tx_data, num_bytes);

    num_words = (num_bytes + 3) / 4;
    for (i = 0; i < num_words; i++) {
        if (tx_data == NULL) {
            word = 0;
        } else {
            word = apollo2_spi_fifo[i];
        }
        AM_REGn(IOMSTR, spi->spi_num, FIFO) = word;
    }
}

static int
apollo2_spi_next_chunk_sz(int buf_sz, int off, int fifo_space)
{
    int bytes_left;

    bytes_left = buf_sz - off;
    if (bytes_left > fifo_space) {
        return fifo_space;
    } else {
        return bytes_left;
    }
}

static int
apollo2_spi_tx_next_chunk(struct apollo2_spi *spi)
{
    int fifo_space;
    int chunk_sz;

    fifo_space = apollo2_spi_fifo_space(spi->spi_num);
    chunk_sz = apollo2_spi_next_chunk_sz(spi->buf_num_bytes, spi->buf_off,
                                         fifo_space);
    if (chunk_sz <= 0) {
        return 0;
    }

    apollo2_spi_clear_ints(spi->spi_num);

    apollo2_spi_fifo_write(spi, spi->txbuf + spi->buf_off, chunk_sz);
    spi->prev_num_bytes = chunk_sz;

    return SYS_EAGAIN;
}

static uint32_t
apollo2_spi_cmd_build(uint16_t num_bytes, uint8_t channel)
{
    return 0x40000000  /* Raw write. */     |
           (num_bytes & 0xF00) << 15        |
           (num_bytes & 0xFF)               |
           channel << 16;
}

static void
apollo2_spi_tx_first_chunk(struct apollo2_spi *spi)
{
    uint32_t cmd;

    apollo2_spi_tx_next_chunk(spi);

    cmd = apollo2_spi_cmd_build(spi->buf_num_bytes, 0);
    apollo2_spi_disable_ints(spi);
    AM_REGn(IOMSTR, spi->spi_num, CMD) = cmd;
    apollo2_spi_reenable_ints(spi);
}

static void
apollo2_spi_service_master(struct apollo2_spi *spi, uint32_t status)
{
    uint8_t prev_op;
    int rc;

    if (spi->op == APOLLO2_SPI_OP_NONE) {
        /* Spurious interrupt or programming error. */
        return;
    }

    /* Copy received data. */
    apollo2_spi_fifo_read(spi, spi->rxbuf + spi->buf_off, spi->prev_num_bytes);
    spi->buf_off += spi->prev_num_bytes;

    assert(spi->buf_off <= spi->buf_num_bytes);

    if (!(status & AM_HAL_IOM_INT_THR)) {
        /* Error or command complete. */

        prev_op = spi->op;
        spi->op = APOLLO2_SPI_OP_NONE;

        if (prev_op == APOLLO2_SPI_OP_NONBLOCKING) {
            spi->txrx_cb_func(spi->txrx_cb_arg, spi->buf_off);
        }

        return;
    }

    /* Transmit next chunk. */
    rc = apollo2_spi_tx_next_chunk(spi);
    assert(rc == 0);
}

static int
apollo2_spi_txrx_begin(struct apollo2_spi *spi, uint8_t op,
                       const void *tx_data, void *rx_data, int num_bytes)
{
    if (spi->op != APOLLO2_SPI_OP_NONE) {
        return SYS_EBUSY;
    }

    if (num_bytes <= 0 || num_bytes > APOLLO2_SPI_MAX_TXR_SZ) {
        return SYS_EINVAL;
    }

    spi->op = op;
    spi->txbuf = tx_data;
    spi->rxbuf = rx_data;
    spi->buf_num_bytes = num_bytes;
    spi->buf_off = 0;

    apollo2_spi_tx_first_chunk(spi);
    return 0;
}

static int
apollo2_spi_txrx_blocking(struct apollo2_spi *spi,
                          const void *tx_data, void *rx_data, int num_bytes)
{
    int rc;

    rc = apollo2_spi_txrx_begin(spi, APOLLO2_SPI_OP_BLOCKING,
                                tx_data, rx_data, num_bytes);
    if (rc != 0) {
        return rc;
    }

    apollo2_spi_block_until_idle(spi);

    return 0;
}

/**
 * Blocking call to send a value on the SPI. Returns the value received from
 * the SPI slave.
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
uint16_t
hal_spi_tx_val(int spi_num, uint16_t val)
{
    struct apollo2_spi *spi;
    uint8_t tx_data;
    uint8_t rx_data;
    int rc;

    spi = apollo2_spi_resolve(spi_num);
    if (spi == NULL) {
        return 0xffff;
    }

    switch (spi->spi_type) {
    case HAL_SPI_TYPE_MASTER:
        tx_data = val;
        rc = apollo2_spi_txrx_blocking(spi, &tx_data, &rx_data, 1);
        if (rc == 0) {
            return rx_data;
        } else {
            return 0xffff;
        }

    case HAL_SPI_TYPE_SLAVE:
        return 0xffff;

    default:
        return 0xffff;
    }
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
    struct apollo2_spi *spi;

    spi = apollo2_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    if (spi->enabled) {
        return SYS_EBUSY;
    }

    spi->txrx_cb_func = txrx_cb;
    spi->txrx_cb_arg = arg;

    return 0;
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
hal_spi_txrx(int spi_num, void *txbuf, void *rxbuf, int num_bytes)
{
    struct apollo2_spi *spi;
    int rc;

    spi = apollo2_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    rc = apollo2_spi_txrx_blocking(spi, txbuf, rxbuf, num_bytes);
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
 * @param num_bytes Number of 8-bit values to be transferred.
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_txrx_noblock(int spi_num, void *txbuf, void *rxbuf, int num_bytes)
{
    struct apollo2_spi *spi;
    int rc;

    spi = apollo2_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    if (spi->txrx_cb_func == NULL) {
        return SYS_ENOENT;
    }

    if (spi->op != APOLLO2_SPI_OP_NONE) {
        return SYS_EBUSY;
    }

    switch (spi->spi_type) {
    case HAL_SPI_TYPE_MASTER:
        rc = apollo2_spi_txrx_begin(spi, APOLLO2_SPI_OP_NONBLOCKING,
                                    txbuf, rxbuf, num_bytes);
        return rc;

    case HAL_SPI_TYPE_SLAVE:
        spi->txbuf = txbuf;
        spi->rxbuf = rxbuf;
        spi->op = APOLLO2_SPI_OP_NONBLOCKING;
        return 0;

    default:
        return SYS_EINVAL;
    }
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
    return SYS_ERANGE;
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
    return SYS_ERANGE;
}

#endif /* SPI_ANY_ENABLED */
