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

#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>

#include <mmc/mmc.h>

#include <stdio.h>

/* Current used MMC commands */
#define CMD0                (0)            /* GO_IDLE_STATE */
#define CMD1                (1)            /* SEND_OP_COND (MMC) */
#define CMD8                (8)            /* SEND_IF_COND */
#define CMD16               (16)           /* SET_BLOCKLEN */
#define CMD17               (17)           /* READ_SINGLE_BLOCK */
#define CMD18               (18)           /* READ_MULTIPLE_BLOCK */
#define CMD24               (24)           /* WRITE_BLOCK */
#define CMD25               (25)           /* WRITE_MULTIPLE_BLOCK */
#define CMD55               (55)           /* APP_CMD */
#define CMD58               (58)           /* READ_OCR */
#define ACMD41              (0x80 + 41)    /* SEND_OP_COND (SDC) */

/* Response types */
#define R1                  (0)
#define R1b                 (1)
#define R2                  (2)
#define R3                  (3)
#define R7                  (4)

/* Response status */
#define R_IDLE              (0x01)
#define R_ERASE_RESET       (0x02)
#define R_ILLEGAL_COMMAND   (0x04)
#define R_CRC_ERROR         (0x08)
#define R_ERASE_ERROR       (0x10)
#define R_ADDR_ERROR        (0x20)
#define R_PARAM_ERROR       (0x40)

static struct hal_spi_settings mmc_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE0,
    /* XXX: MMC initialization accepts clocks in the range 100-400KHz */
    .baudrate   = 100,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};

static int g_spi_num = -1;
static int g_ss_pin = -1;

/**
 * TODO: switch to high-speed SPI after initialization.
 */

static int
mmc_error_by_status(uint8_t status)
{
    return MMC_OK;
}

static int8_t
response_type_by_cmd(uint8_t cmd)
{
    switch (cmd) {
        case CMD8   : return R7;
        case CMD58  : return R3;
    }
    return R1;
}

static uint32_t
ocr_from_r3(uint8_t *response)
{
    uint32_t ocr;

    ocr  = (uint32_t) response[4];
    ocr |= (uint32_t) response[3] <<  8;
    ocr |= (uint32_t) response[2] << 16;
    ocr |= (uint32_t) response[1] << 24;

    return ocr;
}

static uint8_t
status_from_r1(uint8_t *response)
{
    return response[0];
}

static uint8_t
voltage_from_r7(uint8_t *response)
{
    return response[3] & 0xF;
}

static uint8_t
send_mmc_cmd(uint8_t cmd, uint32_t payload)
{
    int n;
    uint8_t response[5];
    uint8_t status;
    uint8_t type;

    if (cmd & 0x80) {
        /* TODO: refactor recursion? */
        send_mmc_cmd(CMD55, 0);
    }

    hal_gpio_write(g_ss_pin, 0);

    /* 4.7.2: Command Format */
    hal_spi_tx_val(g_spi_num, 0x40 | (cmd & ~0x80));

    hal_spi_tx_val(g_spi_num, payload >> 24 & 0xff);
    hal_spi_tx_val(g_spi_num, payload >> 16 & 0xff);
    hal_spi_tx_val(g_spi_num, payload >>  8 & 0xff);
    hal_spi_tx_val(g_spi_num, payload       & 0xff);

    /**
     * 7.2.2 Bus Transfer Protection
     *   NOTE: SD is in CRC off mode by default but CMD0 and CMD8 always
     *   require a valid CRC.
     *   NOTE: CRC can be turned on with CMD59 (CRC_ON_OFF).
     */
    if (cmd == CMD0) {
        hal_spi_tx_val(g_spi_num, 0x95);
    } else if (cmd == CMD8) {
        hal_spi_tx_val(g_spi_num, 0x87);
    } else {
        /* Any value is OK */
        hal_spi_tx_val(g_spi_num, 0x01);
    }

    printf("=======================\n");
    printf("sending cmd %d\n", cmd & ~0x80);

    status = 0xff;
    n = 10;
    do {
        response[0] = (uint8_t) hal_spi_tx_val(g_spi_num, 0xff);
    } while ((response[0] & 0x80) && --n);

    if (n) {
        type = response_type_by_cmd(cmd);
        status = status_from_r1(response);
        printf("status=0x%x\n", status);
        if (!(type == R1 || status & R_ILLEGAL_COMMAND || status & R_CRC_ERROR)) {
            /* Read remaining data for this command */
            for (n = 0; n < 4; n++) {
                response[n+1] = (uint8_t) hal_spi_tx_val(g_spi_num, 0xff);
            }

            printf("response=[%0x][%0x][%0x][%0x]\n",
                    response[1], response[2], response[3], response[4]);

            switch (type) {
                case R3:
                    /* NOTE: ocr is defined in section 5.1 */
                    printf("ocr=0x%08lx\n", ocr_from_r3(response));
                    break;
                case R7:
                    printf("voltage=0x%x\n", voltage_from_r7(response));
                    break;
            }
        }
    }

    printf("\n");

    hal_gpio_write(g_ss_pin, 1);

    return status;
}

/**
 * NOTE:
 */

/**
 * Initialize the MMC driver
 *
 * @param spi_num Number of the SPI channel to be used by MMC
 * @param spi_cfg Low-level device specific SPI configuration
 * @param ss_pin Number of SS pin if SW controlled, -1 otherwise
 *
 * @return 0 on success, non-zero on failure
 */
int
mmc_init(int spi_num, void *spi_cfg, int ss_pin)
{
    int rc;
    int i;
    uint8_t status;
    uint32_t ocr;

    g_ss_pin = ss_pin;
    g_spi_num = spi_num;

    if (g_ss_pin != -1) {
        hal_gpio_init_out(g_ss_pin, 1);
    }

    rc = hal_spi_init(g_spi_num, spi_cfg, HAL_SPI_TYPE_MASTER);
    if (rc) {
        return (rc);
    }

    rc = hal_spi_config(g_spi_num, &mmc_settings);
    if (rc) {
        return (rc);
    }

    hal_spi_set_txrx_cb(0, NULL, NULL);
    hal_spi_enable(0);

    /**
     * NOTE: The state machine below follows:
     *       Section 6.4.1: Power Up Sequence for SD Bus Interface.
     *       Section 7.2.1: Mode Selection and Initialization.
     */

    /* give 10ms for VDD rampup */
    os_time_delay(OS_TICKS_PER_SEC / 100);

    /* send the required >= 74 clock cycles */
    hal_gpio_write(g_ss_pin, 0);

    for (i = 0; i < 74; i++) {
        hal_spi_tx_val(0, 0xff);
    }

    hal_gpio_write(g_ss_pin, 1);

    /* put card in idle state */
    status = send_mmc_cmd(CMD0, 0);

    if (status != R_IDLE) {
        /* No card inserted or bad card! */
        return mmc_error_by_status(status);
    }

    /**
     * 4.3.13: Ask for 2.7-3.3V range, send 0xAA pattern
     *
     * NOTE: cards that are not compliant with "Physical Spec Version 2.00"
     *       will answer this with R_ILLEGAL_COMMAND.
     */
    status = send_mmc_cmd(CMD8, 0x1AA);
    if (status & R_ILLEGAL_COMMAND) {
        /* Ver1.x SD Memory Card or Not SD Memory Card */

        ocr = send_mmc_cmd(CMD58, 0);

        /* TODO: check if voltage range is ok! */

        if (ocr & R_ILLEGAL_COMMAND) {

        }
    } else {
        /* Ver2.00 or later SD Memory Card */

        /* TODO:
         * 1) check CMD8 response pattern and voltage range.
         * 2) send ACMD41 while in R_IDLE up to 1s.
         * 3) send CMD58, check CCS in response.
         */

        status = send_mmc_cmd(ACMD41, 0);

        /* CCS = OCR[30] */
        status = send_mmc_cmd(CMD58, 0);
    }

    return rc;
}

/**
 * @return 0 on success, non-zero on failure
 */
int
mmc_read(void *buf, size_t len)
{
    hal_gpio_write(g_ss_pin, 0);
    hal_gpio_write(g_ss_pin, 1);
    return 0;
}

/**
 * @return 0 on success, non-zero on failure
 */
int
mmc_write(void *buf, size_t len)
{
    return 0;
}
