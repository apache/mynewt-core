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


#include <os/mynewt.h>
#include <assert.h>

#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>

#include <defs/error.h>
#include <syscfg/syscfg.h>

#include <mcu/fe310_hal.h>
#include <mcu/sys_clock.h>
#include <mcu/plic.h>
#include <sifive/devices/spi.h>
#include <env/freedom-e300-hifive1/platform.h>

struct fe310_hal_spi {
    struct hal_spi_settings spi_cfg;

    uint32_t spi_base;

    int len;
    int txleft;
    int rxleft;

    /* Pointers to tx/rx buffers */
    uint8_t *txbuf;
    uint8_t *rxbuf;

    /* Callback and arguments */
    hal_spi_txrx_cb txrx_cb_func;
    void            *txrx_cb_arg;
};

#if MYNEWT_VAL(SPI_1)
struct fe310_hal_spi fe310_hal_spi1 = {
    .spi_base = SPI1_CTRL_ADDR
};
#endif

#if MYNEWT_VAL(SPI_2)
struct fe310_hal_spi fe310_hal_spi2 = {
    .spi_base = SPI2_CTRL_ADDR
};
#endif

static struct fe310_hal_spi *fe310_hal_spis[3] = {
    NULL, /* SPI 0 used to access flash */
#if MYNEWT_VAL(SPI_1)
    &fe310_hal_spi1,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_2)
    &fe310_hal_spi2,
#else
    NULL,
#endif
};

#define FE310_HAL_SPI_RESOLVE(__n, __v)                     \
    if ((__n) > 2) {                                        \
        rc = SYS_EINVAL;                                    \
        goto err;                                           \
    }                                                       \
    (__v) = (struct fe310_hal_spi *) fe310_hal_spis[(__n)]; \
    if ((__v) == NULL) {                                    \
        rc = SYS_EINVAL;                                    \
        goto err;                                           \
    }

static void
spi_interrupt_handler(int int_num)
{
    struct fe310_hal_spi *spi = fe310_hal_spis[int_num - INT_SPI0_BASE];
    uint32_t val;
    int i;

    /* For sanity reason finish interrupt even if transmission is faster
     * then the FIFO feeding code.
     * FIFO is 8 bytes long, 10 should be enough to fill up FIFO.
     */
    for (i = 10; i != 0; --i) {
        val = _REG32(spi->spi_base, SPI_REG_RXFIFO);
        if (spi->rxleft && (val & SPI_RXFIFO_EMPTY) == 0) {
            if (spi->rxbuf) {
                *spi->rxbuf++ = (uint8_t)val;
            }
            spi->rxleft--;
        }
        if (spi->txleft) {
            if (_REG32(spi->spi_base, SPI_REG_TXFIFO) & SPI_TXFIFO_FULL) {
                break;
            }
            _REG32(spi->spi_base, SPI_REG_TXFIFO) = *spi->txbuf++;
            spi->txleft--;
        } else {
            /* Set watermark to 0, interrupt from TX FIFO will be blocked */
            _REG32(spi->spi_base, SPI_REG_TXCTRL) = 0;
            if (!spi->rxleft) {
                plic_disable_interrupt(int_num);
                _REG32(spi->spi_base, SPI_REG_IE) = 0;
                spi->txrx_cb_func(spi->txrx_cb_arg, spi->len);
                break;
            }
        }
    }
}

int
hal_spi_init(int spi_num, void *usercfg, uint8_t spi_type)
{
    int rc;
    struct fe310_hal_spi *spi;
    FE310_HAL_SPI_RESOLVE(spi_num, spi);

    /* Check for valid arguments */
    rc = SYS_EINVAL;
    if ((spi_type != HAL_SPI_TYPE_MASTER)) {
        goto err;
    }

    plic_set_handler(INT_SPI0_BASE + spi_num, spi_interrupt_handler, 3);
    if (spi_num == 1 && MYNEWT_VAL(SPI_1)) {
        /* Set GPIO alternative function IOF0 for SPI1 pins */
        _REG32(GPIO_CTRL_ADDR, GPIO_IOF_SEL) &=
            ~(_BITUL(IOF_SPI1_MOSI) |
              _BITUL(IOF_SPI1_MISO) |
              _BITUL(IOF_SPI1_SCK));
        /* Enable alternate function for SPI1 pins */
        _REG32(GPIO_CTRL_ADDR, GPIO_IOF_EN) |=
            (_BITUL(IOF_SPI1_MOSI) |
             _BITUL(IOF_SPI1_MISO) |
             _BITUL(IOF_SPI1_SCK));
    } else if (spi_num == 2 && MYNEWT_VAL(SPI_2)) {
        /* Set GPIO alternative function IOF0 for SPI2 pins */
        _REG32(GPIO_CTRL_ADDR, GPIO_IOF_SEL) &=
            ~(_BITUL(IOF_SPI2_MOSI) |
              _BITUL(IOF_SPI2_MISO) |
              _BITUL(IOF_SPI2_SCK));
        /* Enable alternate function for SPI2 pins */
        _REG32(GPIO_CTRL_ADDR, GPIO_IOF_EN) |=
            (_BITUL(IOF_SPI2_MOSI) |
             _BITUL(IOF_SPI2_MISO) |
             _BITUL(IOF_SPI2_SCK));
    } else {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = 0;
err:
    return (rc);
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
    int rc = 0;
    struct fe310_hal_spi *spi;
    FE310_HAL_SPI_RESOLVE(spi_num, spi);

    spi->txrx_cb_func = txrx_cb;
    spi->txrx_cb_arg = arg;
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
    return 0;
}

int
hal_spi_config(int spi_num, struct hal_spi_settings *settings)
{
    int rc = 0;
    uint32_t fmt = SPI_FMT_LEN(8);
    uint32_t div;
    struct fe310_hal_spi *spi;
    FE310_HAL_SPI_RESOLVE(spi_num, spi);

    if (settings->data_mode > HAL_SPI_MODE3 ||
        settings->word_size != HAL_SPI_WORD_SIZE_8BIT ||
        settings->data_order > HAL_SPI_LSB_FIRST) {
        rc = SYS_EINVAL;
        goto err;
    }
    if (settings->data_order == HAL_SPI_LSB_FIRST) {
        fmt |= SPI_FMT_ENDIAN(1);
    }
    div = get_cpu_freq() / (2 * settings->baudrate);
    if (div) {
        div--;
    }
    if (div >= (1 << 12)) {
        div = 0xFFF;
    }
    spi->spi_cfg = *settings;
    _REG32(spi->spi_base, SPI_REG_SCKDIV) = div;
    _REG32(spi->spi_base, SPI_REG_SCKMODE) = settings->data_mode;
    _REG32(spi->spi_base, SPI_REG_FMT) = fmt;
    _REG32(spi->spi_base, SPI_REG_CSID) = 0;
    /* HAL does not support hardware CS */
    _REG32(spi->spi_base, SPI_REG_CSMODE) = SPI_CSMODE_OFF;
    /* TX FIFO will not raise interrupts */
    _REG32(spi->spi_base, SPI_REG_TXCTRL) = 0;
    _REG32(spi->spi_base, SPI_REG_RXCTRL) = 0;
    _REG32(spi->spi_base, SPI_REG_IE) = 0;
err:
    return (rc);
}

int
hal_spi_txrx_noblock(int spi_num, void *txbuf, void *rxbuf, int len)
{
    int sr;
    int rc = 0;
    struct fe310_hal_spi *spi;
    FE310_HAL_SPI_RESOLVE(spi_num, spi);

    __HAL_DISABLE_INTERRUPTS(sr);
    spi->rxbuf = (uint8_t *)rxbuf;
    spi->txbuf = (uint8_t *)txbuf;
    spi->rxleft = len;
    spi->txleft = len;
    spi->len = len;
    _REG32(spi->spi_base, SPI_REG_TXCTRL) = 4;
    _REG32(spi->spi_base, SPI_REG_IE) = 1;
    plic_enable_interrupt(INT_SPI0_BASE + spi_num);
    __HAL_ENABLE_INTERRUPTS(sr);
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
    uint32_t retval;
    struct fe310_hal_spi *spi;
    int rc = 0;
    FE310_HAL_SPI_RESOLVE(spi_num, spi);

    while (_REG32(spi->spi_base, SPI_REG_TXFIFO) & SPI_TXFIFO_FULL) {
    }
    _REG32(spi->spi_base, SPI_REG_TXFIFO) = val;
    do {
        retval = _REG32(spi->spi_base, SPI_REG_RXFIFO);
    } while (retval & SPI_RXFIFO_EMPTY);

err:
    return rc ? 0xFFFF : (uint8_t)retval;
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
    int rc = 0;
    struct fe310_hal_spi *spi;
    uint32_t val;
    int sent = 0;
    int received = 0;
    FE310_HAL_SPI_RESOLVE(spi_num, spi);

    if (!len) {
        rc = SYS_EINVAL;
        goto err;
    }
    while (_REG32(spi->spi_base, SPI_REG_TXFIFO) & SPI_TXFIFO_FULL) {
        if (_REG32(spi->spi_base, SPI_REG_RXFIFO)) {
        }
    }
    while (!(_REG32(spi->spi_base, SPI_REG_RXFIFO) & SPI_RXFIFO_EMPTY)) {
    }

    while (received < len) {
        if (sent < len &&
            !(_REG32(spi->spi_base, SPI_REG_TXFIFO) & SPI_TXFIFO_FULL)) {
            _REG32(spi->spi_base, SPI_REG_TXFIFO) = ((uint8_t *)txbuf)[sent++];
        }
        val = _REG32(spi->spi_base, SPI_REG_RXFIFO);
        if (!(val & SPI_RXFIFO_EMPTY)) {
            if (rxbuf) {
                ((uint8_t *)rxbuf)[received] = (uint8_t)val;
            }
            received++;
        }
    }

    rc = 0;
err:
    return rc;
}

int
hal_spi_abort(int spi_num)
{
    int rc = 0;
    struct fe310_hal_spi *spi;
    int sr;

    __HAL_DISABLE_INTERRUPTS(sr);
    FE310_HAL_SPI_RESOLVE(spi_num, spi);

    _REG32(spi->spi_base, SPI_REG_IE) = 0;
    _REG32(spi->spi_base, SPI_REG_TXCTRL) = 0;
    while ((_REG32(spi->spi_base, SPI_REG_RXFIFO) & SPI_RXFIFO_EMPTY) == 0) {
    }
    plic_disable_interrupt(spi_num + INT_SPI0_BASE);
    spi->txbuf = NULL;
    spi->rxbuf = NULL;
    spi->txleft = 0;
    spi->rxleft = 0;
err:
    __HAL_ENABLE_INTERRUPTS(sr);
    return rc;
}
