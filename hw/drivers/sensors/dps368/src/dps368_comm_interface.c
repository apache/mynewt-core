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

#include "dps368/dps368.h"
#include "dps368_priv.h"


/*DPS368 bus W/R handling */
#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
 * Writes a single byte to the specified register using i2c
 * interface
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
dps368_i2c_write_reg(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;
    uint8_t payload[2] =
        { reg, value };

    struct hal_i2c_master_data data_struct =
        { .address = itf->si_addr, .len = 2, .buffer = payload };

    rc = i2cn_master_write(itf->si_num, &data_struct,
            MYNEWT_VAL(DPS368_I2C_TIMEOUT_TICKS), 1,
            MYNEWT_VAL(DPS368_I2C_RETRIES));

    if (rc) {
        DPS368_LOG(ERROR,
                "Could not write to 0x%02X:0x%02X with value 0x%02X\n",
                itf->si_addr, reg, value);
    }

    return rc;
}

/**
 * Writes a single byte to the specified register using SPI
 * interface
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
dps368_spi_write_reg(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the register address w/write command */
    rc = hal_spi_tx_val(itf->si_num, reg & ~DPS368_SPI_READ_CMD_BIT);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        DPS368_LOG(ERROR, "SPI_%u register write failed addr:0x%02X\n",
                itf->si_num, reg);
        goto err;
    }

    /* Write data */
    rc = hal_spi_tx_val(itf->si_num, value);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        DPS368_LOG(ERROR, "SPI_%u write failed addr:0x%02X\n", itf->si_num,
                reg);
        goto err;
    }

    rc = 0;

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    os_time_delay((OS_TICKS_PER_SEC * 30) / 1000 + 1);

    return rc;
}


/**
 * Read bytes from the specified register using i2c interface
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param The number of bytes to read
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
dps368_i2c_read_regs(struct sensor_itf *itf, uint8_t reg, uint8_t size,
    uint8_t *buffer)
{
    int rc;
    struct hal_i2c_master_data wdata = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = &reg
    };
    struct hal_i2c_master_data rdata = {
        .address = itf->si_addr,
        .len = size,
        .buffer = buffer,
    };

    rc = i2cn_master_write_read_transact(itf->si_num, &wdata, &rdata,
            MYNEWT_VAL(DPS368_I2C_TIMEOUT_TICKS) * (size + 1), 1,
            MYNEWT_VAL(DPS368_I2C_RETRIES));
    if (rc) {
        DPS368_LOG(ERROR, "I2C access failed at address 0x%02X\n",
                itf->si_addr);
        return rc;
    }

    return rc;
}


/**
 *
 * Read bytes from the specified register using SPI interface
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param The number of bytes to read
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
dps368_spi_read_regs(struct sensor_itf *itf, uint8_t reg, uint8_t size,
    uint8_t *buffer)
{
    int i;
    uint16_t retval;
    int rc;
    rc = 0;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(itf->si_num, reg | DPS368_SPI_READ_CMD_BIT);
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        DPS368_LOG(ERROR, "SPI_%u register write failed addr:0x%02X\n",
                itf->si_num, reg);
        goto err;
    }

    for (i = 0; i < size; i++) {
        /* Read data */
        retval = hal_spi_tx_val(itf->si_num, 0);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            DPS368_LOG(ERROR, "SPI_%u read failed addr:0x%02X\n", itf->si_num,
                    reg);
            goto err;
        }
        buffer[i] = retval;
    }

    rc = 0;

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    return rc;
}

#endif

/**
 * Write a single register to DPS368 value over underlying communication interface
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 */
int dps368_write_reg(struct sensor_itf *itf, uint8_t addr, uint8_t value)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    uint8_t data[2] = { addr, value };

    rc = bus_node_simple_write(itf->si_dev, data, 2);
#else
    rc = sensor_itf_lock(itf, MYNEWT_VAL(DPS368_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = dps368_i2c_write_reg(itf, addr, value);
    } else {
        rc = dps368_spi_write_reg(itf, addr, value);
    }

    sensor_itf_unlock(itf);
#endif
    
    return rc;
}

/**
 * Read bytes from the specified register using specified interface
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param The number of bytes to read
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int dps368_read_regs(struct sensor_itf *itf, uint8_t addr, uint8_t *buff, uint8_t len)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct dps368 *dev = (struct dps368 *)itf->si_dev;

    if (dev->node_is_spi) {
        addr |= DPS368_SPI_READ_CMD_BIT;
    }

    rc = bus_node_simple_write_read_transact(itf->si_dev, &addr, 1, buff, len);
#else
    rc = sensor_itf_lock(itf, MYNEWT_VAL(DPS368_ITF_LOCK_TMO));

    if (rc) {
        return rc;
    }

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = dps368_i2c_read_regs(itf, addr, len, buff);
    } else {
        rc = dps368_spi_read_regs(itf, addr, len, buff);
    }

    sensor_itf_unlock(itf);
#endif

    return rc;
}


