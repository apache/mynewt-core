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

#include <stdbool.h>
#include <stddef.h>
#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"
#include "mcu/mcu.h"
#include "mcu/mips_hal.h"
#include "mcu/p32mx470f512h.h"
#include "mcu/pps.h"

#define SPIxCON(P)          (base_address[P][0x0 / 0x4])
#define SPIxCONCLR(P)       (base_address[P][0x4 / 0x4])
#define SPIxCONSET(P)       (base_address[P][0x8 / 0x4])
#define SPIxSTAT(P)         (base_address[P][0x10 / 0x4])
#define SPIxSTATCLR(P)      (base_address[P][0x14 / 0x4])
#define SPIxBUF(P)          (base_address[P][0x20 / 0x4])
#define SPIxBRG(P)          (base_address[P][0x30 / 0x4])
#define SPIxCON2(P)         (base_address[P][0x40 / 0x4])

static volatile uint32_t * base_address[SPI_CNT] = {
    (volatile uint32_t *)_SPI1_BASE_ADDRESS,
    (volatile uint32_t *)_SPI2_BASE_ADDRESS,
};

struct hal_spi {
    bool                      slave;
    uint8_t                   *txbuf;
    uint8_t                   *rxbuf;
    int                       len;
    int                       txcnt;
    int                       rxcnt;
    hal_spi_txrx_cb           callback;
    void                      *arg;
    const struct mips_spi_cfg *pins;
    uint32_t                  con;
    uint32_t                  brg;
};

static struct hal_spi spis[SPI_CNT];

static void
hal_spi_power_up(int spi_num)
{
    uint32_t mask = 0;

    switch (spi_num) {
        case 0:
            mask = _PMD5_SPI1MD_MASK;
            break;
        case 1:
            mask = _PMD5_SPI2MD_MASK;
            break;
    }

    if (!(PMD5 & mask))
        return;

    PMD5CLR = mask;

    /* It appeared that powering down the SPI module also clears SPIxBRG and SPIxCON */
    SPIxBRG(spi_num) = spis[spi_num].brg;
    SPIxCON(spi_num) = spis[spi_num].con;
}

static void
hal_spi_power_down(int spi_num)
{
    /* It appeared that powering down the SPI module also clears SPIxBRG and SPIxCON */
    spis[spi_num].brg = SPIxBRG(spi_num);
    spis[spi_num].con = SPIxCON(spi_num);

    switch (spi_num) {
        case 0:
            PMD5SET = _PMD5_SPI1MD_MASK;
            break;
        case 1:
            PMD5SET = _PMD5_SPI2MD_MASK;
            break;
    }
}

static int
hal_spi_config_master(int spi_num, struct hal_spi_settings *psettings)
{
    uint32_t pbclk, pbdiv;

    /*
     * Make sure that the SPI module is not powered down.
     * If the module is powered down, one cannot write to registers.
     */
    hal_spi_power_up(spi_num);

    SPIxCON(spi_num) = 0;
    SPIxCON2(spi_num) = 0;

    /* Clear RX FIFO */
    while (!(SPIxSTAT(spi_num) & _SPI1STAT_SPITBE_MASK)) {
        volatile int rdata = SPIxBUF(spi_num);
        (void)rdata;
    }

    /* The SPI module only supports MSB first */
    if (psettings->data_order == HAL_SPI_LSB_FIRST) {
        return -1;
    }

    /* Only 8-bit word size is supported */
    if (psettings->word_size != HAL_SPI_WORD_SIZE_8BIT) {
        return -1;
    }

    switch (psettings->data_mode) {
        case HAL_SPI_MODE0:
            SPIxCONCLR(spi_num) = _SPI1CON_CKP_MASK;
            SPIxCONSET(spi_num) = _SPI1CON_CKE_MASK;
            break;
        case HAL_SPI_MODE1:
            SPIxCONCLR(spi_num) = _SPI1CON_CKP_MASK | _SPI1CON_CKE_MASK;
            break;
        case HAL_SPI_MODE2:
            SPIxCONSET(spi_num) = _SPI1CON_CKP_MASK | _SPI1CON_CKE_MASK;
            break;
        case HAL_SPI_MODE3:
            SPIxCONCLR(spi_num) = _SPI1CON_CKE_MASK;
            SPIxCONSET(spi_num) = _SPI1CON_CKP_MASK;
            break;
        default:
            return -1;
    }

    /*
     * From equation 23-1 of Section 23 of PIC32 FRM:
     *
     *                 Fpb2
     * Fsck =  -------------------
     *          2 * (SPIxBRG + 1)
     */
    pbdiv = (OSCCON & _OSCCON_PBDIV_MASK) >> _OSCCON_PBDIV_POSITION;
    pbclk = MYNEWT_VAL(CLOCK_FREQ) / (pbdiv + 1);
    SPIxBRG(spi_num) = (pbclk / (2 * psettings->baudrate)) - 1;

    SPIxSTATCLR(spi_num) = _SPI1STAT_SPIROV_MASK;
    SPIxCONSET(spi_num) = _SPI1CON_ENHBUF_MASK | _SPI1CON_MSTEN_MASK;

    return 0;
}

static int
hal_spi_config_pins(int spi_num, uint8_t mode)
{
    int ret = 0;

    if (hal_gpio_init_out(spis[spi_num].pins->mosi, 0) ||
        hal_gpio_init_out(spis[spi_num].pins->sck, 1) ||
        hal_gpio_init_in(spis[spi_num].pins->miso, HAL_GPIO_PULL_NONE)) {
        return -1;
    }

    /*
     * To avoid any glitches when turning off and on module, the SCK pin must
     * be set to the correct value depending on the mode.
     */
    switch (mode) {
        case HAL_SPI_MODE0:
        case HAL_SPI_MODE1:
            hal_gpio_write(spis[spi_num].pins->sck, 0);
            break;
        case HAL_SPI_MODE2:
        case HAL_SPI_MODE3:
            hal_gpio_write(spis[spi_num].pins->sck, 1);
            break;
    }

    switch (spi_num) {
        case 0:
            ret += pps_configure_output(spis[spi_num].pins->mosi,
                                        SDO1_OUT_FUNC);
            ret += pps_configure_input(spis[spi_num].pins->miso,
                                       SDI1_IN_FUNC);
            break;
        case 1:
            ret += pps_configure_output(spis[spi_num].pins->mosi,
                                        SDO2_OUT_FUNC);
            ret += pps_configure_input(spis[spi_num].pins->miso,
                                       SDI2_IN_FUNC);
            break;
    }

    return ret;
}

static void
hal_spi_enable_int(int spi_num)
{
    switch (spi_num) {
        case 0:
            IFS1CLR = _IFS1_SPI1TXIF_MASK;
            IEC1SET = _IEC1_SPI1TXIE_MASK;
            break;
        case 1:
            IFS1CLR = _IFS1_SPI2TXIF_MASK;
            IEC1SET = _IEC1_SPI2TXIE_MASK;
            break;
    }
}

static void
hal_spi_disable_int(int spi_num)
{
    switch (spi_num) {
        case 0:
            IFS1CLR = _IFS1_SPI1TXIF_MASK;
            IEC1CLR = _IEC1_SPI1TXIE_MASK;
            break;
        case 1:
            IFS1CLR = _IFS1_SPI2TXIF_MASK;
            IEC1CLR = _IEC1_SPI2TXIE_MASK;
            break;
    }
}

static void
hal_spi_handle_isr(int spi_num)
{
    /* Read everything in RX FIFO */
    while (!(SPIxSTAT(spi_num) & _SPI1STAT_SPIRBE_MASK)) {
        volatile uint32_t rxdata = SPIxBUF(spi_num);
        if (spis[spi_num].rxbuf && spis[spi_num].rxcnt) {
            *spis[spi_num].rxbuf++ = rxdata;
            --spis[spi_num].rxcnt;
        }
    }

    if (spis[spi_num].txcnt == 0 && spis[spi_num].rxcnt == 0) {
        spis[spi_num].txbuf = NULL;
        spis[spi_num].rxbuf = NULL;

        if (spis[spi_num].callback) {
            spis[spi_num].callback(spis[spi_num].arg, spis[spi_num].len);
        }
        hal_spi_disable_int(spi_num);
    }


    /* Fill TX FIFO */
    while (spis[spi_num].txcnt &&
           !(SPIxSTAT(spi_num) & _SPI1STAT_SPITBF_MASK)) {
        SPIxBUF(spi_num) = *spis[spi_num].txbuf++;
        --spis[spi_num].txcnt;
    }
}

void
__attribute__((interrupt(IPL2AUTO), vector(_SPI_1_VECTOR)))
hal_spi1_isr(void)
{
    hal_spi_handle_isr(0);
    IFS1CLR = _IFS1_SPI1TXIF_MASK;
}

void
__attribute__((interrupt(IPL2AUTO), vector(_SPI_2_VECTOR)))
hal_spi2_isr(void)
{
    hal_spi_handle_isr(1);
    IFS1CLR = _IFS1_SPI2TXIF_MASK;
}

int
hal_spi_init(int spi_num, void *cfg, uint8_t spi_type)
{
    if (spi_type != HAL_SPI_TYPE_MASTER &&
        spi_type != HAL_SPI_TYPE_SLAVE) {
        return -1;
    }

    spis[spi_num].slave = spi_type;
    spis[spi_num].pins = cfg;

    return 0;
}

int
hal_spi_config(int spi_num, struct hal_spi_settings *psettings)
{
    /* Slave mode not supported */
    if (spis[spi_num].slave) {
        return -1;
    }

    /* Configure pins */
    if (spis[spi_num].pins) {
        if (hal_spi_config_pins(spi_num, psettings->data_mode)) {
            return -1;
        }
    }

    return hal_spi_config_master(spi_num, psettings);
}

int
hal_spi_set_txrx_cb(int spi_num, hal_spi_txrx_cb txrx_cb, void *arg)
{
    if (SPIxCON(spi_num) & _SPI1CON_ON_MASK) {
        return -1;
    }

    spis[spi_num].callback = txrx_cb;
    spis[spi_num].arg = arg;
    return 0;
}

int
hal_spi_enable(int spi_num)
{
    hal_spi_power_up(spi_num);
    SPIxCONSET(spi_num) = _SPI1CON_ON_MASK;

    return 0;
}

int
hal_spi_disable(int spi_num)
{
    /*
     * Disabling SPI clears the FIFO, so this makes sure that everything was
     * sent before disabling the module.
     */
    while (!(SPIxSTAT(spi_num) & _SPI1STAT_SPITBE_MASK)) {
    }

    SPIxCONCLR(spi_num) = _SPI1CON_ON_MASK;
    hal_spi_power_down(spi_num);

    return 0;
}

uint16_t
hal_spi_tx_val(int spi_num, uint16_t val)
{
    if (spis[spi_num].slave) {
        return 0xFFFF;
    }

    /* Wait until there is some space in TX FIFO */
    while (SPIxSTAT(spi_num) & _SPI1STAT_SPITBF_MASK) {
    }

    SPIxBUF(spi_num) = val;

    /* Wait until RX FIFO is not empty */
    while (SPIxSTAT(spi_num) & _SPI1STAT_SPIRBE_MASK) {
    }

    return SPIxBUF(spi_num);
}

int
hal_spi_txrx(int spi_num, void *txbuf, void *rxbuf, int cnt)
{
    uint8_t *tx = (uint8_t *)txbuf;
    uint8_t *rx = (uint8_t *)rxbuf;

    /* Slave mode not supported */
    if (spis[spi_num].slave) {
        return -1;
    }

    while (cnt--) {
        uint8_t rdata;
        if (tx) {
            /* Wait until there is some space in TX FIFO */
            while (SPIxSTAT(spi_num) & _SPI1STAT_SPITBF_MASK) {
            }

            SPIxBUF(spi_num) = *tx++;
        }

        /* Wait until RX FIFO is not empty */
        while (SPIxSTAT(spi_num) & _SPI1STAT_SPIRBE_MASK) {
        }

        /* Always read RX FIFO to avoid overrun */
        rdata = SPIxBUF(spi_num);

        if (rx) {
            *rx++ = rdata;
        }
    }

    return 0;
}

int
hal_spi_txrx_noblock(int spi_num, void *txbuf, void *rxbuf, int cnt)
{
    uint32_t ctx;

    /* Slave mode not supported */
    if (spis[spi_num].slave) {
        return -1;
    }

    if (txbuf == NULL) {
        return -1;
    }

    /* Check if a transfer is pending */
    if (spis[spi_num].rxbuf != NULL || spis[spi_num].txbuf != NULL) {
        return -1;
    }

    spis[spi_num].txbuf = txbuf;
    spis[spi_num].rxbuf = rxbuf;
    spis[spi_num].txcnt = cnt;
    spis[spi_num].rxcnt = cnt;
    spis[spi_num].len = cnt;

    /* Configure SPIxTXIF to trigger when TX FIFO is empty */
    SPIxCONCLR(spi_num) = _SPI1CON_STXISEL_MASK;
    SPIxCONSET(spi_num) = 0b01 << _SPI1CON_STXISEL_POSITION;

    /* Set interrupt priority */
    switch (spi_num) {
        case 0:
            IPC7CLR = _IPC7_SPI1IS_MASK | _IPC7_SPI1IP_MASK;
            IPC7SET = 2 << _IPC7_SPI1IP_POSITION;
            break;
        case 1:
            IPC8CLR = _IPC8_SPI2IS_MASK | _IPC8_SPI2IP_MASK;
            IPC8SET = 2 << _IPC8_SPI2IP_POSITION;
            break;
    }

    /* Enable interrupt */
    hal_spi_enable_int(spi_num);

    return 0;
}

int
hal_spi_slave_set_def_tx_val(int spi_num, uint16_t val)
{
    /* Slave mode not supported */
    return -1;
}

int
hal_spi_abort(int spi_num)
{
    /* Cannot abort transfer if spi is not enabled */
    if (!(SPIxCON(spi_num) & _SPI1CON_ON_MASK)) {
        return -1;
    }

    hal_spi_disable_int(spi_num);
    spis[spi_num].txbuf = NULL;
    spis[spi_num].rxbuf = NULL;
    spis[spi_num].txcnt = 0;
    spis[spi_num].rxcnt = 0;
    spis[spi_num].len = 0;

    /* Make sure that we finished transmitting current byte before turning off module */
    while (!(SPIxSTAT(spi_num) & _SPI1STAT_SRMT_MASK)) {
    }

    /* Clear TX and RX FIFO by turning off and on module */
    SPIxCONCLR(spi_num) = _SPI1CON_ON_MASK;
    asm volatile ("nop");
    SPIxCONSET(spi_num) = _SPI1CON_ON_MASK;

    return 0;
}
