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

#include <xc.h>
#include "os/mynewt.h"
#include "bsp/bsp.h"
#include <hal/hal_gpio.h>
#include <hal/hal_i2c.h>
#include <mcu/p32mx470f512h.h>
#include "mcu/mips_hal.h"

#define I2CxCON(I)              (base_address[I][0x00 / 0x04])
#define I2CxCONCLR(I)           (base_address[I][0x04 / 0x04])
#define I2CxCONSET(I)           (base_address[I][0x08 / 0x04])
#define I2CxSTAT(I)             (base_address[I][0x10 / 0x04])
#define I2CxBRG(I)              (base_address[I][0x40 / 0x04])
#define I2CxTRN(I)              (base_address[I][0x50 / 0x04])
#define I2CxRCV(I)              (base_address[I][0x60 / 0x04])
#define WRITE_MODE              (0)
#define READ_MODE               (1)
#define PULSE_GOBBLER_DELAY     (104)   /* In nanoseconds */

static volatile uint32_t *base_address[I2C_CNT] = {
    (volatile uint32_t *)_I2C1_BASE_ADDRESS,
    (volatile uint32_t *)_I2C2_BASE_ADDRESS,
};

static int
send_byte(uint8_t i2c_num, uint8_t data, uint32_t deadline)
{
    I2CxTRN(i2c_num) = data;

    while (I2CxSTAT(i2c_num) & _I2C1STAT_TRSTAT_MASK) {
        if (os_time_get() > deadline) {
            return 0;
        }
    }

    if (I2CxSTAT(i2c_num) & _I2C1STAT_ACKSTAT_MASK) {   /* NACK received */
        return 0;
    }

    return 1;
}

static int
receive_byte(uint8_t i2c_num, uint8_t *data, uint8_t nak, uint32_t deadline)
{
    I2CxCONSET(i2c_num) = _I2C1CON_RCEN_MASK;

    /* Wait for some data in RX FIFO */
    while (!(I2CxSTAT(i2c_num) & _I2C1STAT_RBF_MASK)) {
        if (os_time_get() > deadline) {
            return 0;
        }
    }

    /* Send ACK/NAK */
    if (nak) {
        I2CxCONSET(i2c_num) = _I2C1CON_ACKDT_MASK;
    } else {
        I2CxCONCLR(i2c_num) = _I2C1CON_ACKDT_MASK;
    }

    I2CxCONSET(i2c_num) = _I2C1CON_ACKEN_MASK;
    while (I2CxCON(i2c_num) & _I2C1CON_ACKEN_MASK) {
        if (os_time_get() > deadline) {
            return 0;
        }
    }

    *data = I2CxRCV(i2c_num);
    return 1;
}

static int
send_address(uint8_t i2c_num, uint8_t address, uint8_t read_byte,
             uint32_t deadline)
{
    return send_byte(i2c_num, (address << 1) | (read_byte & 0x1), deadline);
}

static int
send_start(uint8_t i2c_num, uint32_t deadline)
{
    I2CxCONSET(i2c_num) = _I2C1CON_SEN_MASK;
    while (I2CxCON(i2c_num) & _I2C1CON_SEN_MASK) {
        if (os_time_get() > deadline) {
            return -1;
        }
    }

    return 0;
}

static int
send_stop(uint8_t i2c_num, uint32_t deadline)
{
    I2CxCONSET(i2c_num) = _I2C1CON_PEN_MASK;
    while (I2CxCON(i2c_num) & _I2C1CON_PEN_MASK) {
        if (os_time_get() > deadline) {
            return -1;
        }
    }

    return 0;
}

static uint32_t
hal_i2c_get_peripheral_clock(void)
{
    uint32_t divisor = 1;
    divisor <<= (OSCCON & _OSCCON_PBDIV_MASK) >> _OSCCON_PBDIV_POSITION;
    return MYNEWT_VAL(CLOCK_FREQ) / divisor;
}

int
hal_i2c_init(uint8_t i2c_num, void *cfg)
{
    struct mips_i2c_cfg *config;
    uint64_t baudrate = 0;

    if (cfg == NULL) {
        return -1;
    }

    config = (struct mips_i2c_cfg *)cfg;

    /* Configure SCL and SDA as digital output */
    if (hal_gpio_init_out(config->scl, 1) ||
        hal_gpio_init_out(config->sda, 1)) {
        return -1;
    }

    I2CxCON(i2c_num) = 0;

    /*
     * From PIC32 family reference manual,
     * Section 24. Inter-Integrated Circuit, Equation 24-1:
     *
     *               10^9
     *              -------  - PGD
     *              2*Fsck
     * baudrate = ----------------- * Pbclk - 2
     *                10^9
     */
    baudrate =
        (1000 * 1000 * 1000) / (2 * config->frequency) - PULSE_GOBBLER_DELAY;
    baudrate *= hal_i2c_get_peripheral_clock();
    baudrate /= (1000 * 1000 * 1000);
    baudrate -= 2;

    /* I2CxBRG must not be set to 0 or 1 */
    if (baudrate == 0 || baudrate == 1)
        return -2;

    I2CxBRG(i2c_num) = baudrate;
    I2CxCONSET(i2c_num) = _I2C1CON_SMEN_MASK;
    I2CxCONSET(i2c_num) = _I2C1CON_ON_MASK;

    return 0;
}

int
hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                     uint32_t timeout, uint8_t last_op)
{
    uint16_t byte_sent_count = 0;
    int ret = 0;
    uint32_t deadline = os_time_get() + timeout;

    if (send_start(i2c_num, deadline)) {
        ret = -1;
        goto hal_i2c_master_write_stop;
    }

    if (send_address(i2c_num, pdata->address, WRITE_MODE, deadline) != 1) {
        ret = -1;
        goto hal_i2c_master_write_stop;
    }

    while (byte_sent_count < pdata->len) {
        if (send_byte(i2c_num, pdata->buffer[byte_sent_count],
                      deadline) != 1) {
            ret = -1;
            goto hal_i2c_master_write_stop;
        }
        ++byte_sent_count;
    }

    if (!last_op) {
        return ret;
    }

hal_i2c_master_write_stop:
    if (send_stop(i2c_num, deadline)) {
        ret = -1;
    }

    return ret;
}

int
hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                    uint32_t timeout, uint8_t last_op)
{
    int ret = 0;
    uint16_t byte_received_count = 0;
    uint32_t deadline = os_time_get() + timeout;

    if (i2c_num >= I2C_CNT) {
        return -1;
    }

    if (send_start(i2c_num, deadline)) {
        ret = -1;
        goto hal_i2c_master_read_stop;
    }

    if (send_address(i2c_num, pdata->address, READ_MODE, deadline) != 1) {
        ret = -1;
        goto hal_i2c_master_read_stop;
    }

    while (byte_received_count < pdata->len) {
        if (receive_byte(i2c_num, &pdata->buffer[byte_received_count],
                         (byte_received_count + 1) == pdata->len,
                         deadline) != 1) {
            ret = -1;
            goto hal_i2c_master_read_stop;
        }
        ++byte_received_count;
    }

    if (!last_op) {
        return ret;
    }

hal_i2c_master_read_stop:
    if (send_stop(i2c_num, deadline)) {
        ret = -1;
    }

    return ret;
}

int
hal_i2c_master_probe(uint8_t i2c_num, uint8_t address,
                     uint32_t timeout)
{
    struct hal_i2c_master_data data;
    uint8_t buffer;

    data.address = address;
    data.buffer = NULL;
    data.len = 0;

    return hal_i2c_master_read(i2c_num, &data, timeout, 1);
}
