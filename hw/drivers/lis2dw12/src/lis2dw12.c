/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * resarding copyright ownership.  The ASF licenses this file
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

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "os/mynewt.h"
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "lis2dw12/lis2dw12.h"
#include "lis2dw12_priv.h"
#include "hal/hal_gpio.h"
#include "modlog/modlog.h"
#include "stats/stats.h"
#include <syscfg/syscfg.h>

/*
 * Max time to wait for interrupt.
 */
#define LIS2DW12_MAX_INT_WAIT (4 * OS_TICKS_PER_SEC)

static struct hal_spi_settings spi_lis2dw12_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE3,
    .baudrate   = 4000,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};

/* Define the stats section and records */
STATS_SECT_START(lis2dw12_stat_section)
    STATS_SECT_ENTRY(write_errors)
    STATS_SECT_ENTRY(read_errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(lis2dw12_stat_section)
    STATS_NAME(lis2dw12_stat_section, write_errors)
    STATS_NAME(lis2dw12_stat_section, read_errors)
STATS_NAME_END(lis2dw12_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(lis2dw12_stat_section) g_lis2dw12stats;

#define LIS2DW12_LOG(lvl_, ...) \
    MODLOG_ ## lvl_(MYNEWT_VAL(LIS2DW12_LOG_MODULE), __VA_ARGS__)


/**
 * Write multiple length data to LIS2DW12 driver over I2C  (MAX: 19 bytes)
 *
 * @param The driver interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
static int
lis2dw12_i2c_writelen(struct driver_itf *itf, uint8_t addr, uint8_t *buffer,
                      uint8_t len)
{
    int rc;
    uint8_t payload[20] = { addr, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = len + 1,
        .buffer = payload
    };

    if (len > (sizeof(payload) - 1)) {
        rc = OS_EINVAL;
        goto err;
    }

    memcpy(&payload[1], buffer, len);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LIS2DW12_LOG(ERROR, "I2C access failed at address 0x%02X\n",
                     data_struct.address);
        STATS_INC(g_lis2dw12stats, write_errors);
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Write multiple length data to LIS2DW12 driver over SPI
 *
 * @param The driver interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
static int
lis2dw12_spi_writelen(struct driver_itf *itf, uint8_t addr, uint8_t *payload,
                      uint8_t len)
{
    int i;
    int rc;

    /*
     * Auto register address increment is needed if the length
     * requested is moret than 1
     */
    if (len > 1) {
        addr |= LIS2DW12_SPI_READ_CMD_BIT;
    }

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);


    /* Send the address */
    rc = hal_spi_tx_val(itf->si_num, addr);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        LIS2DW12_LOG(ERROR, "SPI_%u register write failed addr:0x%02X\n",
                     itf->si_num, addr);
        STATS_INC(g_lis2dw12stats, write_errors);
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        rc = hal_spi_tx_val(itf->si_num, payload[i]);
        if (rc == 0xFFFF) {
            rc = SYS_EINVAL;
            LIS2DW12_LOG(ERROR, "SPI_%u write failed addr:0x%02X:0x%02X\n",
                         itf->si_num, addr);
            STATS_INC(g_lis2dw12stats, write_errors);
            goto err;
        }
    }


    rc = 0;

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    return rc;
}

/**
 * Write multiple length data to LIS2DW12 driver over different interfaces
 *
 * @param The driver interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_writelen(struct driver_itf *itf, uint8_t addr, uint8_t *payload,
                  uint8_t len)
{
    int rc;

    rc = driver_itf_lock(itf, MYNEWT_VAL(LIS2DW12_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    if (itf->si_type == DRIVER_ITF_I2C) {
        rc = lis2dw12_i2c_writelen(itf, addr, payload, len);
    } else {
        rc = lis2dw12_spi_writelen(itf, addr, payload, len);
    }

    driver_itf_unlock(itf);

    return rc;
}

/**
 * Read multiple bytes starting from specified register over i2c
 *
 * @param The driver interface
 * @param The register address start reading from
 * @param Pointer to where the register value should be written
 * @param Number of bytes to read
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lis2dw12_i2c_readlen(struct driver_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len)
{
    int rc;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = &reg
    };

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LIS2DW12_LOG(ERROR, "I2C access failed at address 0x%02X\n",
                     itf->si_addr);
        STATS_INC(g_lis2dw12stats, write_errors);
        return rc;
    }

    /* Read data */
    data_struct.len = len;
    data_struct.buffer = buffer;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
        LIS2DW12_LOG(ERROR, "Failed to read from 0x%02X:0x%02X\n",
                     itf->si_addr, reg);
        STATS_INC(g_lis2dw12stats, read_errors);
    }

    return rc;
}

/**
 * Read multiple bytes starting from specified register over SPI
 *
 * @param The driver interface
 * @param The register address start reading from
 * @param Pointer to where the register value should be written
 * @param Number of bytes to read
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_spi_readlen(struct driver_itf *itf, uint8_t reg, uint8_t *buffer,
                    uint8_t len)
{
    int i;
    uint16_t retval;
    int rc = 0;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(itf->si_num, reg | LIS2DW12_SPI_READ_CMD_BIT);

    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        LIS2DW12_LOG(ERROR, "SPI_%u register write failed addr:0x%02X\n",
                     itf->si_num, reg);
        STATS_INC(g_lis2dw12stats, read_errors);
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(itf->si_num, 0);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            LIS2DW12_LOG(ERROR, "SPI_%u read failed addr:0x%02X\n",
                         itf->si_num, reg);
            STATS_INC(g_lis2dw12stats, read_errors);
            goto err;
        }
        buffer[i] = retval;
    }

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    return rc;
}


/**
 * Write byte to driver over different interfaces
 *
 * @param The driver interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_write8(struct driver_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;

    rc = driver_itf_lock(itf, MYNEWT_VAL(LIS2DW12_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    if (itf->si_type == DRIVER_ITF_I2C) {
        rc = lis2dw12_i2c_writelen(itf, reg, &value, 1);
    } else {
        rc = lis2dw12_spi_writelen(itf, reg, &value, 1);
    }

    driver_itf_unlock(itf);

    return rc;
}

/**
 * Read byte data from driver over different interfaces
 *
 * @param The driver interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_read8(struct driver_itf *itf, uint8_t reg, uint8_t *value)
{
    int rc;

    rc = driver_itf_lock(itf, MYNEWT_VAL(LIS2DW12_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    if (itf->si_type == DRIVER_ITF_I2C) {
        rc = lis2dw12_i2c_readlen(itf, reg, value, 1);
    } else {
        rc = lis2dw12_spi_readlen(itf, reg, value, 1);
    }

    driver_itf_unlock(itf);

    return rc;
}

/**
 * Read multiple bytes starting from specified register over different interfaces
 *
 * @param The driver interface
 * @param The register address start reading from
 * @param Pointer to where the register value should be written
 * @param Number of bytes to read
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_readlen(struct driver_itf *itf, uint8_t reg, uint8_t *buffer,
                uint8_t len)
{
    int rc;

    rc = driver_itf_lock(itf, MYNEWT_VAL(LIS2DW12_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    if (itf->si_type == DRIVER_ITF_I2C) {
        rc = lis2dw12_i2c_readlen(itf, reg, buffer, len);
    } else {
        rc = lis2dw12_spi_readlen(itf, reg, buffer, len);
    }

    driver_itf_unlock(itf);

    return rc;
}

/**
 * Calculates the acceleration in m/s^2 from mg
 *
 * @param acc value in mg
 * @param float ptr to return calculated value in ms2
 */
void
lis2dw12_calc_acc_ms2(int16_t acc_mg, float *acc_ms2)
{
    *acc_ms2 = (acc_mg * STANDARD_ACCEL_GRAVITY)/1000;
}

/**
 * Calculates the acceleration in mg from m/s^2
 *
 * @param acc value in m/s^2
 * @param int16 ptr to return calculated value in mg
 */
void
lis2dw12_calc_acc_mg(float acc_ms2, int16_t *acc_mg)
{
    *acc_mg = (acc_ms2 * 1000)/STANDARD_ACCEL_GRAVITY;
}

/**
 * Gets a new data sample from the sensor.
 *
 * @param The sensor interface
 * @param x axis data
 * @param y axis data
 * @param z axis data
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_data(struct driver_itf *itf, uint8_t fs, int16_t *x, int16_t *y, int16_t *z)
{
    int rc;
    uint8_t payload[6] = {0};

    *x = *y = *z = 0;

    rc = lis2dw12_readlen(itf, LIS2DW12_REG_OUT_X_L, payload, 6);
    if (rc) {
        goto err;
    }

    *x = payload[0] | (payload[1] << 8);
    *y = payload[2] | (payload[3] << 8);
    *z = payload[4] | (payload[5] << 8);

    /*
     * Since full scale is +/-(fs)g,
     * fs should be multiplied by 2 to account for full scale.
     * To calculate mg from g we use the 1000 multiple.
     * Since the full scale is represented by 16 bit value,
     * we use that as a divisor.
     */
    *x = (fs * 2 * 1000 * *x)/UINT16_MAX;
    *y = (fs * 2 * 1000 * *y)/UINT16_MAX;
    *z = (fs * 2 * 1000 * *z)/UINT16_MAX;

    return 0;
err:
    return rc;
}

/**
 * Reset lis2dw12
 *
 * @param The driver interface
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_reset(struct driver_itf *itf)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG2, &reg);
    if (rc) {
        goto err;
    }

    reg |= LIS2DW12_CTRL_REG2_SOFT_RESET | LIS2DW12_CTRL_REG2_BOOT;

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG2, reg);
    if (rc) {
        goto err;
    }

    os_time_delay((OS_TICKS_PER_SEC * 6/1000) + 1);

err:
    return rc;
}

/**
 * Get chip ID
 *
 * @param driver interface
 * @param ptr to chip id to be filled up
 */
int
lis2dw12_get_chip_id(struct driver_itf *itf, uint8_t *chip_id)
{
    uint8_t reg;
    int rc;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WHO_AM_I, &reg);

    if (rc) {
        goto err;
    }

    *chip_id = reg;

err:
    return rc;
}

/**
 * Sets the full scale selection
 *
 * @param The driver interface
 * @param The fs setting
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_full_scale(struct driver_itf *itf, uint8_t fs)
{
    int rc;
    uint8_t reg;

    if (fs > LIS2DW12_FS_16G) {
        LIS2DW12_LOG(ERROR, "Invalid full scale value\n");
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG6, &reg);
    if (rc) {
        goto err;
    }

    reg &= ~LIS2DW12_CTRL_REG6_FS;
    reg |= (fs & LIS2DW12_CTRL_REG6_FS);

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG6, reg);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets the full scale selection
 *
 * @param The driver interface
 * @param ptr to fs
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_full_scale(struct driver_itf *itf, uint8_t *fs)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG6, &reg);
    if (rc) {
        goto err;
    }

    *fs = reg & LIS2DW12_CTRL_REG6_FS;

    return 0;
err:
    return rc;
}

/**
 * Sets the rate
 *
 * @param The driver interface
 * @param The rate
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_rate(struct driver_itf *itf, uint8_t rate)
{
    int rc;
    uint8_t reg;

    if (rate > LIS2DW12_DATA_RATE_1600HZ) {
        LIS2DW12_LOG(ERROR, "Invalid rate value\n");
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG1, &reg);
    if (rc) {
        goto err;
    }

    reg &= ~LIS2DW12_CTRL_REG1_ODR;
    reg |= (rate & LIS2DW12_CTRL_REG1_ODR);

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG1, reg);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets the rate
 *
 * @param The driver ineterface
 * @param ptr to the rate
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_rate(struct driver_itf *itf, uint8_t *rate)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG1, &reg);
    if (rc) {
        goto err;
    }

    *rate = reg & LIS2DW12_CTRL_REG1_ODR;

    return 0;
err:
    return rc;
}

/**
 * Sets the low noise enable
 *
 * @param The driver interface
 * @param low noise enabled
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_low_noise(struct driver_itf *itf, uint8_t en)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG6, &reg);
    if (rc) {
        goto err;
    }

    if (en) {
        reg |= LIS2DW12_CTRL_REG6_LOW_NOISE;
    } else {
        reg &= ~LIS2DW12_CTRL_REG6_LOW_NOISE;
    }

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG6, reg);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets whether low noise is enabled
 *
 * @param The driver interface
 * @param ptr to low noise settings read from driver
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_low_noise(struct driver_itf *itf, uint8_t *en)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG6, &reg);
    if (rc) {
        goto err;
    }

    *en = (reg & LIS2DW12_CTRL_REG6_LOW_NOISE) > 0;

    return 0;
err:
    return rc;
}

/**
 * Sets the power mode of the driver
 *
 * @param The driver interface
 * @param power mode
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_power_mode(struct driver_itf *itf, uint8_t mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG1, &reg);
    if (rc) {
        goto err;
    }

    reg &= ~LIS2DW12_CTRL_REG1_MODE;
    reg &= ~LIS2DW12_CTRL_REG1_LP_MODE;
    reg |= mode;

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG1, reg);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets the power mode of the driver
 *
 * @param The driver interface
 * @param ptr to power mode setting read from driver
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_power_mode(struct driver_itf *itf, uint8_t *mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG1, &reg);
    if (rc) {
        goto err;
    }

    *mode = reg & (LIS2DW12_CTRL_REG1_MODE | LIS2DW12_CTRL_REG1_LP_MODE);

    return 0;
err:
    return rc;
}

/**
 * Sets the self test mode of the driver
 *
 * @param The driver interface
 * @param self test mode
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_self_test(struct driver_itf *itf, uint8_t mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG3, &reg);
    if (rc) {
        goto err;
    }

    reg &= ~LIS2DW12_CTRL_REG3_ST_MODE;
    reg |= (mode & LIS2DW12_CTRL_REG3_ST_MODE);

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG3, reg);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets the self test mode of the driver
 *
 * @param The driver interface
 * @param ptr to self test mode read from driver
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_self_test(struct driver_itf *itf, uint8_t *mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG3, &reg);
    if (rc) {
        goto err;
    }

    *mode = reg & LIS2DW12_CTRL_REG3_ST_MODE;

    return 0;
err:
    return rc;
}

/**
 * Sets the interrupt push-pull/open-drain selection
 *
 * @param The driver interface
 * @param interrupt setting (0 = push-pull, 1 = open-drain)
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_int_pp_od(struct driver_itf *itf, uint8_t mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG3, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DW12_CTRL_REG3_PP_OD;
    reg |= mode ? LIS2DW12_CTRL_REG3_PP_OD : 0;

    return lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG3, reg);
}

/**
 * Gets the interrupt push-pull/open-drain selection
 *
 * @param The driver interface
 * @param ptr to store setting (0 = push-pull, 1 = open-drain)
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_int_pp_od(struct driver_itf *itf, uint8_t *mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG3, &reg);
    if (rc) {
        return rc;
    }

    *mode = (reg & LIS2DW12_CTRL_REG3_PP_OD) ? 1 : 0;

    return 0;
}

/**
 * Sets whether latched interrupts are enabled
 *
 * @param The driver interface
 * @param value to set (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_latched_int(struct driver_itf *itf, uint8_t en)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG3, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DW12_CTRL_REG3_LIR;
    reg |= en ? LIS2DW12_CTRL_REG3_LIR : 0;

    return lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG3, reg);

}

/**
 * Gets whether latched interrupts are enabled
 *
 * @param The driver interface
 * @param ptr to store value (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_latched_int(struct driver_itf *itf, uint8_t *en)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG3, &reg);
    if (rc) {
        return rc;
    }

    *en = (reg & LIS2DW12_CTRL_REG3_LIR) ? 1 : 0;

    return 0;
}

/**
 * Sets whether interrupts are active high or low
 *
 * @param The driver interface
 * @param value to set (0 = active high, 1 = active low)
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_int_active_low(struct driver_itf *itf, uint8_t low)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG3, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DW12_CTRL_REG3_H_LACTIVE;
    reg |= low ? LIS2DW12_CTRL_REG3_H_LACTIVE : 0;

    return lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG3, reg);

}

/**
 * Gets whether interrupts are active high or low
 *
 * @param The driver interface
 * @param ptr to store value (0 = active high, 1 = active low)
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_int_active_low(struct driver_itf *itf, uint8_t *low)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG3, &reg);
    if (rc) {
        return rc;
    }

    *low = (reg & LIS2DW12_CTRL_REG3_H_LACTIVE) ? 1 : 0;

    return 0;

}

/**
 * Sets single data conversion mode
 *
 * @param The driver interface
 * @param value to set (0 = trigger on INT2 pin, 1 = trigger on write to SLP_MODE_1)
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_slp_mode(struct driver_itf *itf, uint8_t mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG3, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DW12_CTRL_REG3_SLP_MODE_SEL;
    reg |= mode ? LIS2DW12_CTRL_REG3_SLP_MODE_SEL : 0;

    return lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG3, reg);
}

/**
 * Gets single data conversion mode
 *
 * @param The driver interface
 * @param ptr to store value (0 = trigger on INT2 pin, 1 = trigger on write to SLP_MODE_1)
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_slp_mode(struct driver_itf *itf, uint8_t *mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG3, &reg);
    if (rc) {
        return rc;
    }

    *mode = (reg & LIS2DW12_CTRL_REG3_SLP_MODE_SEL) ? 1 : 0;

    return 0;
}

/**
 * Starts a data conversion in on demand mode
 *
 * @param The driver interface
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_start_on_demand_conversion(struct driver_itf *itf)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG3, &reg);
    if (rc) {
        return rc;
    }

    reg |= LIS2DW12_CTRL_REG3_SLP_MODE_1;

    return lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG3, reg);
}


/**
 * Set filter config
 *
 * @param The driver interface
 * @param the filter bandwidth
 * @param filter type (1 = high pass, 0 = low pass)
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_filter_cfg(struct driver_itf *itf, uint8_t bw, uint8_t type)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG6, &reg);
    if (rc) {
        goto err;
    }

    reg &= ~LIS2DW12_CTRL_REG6_BW_FILT;
    reg &= ~LIS2DW12_CTRL_REG6_FDS;
    reg |= (bw & 0x3) << 6;
    if (type) {
        reg |= LIS2DW12_CTRL_REG6_FDS;
    }

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG6, reg);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;

}

/**
 * Get filter config
 *
 * @param The driver interface
 * @param ptr to the filter bandwidth
 * @param ptr to filter type (1 = high pass, 0 = low pass)
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_filter_cfg(struct driver_itf *itf, uint8_t *bw, uint8_t *type)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG6, &reg);
    if (rc) {
        goto err;
    }

    *bw = (reg & LIS2DW12_CTRL_REG6_BW_FILT) >> 6;
    *type = (reg & LIS2DW12_CTRL_REG6_FDS) > 0;

    return 0;
err:
    return rc;
}

/**
 * Sets new offsets in driver
 *
 * @param The driver interface
 * @param X offset
 * @param Y offset
 * @param Z offset
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lis2dw12_set_offsets(struct driver_itf *itf, int8_t offset_x,
                     int8_t offset_y, int8_t offset_z, uint8_t weight)
{
    uint8_t reg;
    int rc;

    rc = lis2dw12_write8(itf, LIS2DW12_REG_X_OFS, offset_x);
    if (rc) {
        return rc;
    }

    rc = lis2dw12_write8(itf, LIS2DW12_REG_Y_OFS, offset_y);
    if (rc) {
        return rc;
    }

    rc = lis2dw12_write8(itf, LIS2DW12_REG_Z_OFS, offset_z);
    if (rc) {
        return rc;
    }

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG7, &reg);
    if (rc) {
        return rc;
    }

    if (weight) {
        reg |= LIS2DW12_CTRL_REG7_USR_OFF_W;
    } else {
        reg &= ~LIS2DW12_CTRL_REG7_USR_OFF_W;
    }

    return lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG7, reg);
}

/**
 * Gets the offsets currently set
 *
 * @param The driver interface
 * @param Pointer to location to store X offset
 * @param Pointer to location to store Y offset
 * @param Pointer to location to store Z offset
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lis2dw12_get_offsets(struct driver_itf *itf, int8_t *offset_x,
                     int8_t *offset_y, int8_t *offset_z, uint8_t *weight)
{
    uint8_t reg;
    int rc;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_X_OFS, (uint8_t *)offset_x);
    if (rc) {
        return rc;
    }

    rc = lis2dw12_read8(itf, LIS2DW12_REG_Y_OFS, (uint8_t *)offset_y);
    if (rc) {
        return rc;
    }

    rc = lis2dw12_read8(itf, LIS2DW12_REG_Z_OFS, (uint8_t *)offset_z);
    if (rc) {
        return rc;
    }

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG7, &reg);
    if (rc) {
        return rc;
    }

    *weight = reg & LIS2DW12_CTRL_REG7_USR_OFF_W ? 1 : 0;

    return 0;
}

/**
 * Sets whether offset are enabled (only work when low pass filtering is enabled)
 *
 * @param The driver interface
 * @param value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero error on failure.
 */
int lis2dw12_set_offset_enable(struct driver_itf *itf, uint8_t enabled)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG7, &reg);
    if (rc) {
        return rc;
    }

    if (enabled) {
        reg |= LIS2DW12_CTRL_REG7_USR_OFF_OUT;
    } else {
        reg &= ~LIS2DW12_CTRL_REG7_USR_OFF_OUT;
    }

    return lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG7, reg);
}


/**
 * Set tap detection configuration
 *
 * @param The driver interface
 * @param the tap settings
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_tap_cfg(struct driver_itf *itf, struct lis2dw12_tap_settings *cfg)
{
    int rc;
    uint8_t reg;

    reg = cfg->en_4d ? LIS2DW12_TAP_THS_X_4D_EN : 0;
    reg |= (cfg->ths_6d & 0x3) << 5;
    reg |= cfg->tap_ths_x & LIS2DW12_TAP_THS_X_THS;

    rc = lis2dw12_write8(itf, LIS2DW12_REG_TAP_THS_X, reg);
    if (rc) {
        return rc;
    }

    reg = 0;
    reg |= (cfg->tap_priority & 0x7) << 5;
    reg |= cfg->tap_ths_y & LIS2DW12_TAP_THS_Y_THS;

    rc = lis2dw12_write8(itf, LIS2DW12_REG_TAP_THS_Y, reg);
    if (rc) {
        return rc;
    }

    reg = 0;
    reg |= cfg->en_x ? LIS2DW12_TAP_THS_Z_X_EN : 0;
    reg |= cfg->en_y ? LIS2DW12_TAP_THS_Z_Y_EN : 0;
    reg |= cfg->en_z ? LIS2DW12_TAP_THS_Z_Z_EN : 0;
    reg |= cfg->tap_ths_z & LIS2DW12_TAP_THS_Z_THS;

    rc = lis2dw12_write8(itf, LIS2DW12_REG_TAP_THS_Z, reg);
    if (rc) {
        return rc;
    }

    reg = 0;
    reg |= (cfg->latency & 0xf) << 4;
    reg |= (cfg->quiet & 0x3) << 2;
    reg |= cfg->shock & LIS2DW12_INT_DUR_SHOCK;

    return lis2dw12_write8(itf, LIS2DW12_REG_INT_DUR, reg);
}

/**
 * Get tap detection config
 *
 * @param The driver interface
 * @param ptr to the tap settings
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_tap_cfg(struct driver_itf *itf, struct lis2dw12_tap_settings *cfg)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_TAP_THS_X, &reg);
    if (rc) {
        return rc;
    }

    cfg->en_4d = (reg & LIS2DW12_TAP_THS_X_4D_EN) > 0;
    cfg->ths_6d = (reg & LIS2DW12_TAP_THS_X_6D_THS) >> 5;
    cfg->tap_ths_x = reg & LIS2DW12_TAP_THS_X_THS;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_TAP_THS_Y, &reg);
    if (rc) {
        return rc;
    }

    cfg->tap_priority = (reg & LIS2DW12_TAP_THS_Y_PRIOR) >> 5;
    cfg->tap_ths_y = reg & LIS2DW12_TAP_THS_Y_THS;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_TAP_THS_Z, &reg);
    if (rc) {
        return rc;
    }

    cfg->en_x = (reg & LIS2DW12_TAP_THS_Z_X_EN) > 0;
    cfg->en_y = (reg & LIS2DW12_TAP_THS_Z_Y_EN) > 0;
    cfg->en_z = (reg & LIS2DW12_TAP_THS_Z_Z_EN) > 0;
    cfg->tap_ths_z = reg & LIS2DW12_TAP_THS_Z_THS;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_INT_DUR, &reg);
    if (rc) {
        return rc;
    }

    cfg->latency = (reg & LIS2DW12_INT_DUR_LATENCY) >> 4;
    cfg->quiet = (reg & LIS2DW12_INT_DUR_QUIET) >> 2;
    cfg->shock = reg & LIS2DW12_INT_DUR_SHOCK;

    return 0;
}

/**
 * Set freefall detection configuration
 *
 * @param The driver interface
 * @param freefall duration (6 bits LSB = 1/ODR)
 * @param freefall threshold (3 bits)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_freefall(struct driver_itf *itf, uint8_t dur, uint8_t ths)
{
    int rc;
    uint8_t reg;

    reg = 0;
    reg |= (dur & 0x1F) << 3;
    reg |= ths & LIS2DW12_FREEFALL_THS;

    rc = lis2dw12_write8(itf, LIS2DW12_REG_FREEFALL, reg);
    if (rc) {
        return rc;
    }

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_DUR, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DW12_WAKE_DUR_FF_DUR;
    reg |= dur & 0x20 ? LIS2DW12_WAKE_DUR_FF_DUR : 0;

    return lis2dw12_write8(itf, LIS2DW12_REG_WAKE_UP_DUR, reg);
}

/**
 * Get freefall detection config
 *
 * @param The driver interface
 * @param ptr to freefall duration
 * @param ptr to freefall threshold
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_freefall(struct driver_itf *itf, uint8_t *dur, uint8_t *ths)
{
    int rc;
    uint8_t ff_reg, wake_reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_FREEFALL, &ff_reg);
    if (rc) {
        return rc;
    }

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_DUR, &wake_reg);
    if (rc) {
        return rc;
    }

    *dur = (ff_reg & LIS2DW12_FREEFALL_DUR) >> 3;
    *dur |= wake_reg & LIS2DW12_WAKE_DUR_FF_DUR ? (1 << 5) : 0;
    *ths = ff_reg & LIS2DW12_FREEFALL_THS;

    return 0;
}

/**
 * Setup FIFO
 *
 * @param The driver interface
 * @param FIFO mode to setup
 * @param Threshold to set for FIFO
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_fifo_cfg(struct driver_itf *itf, enum lis2dw12_fifo_mode mode, uint8_t fifo_ths)
{
    uint8_t reg = 0;

    reg |= fifo_ths & LIS2DW12_FIFO_CTRL_FTH;
    reg |= (mode & 0x7) << 5;

    return lis2dw12_write8(itf, LIS2DW12_REG_FIFO_CTRL, reg);
}

/**
 * Get Number of Samples in FIFO
 *
 * @param The driver interface
 * @param Pointer to return number of samples in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_fifo_samples(struct driver_itf *itf, uint8_t *samples)
{
    uint8_t reg;
    int rc;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_FIFO_SAMPLES, &reg);
    if (rc) {
        return rc;
    }

    *samples = reg & LIS2DW12_FIFO_SAMPLES;

    return 0;
}

/**
 * Clear interrupt pin configuration for interrupt 1
 *
 * @param itf The driver interface
 * @param cfg int1 config
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_clear_int1_pin_cfg(struct driver_itf *itf, uint8_t cfg)
{
    int rc;
    uint8_t reg;

    reg = 0;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG4, &reg);
    if (rc) {
        goto err;
    }

    reg &= ~cfg;

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG4, reg);

err:
    return rc;
}

/**
 * Clear interrupt pin configuration for interrupt 2
 *
 * @param itf The driver interface
 * @param cfg int2 config
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_clear_int2_pin_cfg(struct driver_itf *itf, uint8_t cfg)
{
    int rc;
    uint8_t reg;

    reg = 0;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG5, &reg);
    if (rc) {
        goto err;
    }

    reg &= ~cfg;

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG5, reg);

err:
    return rc;
}



/**
 * Set interrupt pin configuration for interrupt 1
 *
 * @param The driver interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_int1_pin_cfg(struct driver_itf *itf, uint8_t cfg)
{
    int rc;
    uint8_t reg;

    reg = 0;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG4, &reg);
    if (rc) {
        goto err;
    }

    reg |= cfg;

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG4, reg);

err:
    return rc;
}

/**
 * Set interrupt pin configuration for interrupt 2
 *
 * @param The driver interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_int2_pin_cfg(struct driver_itf *itf, uint8_t cfg)
{
    int rc;
    uint8_t reg;

    reg = 0;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG5, &reg);
    if (rc) {
       goto err;
    }

    reg |= cfg;

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG5, reg);

err:
    return rc;
}

/**
 * Set Wake Up Threshold configuration
 *
 * @param The driver interface
 * @param wake_up_ths value to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_wake_up_ths(struct driver_itf *itf, uint8_t val)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_THS, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DW12_WAKE_THS_THS;
    reg |= val & LIS2DW12_WAKE_THS_THS;

    return lis2dw12_write8(itf, LIS2DW12_REG_WAKE_UP_THS, reg);
}

/**
 * Get Wake Up Threshold config
 *
 * @param The driver interface
 * @param ptr to store wake_up_ths value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_ths(struct driver_itf *itf, uint8_t *val)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_THS, &reg);
    if (rc) {
        return rc;
    }

    *val = reg & LIS2DW12_WAKE_THS_THS;
    return 0;
}

/**
 * Set whether sleep on inactivity is enabled
 *
 * @param The driver interface
 * @param value to set (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_inactivity_sleep_en(struct driver_itf *itf, uint8_t en)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_THS, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DW12_WAKE_THS_SLEEP_ON;
    reg |= en ? LIS2DW12_WAKE_THS_SLEEP_ON : 0;

    return lis2dw12_write8(itf, LIS2DW12_REG_WAKE_UP_THS, reg);
}

/**
 * Get whether sleep on inactivity is enabled
 *
 * @param The driver interface
 * @param ptr to store read state (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_inactivity_sleep_en(struct driver_itf *itf, uint8_t *en)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_THS, &reg);
    if (rc) {
        return rc;
    }

    *en = (reg & LIS2DW12_WAKE_THS_SLEEP_ON) ? 1 : 0;
    return 0;

}

/**
 * Set whether double tap event is enabled
 *
 * @param The driver interface
 * @param value to set (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_double_tap_event_en(struct driver_itf *itf, uint8_t en)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_THS, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DW12_WAKE_THS_SINGLE_DOUBLE_TAP;
    reg |= en ? LIS2DW12_WAKE_THS_SINGLE_DOUBLE_TAP : en;

    return lis2dw12_write8(itf, LIS2DW12_REG_WAKE_UP_THS, reg);
}

/**
 * Get whether double tap event is enabled
 *
 * @param The driver interface
 * @param ptr to store read state (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_double_tap_event_en(struct driver_itf *itf, uint8_t *en)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_THS, &reg);
    if (rc) {
        return rc;
    }

    *en = (reg & LIS2DW12_WAKE_THS_SINGLE_DOUBLE_TAP) ? 1 : 0;
    return 0;

}

/**
 * Set Wake Up Duration
 *
 * @param The driver interface
 * @param value to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_wake_up_dur(struct driver_itf *itf, uint8_t val)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_DUR, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DW12_WAKE_DUR_DUR;
    reg |= (val & LIS2DW12_WAKE_DUR_DUR) << 5;

    return lis2dw12_write8(itf, LIS2DW12_REG_WAKE_UP_DUR, reg);

}

/**
 * Get Wake Up Duration
 *
 * @param The driver interface
 * @param ptr to store wake_up_dur value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_dur(struct driver_itf *itf, uint8_t *val)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_DUR, &reg);
    if (rc) {
        return rc;
    }

    *val = (reg & LIS2DW12_WAKE_DUR_DUR) >> 5;
    return 0;
}

/**
 * Set Sleep Duration
 *
 * @param The driver interface
 * @param value to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_sleep_dur(struct driver_itf *itf, uint8_t val)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_DUR, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DW12_WAKE_DUR_SLEEP_DUR;
    reg |= (val & LIS2DW12_WAKE_DUR_SLEEP_DUR);

    return lis2dw12_write8(itf, LIS2DW12_REG_WAKE_UP_DUR, reg);
}

/**
 * Get Sleep Duration
 *
 * @param The driver interface
 * @param ptr to store sleep_dur value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_sleep_dur(struct driver_itf *itf, uint8_t *val)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_DUR, &reg);
    if (rc) {
        return rc;
    }

    *val = reg & LIS2DW12_WAKE_DUR_SLEEP_DUR;
    return 0;
}

/**
 * Set Stationary Detection Enable
 *
 * @param The driver interface
 * @param value to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_stationary_en(struct driver_itf *itf, uint8_t en)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_DUR, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DW12_WAKE_DUR_STATIONARY;
    reg |= en ? LIS2DW12_WAKE_DUR_STATIONARY : 0;

    return lis2dw12_write8(itf, LIS2DW12_REG_WAKE_UP_DUR, reg);

}

/**
 * Get Stationary Detection Enable
 *
 * @param The driver interface
 * @param ptr to store sleep_dur value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_stationary_en(struct driver_itf *itf, uint8_t *en)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_DUR, &reg);
    if (rc) {
        return rc;
    }

    *en = (reg & LIS2DW12_WAKE_DUR_STATIONARY) ? 1 : 0;
    return 0;
}

/**
 * Clear interrupt 1
 *
 * @param The driver interface
 */
int
lis2dw12_clear_int(struct driver_itf *itf, uint8_t *src)
{
    return lis2dw12_read8(itf, LIS2DW12_REG_INT_SRC, src);
}

/**
 * Get Interrupt Status
 *
 * @param The driver interface
 * @param pointer to return interrupt status in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_int_status(struct driver_itf *itf, uint8_t *status)
{
    return lis2dw12_read8(itf, LIS2DW12_REG_STATUS_REG, status);
}

/**
 * Get Wake Up Source
 *
 * @param The driver interface
 * @param pointer to return wake_up_src in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_src(struct driver_itf *itf, uint8_t *status)
{
    return lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_SRC, status);
}

/**
 * Get Tap Source
 *
 * @param The driver interface
 * @param pointer to return tap_src in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_tap_src(struct driver_itf *itf, uint8_t *status)
{
    return lis2dw12_read8(itf, LIS2DW12_REG_TAP_SRC, status);
}

/**
 * Get 6D Source
 *
 * @param The driver interface
 * @param pointer to return sixd_src in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_sixd_src(struct driver_itf *itf, uint8_t *status)
{
    return lis2dw12_read8(itf, LIS2DW12_REG_SIXD_SRC, status);
}


/**
 * Set whether interrupts are enabled
 *
 * @param The driver interface
 * @param value to set (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_int_enable(struct driver_itf *itf, uint8_t enabled)
{
    uint8_t reg;
    int rc;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG7, &reg);
    if (rc) {
        return rc;
    }

    if (enabled) {
        reg |= LIS2DW12_CTRL_REG7_INT_EN;
    } else {
        reg &= ~LIS2DW12_CTRL_REG7_INT_EN;
    }

    return lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG7, reg);
}

/**
 * Set whether interrupt 2 signals is mapped onto interrupt 1 pin
 *
 * @param The driver interface
 * @param value to set (false = disabled, true = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_int2_on_int1_map(struct driver_itf *itf, bool enable)
{
    uint8_t reg;
    int rc;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG7, &reg);
    if (rc) {
        return rc;
    }

    if (enable) {
        reg |= LIS2DW12_CTRL_REG7_INT2_ON_INT1;
    } else {
        reg &= ~LIS2DW12_CTRL_REG7_INT2_ON_INT1;
    }

    return lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG7, reg);
}

/**
 * Get whether interrupt 1 signals is mapped onto interrupt 2 pin
 *
 * @param The driver interface
 * @param value to set (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_int1_on_int2_map(struct driver_itf *itf, uint8_t *val)
{
    uint8_t reg;
    int rc;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG7, &reg);
    if (rc) {
        return rc;
    }

    *val = (reg & LIS2DW12_CTRL_REG7_INT2_ON_INT1) >> 6;
    return 0;
}

/**
 * Run Self test on driver
 *
 * @param The driver interface
 * @param pointer to return test result in (0 on pass, non-zero on failure)
 *
 * @return 0 on sucess, non-zero on failure
 */
int lis2dw12_run_self_test(struct driver_itf *itf, int *result)
{
    int rc;
    /*configure min and max values for reading 5 samples, and accounting for
     * both negative and positive offset */
    int min = LIS2DW12_ST_MIN*5*2;
    int max = LIS2DW12_ST_MAX*5*2;

    int16_t data[3], diff[3] = {0,0,0};
    int i;
    uint8_t prev_config[6];
    /* set config per datasheet, with positive self test mode enabled. */
    uint8_t st_config[] = {0x44, 0x04, 0x40, 0x00, 0x00, 0x10};

    rc = lis2dw12_readlen(itf, LIS2DW12_REG_CTRL_REG1, prev_config, 6);
    if (rc) {
        return rc;
    }
    rc = lis2dw12_writelen(itf, LIS2DW12_REG_CTRL_REG2, &st_config[1], 5);
    rc = lis2dw12_writelen(itf, LIS2DW12_REG_CTRL_REG1, st_config, 1);
    if (rc) {
        return rc;
    }

    /* go into self test mode 1 */
    rc = lis2dw12_set_self_test(itf, LIS2DW12_ST_MODE_MODE1);
    if (rc) {
        return rc;
    }

    /* wait 100ms */
    os_time_delay(OS_TICKS_PER_SEC / 100);
    rc = lis2dw12_get_data(itf, 2, &(data[0]), &(data[1]), &(data[2]));
    if (rc) {
        return rc;
    }

    /* take positive offset reading */
    for(i=0; i<5; i++) {

        rc = lis2dw12_get_data(itf, 2, &(data[0]), &(data[1]), &(data[2]));
        if (rc) {
            return rc;
        }
        diff[0] += data[0];
        diff[1] += data[1];
        diff[2] += data[2];
        /* wait at least 20 ms */
        os_time_delay(OS_TICKS_PER_SEC / 50 + 1);
    }

    /* go into self test mode 2 */
    rc = lis2dw12_set_self_test(itf, LIS2DW12_ST_MODE_MODE2);
    if (rc) {
        return rc;
    }

    os_time_delay(OS_TICKS_PER_SEC / 50 + 1);
    rc = lis2dw12_get_data(itf, 2, &(data[0]), &(data[1]), &(data[2]));
    if (rc) {
        return rc;
    }

    /* take negative offset reading */
    for (i=0; i<5; i++) {

            rc = lis2dw12_get_data(itf, 2, &(data[0]), &(data[1]), &(data[2]));
            if (rc) {
                return rc;
            }
            diff[0] -= data[0];
            diff[1] -= data[1];
            diff[2] -= data[2];
            /* wait at least 20 ms */
            os_time_delay(OS_TICKS_PER_SEC / 50 + 1);
        }

    /* disable self test mod */
    rc = lis2dw12_writelen(itf, LIS2DW12_REG_CTRL_REG1, prev_config, 6);
        if (rc) {
            return rc;
        }

    /* compare values to thresholds */
    *result = 0;
    for (i = 0; i < 3; i++) {
        if ((diff[i] < min) || (diff[i] > max)) {
            *result -= 1;
        }
    }

    return 0;
}

void
init_interrupt(struct lis2dw12_int *interrupt, struct driver_int *ints)
{
    os_error_t error;

    error = os_sem_init(&interrupt->wait, 0);
    assert(error == OS_OK);

    interrupt->active = false;
    interrupt->asleep = false;
    interrupt->ints = ints;
}

void
undo_interrupt(struct lis2dw12_int * interrupt)
{
    OS_ENTER_CRITICAL(interrupt->lock);
    interrupt->active = false;
    interrupt->asleep = false;
    OS_EXIT_CRITICAL(interrupt->lock);
}

int
wait_interrupt(struct lis2dw12_int *interrupt, uint8_t int_num)
{
    bool wait;
    os_error_t error;

    OS_ENTER_CRITICAL(interrupt->lock);

    /* Check if we did not missed interrupt */
    if (hal_gpio_read(interrupt->ints[int_num].host_pin) ==
                                            interrupt->ints[int_num].active) {
        OS_EXIT_CRITICAL(interrupt->lock);
        return OS_OK;
    }

    if (interrupt->active) {
        interrupt->active = false;
        wait = false;
    } else {
        interrupt->asleep = true;
        wait = true;
    }
    OS_EXIT_CRITICAL(interrupt->lock);

    if (wait) {
        error = os_sem_pend(&interrupt->wait, LIS2DW12_MAX_INT_WAIT);
        if (error == OS_TIMEOUT) {
            return error;
        }
        assert(error == OS_OK);
    }
    return OS_OK;
}

static void
wake_interrupt(struct lis2dw12_int *interrupt)
{
    bool wake;

    OS_ENTER_CRITICAL(interrupt->lock);
    if (interrupt->asleep) {
        interrupt->asleep = false;
        wake = true;
    } else {
        interrupt->active = true;
        wake = false;
    }
    OS_EXIT_CRITICAL(interrupt->lock);

    if (wake) {
        os_error_t error;

        error = os_sem_release(&interrupt->wait);
        assert(error == OS_OK);
    }
}

static void
lis2dw12_int_irq_handler(void *arg)
{
    struct lis2dw12 *lis2dw12 = arg;

    if(lis2dw12->pdd.interrupt) {
        wake_interrupt(lis2dw12->pdd.interrupt);
    }

    // sensor_mgr_put_interrupt_evt(sensor);
}

static int
init_intpin(struct lis2dw12 *lis2dw12, hal_gpio_irq_handler_t handler)
{
    hal_gpio_irq_trig_t trig;
    int pin = -1;
    int rc;
    int i;

    for (i = 0; i < MYNEWT_VAL(DRIVER_MAX_INTERRUPTS_PINS); i++){
        pin = lis2dw12->itf.si_ints[i].host_pin;
        if (pin >= 0) {
            break;
        }
    }

    if (pin < 0) {
        LIS2DW12_LOG(ERROR, "Interrupt pin not configured\n");
        return SYS_EINVAL;
    }

    if (lis2dw12->itf.si_ints[i].active) {
        trig = HAL_GPIO_TRIG_RISING;
    } else {
        trig = HAL_GPIO_TRIG_FALLING;
    }

    rc = hal_gpio_irq_init(pin,
                           handler,
                           lis2dw12,
                           trig,
                           HAL_GPIO_PULL_NONE);
    if (rc != 0) {
        LIS2DW12_LOG(ERROR, "Failed to initialise interrupt pin %d\n", pin);
        return rc;
    }

    return 0;
}

int
disable_interrupt(struct lis2dw12 *lis2dw12, uint8_t int_to_disable, uint8_t int_num)
{
    struct lis2dw12_pdd *pdd;
    struct driver_itf *itf;
    int rc;

    if (int_to_disable == 0) {
        return SYS_EINVAL;
    }

    itf = &lis2dw12->itf;
    pdd = &lis2dw12->pdd;

    pdd->int_enable &= ~(int_to_disable << (int_num * 8));

    /* disable int pin */
    if (!pdd->int_enable) {
        hal_gpio_irq_disable(itf->si_ints[int_num].host_pin);
        /* disable interrupt in device */
        rc = lis2dw12_set_int_enable(itf, 0);
        if (rc) {
            pdd->int_enable |= (int_to_disable << (int_num * 8));
            return rc;
        }
    }

    /* update interrupt setup in device */
    if (int_num == 0) {
        rc = lis2dw12_clear_int1_pin_cfg(itf, int_to_disable);
    } else {
        rc = lis2dw12_clear_int2_pin_cfg(itf, int_to_disable);
    }

    return rc;
}


int
enable_interrupt(struct lis2dw12 *lis2dw12, uint8_t int_to_enable, uint8_t int_num)
{
    struct lis2dw12_pdd *pdd;
    struct driver_itf *itf;
    uint8_t reg;
    int rc;

    if (!int_to_enable) {
        rc = SYS_EINVAL;
        goto err;
    }

    itf = &lis2dw12->itf;
    pdd = &lis2dw12->pdd;

    rc = lis2dw12_clear_int(itf, &reg);
    if (rc) {
        goto err;
    }

    /* if no interrupts are currently in use enable int pin */
    if (!pdd->int_enable) {
        hal_gpio_irq_enable(itf->si_ints[int_num].host_pin);

        rc = lis2dw12_set_int_enable(itf, 1);
        if (rc) {
            goto err;
        }
    }

    pdd->int_enable |= (int_to_enable << (int_num * 8));

    /* enable interrupt in device */
    if (int_num == 0) {
        rc = lis2dw12_set_int1_pin_cfg(itf, int_to_enable);
    } else {
        rc = lis2dw12_set_int2_pin_cfg(itf, int_to_enable);
    }

    if (rc) {
        disable_interrupt(lis2dw12, int_to_enable, int_num);
        goto err;
    }

    return 0;
err:
    return rc;
}

int
lis2dw12_get_fs(struct driver_itf *itf, uint8_t *fs)
{
    int rc;

    rc = lis2dw12_get_full_scale(itf, fs);
    if (rc) {
        return rc;
    }

    if (*fs == LIS2DW12_FS_2G) {
        *fs = 2;
    } else if (*fs == LIS2DW12_FS_4G) {
        *fs = 4;
    } else if (*fs == LIS2DW12_FS_8G) {
        *fs = 8;
    } else if (*fs == LIS2DW12_FS_16G) {
        *fs = 16;
    } else {
        return SYS_EINVAL;
    }

    return 0;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this accelerometer
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lis2dw12_init(struct os_dev *dev, void *arg)
{
    int rc;
    struct lis2dw12 *lis2dw12;
    // struct driver_itf *itf;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    lis2dw12 = (struct lis2dw12 *) dev;
    // itf = (struct driver_itf *) arg;

    // todo.. store itf in lis2dw12
    // lis2dw12->itf = itf;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lis2dw12stats),
        STATS_SIZE_INIT_PARMS(g_lis2dw12stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lis2dw12_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_lis2dw12stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    if (lis2dw12->itf.si_type == DRIVER_ITF_SPI) {

        rc = hal_spi_disable(lis2dw12->itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_spi_config(lis2dw12->itf.si_num, &spi_lis2dw12_settings);
        if (rc == EINVAL) {
            /* If spi is already enabled, for nrf52, it returns -1, We should not
             * fail if the spi is already enabled
             */
            goto err;
        }

        rc = hal_spi_enable(lis2dw12->itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_gpio_init_out(lis2dw12->itf.si_cs_pin, 1);
        if (rc) {
            goto err;
        }
    }

    init_interrupt(&lis2dw12->intr, lis2dw12->itf.si_ints);

    lis2dw12->pdd.interrupt = NULL;

    rc = init_intpin(lis2dw12, lis2dw12_int_irq_handler);
    if (rc) {
        return rc;
    }

    return 0;
err:
    return rc;
}

/**
 * Configure the driver
 *
 * @param ptr to device
 * @param ptr to device config
 */
int
lis2dw12_config(struct lis2dw12 *lis2dw12, struct lis2dw12_cfg *cfg)
{
    int rc;
    uint8_t chip_id;
    struct driver_itf *itf;

    itf = &lis2dw12->itf;

    rc = lis2dw12_get_chip_id(itf, &chip_id);
    if (rc) {
        goto err;
    }

    if (chip_id != LIS2DW12_ID) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lis2dw12_reset(itf);
    if (rc) {
        goto err;
    }

    rc = lis2dw12_set_int_pp_od(itf, cfg->int_pp_od);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.int_pp_od = cfg->int_pp_od;

    rc = lis2dw12_set_latched_int(itf, cfg->int_latched);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.int_latched = cfg->int_latched;

    rc = lis2dw12_set_int_active_low(itf, cfg->int_active_low);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.int_active_low = cfg->int_active_low;

    rc = lis2dw12_set_slp_mode(itf, cfg->slp_mode);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.slp_mode = cfg->slp_mode;

    rc = lis2dw12_set_offsets(itf, cfg->offset_x, cfg->offset_y, cfg->offset_z,
                              cfg->offset_weight);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.offset_x = cfg->offset_x;
    lis2dw12->cfg.offset_y = cfg->offset_y;
    lis2dw12->cfg.offset_z = cfg->offset_z;
    lis2dw12->cfg.offset_weight = cfg->offset_weight;

    rc = lis2dw12_set_offset_enable(itf, cfg->offset_en);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.offset_en = cfg->offset_en;

    rc = lis2dw12_set_filter_cfg(itf, cfg->filter_bw, cfg->high_pass);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.filter_bw = cfg->filter_bw;
    lis2dw12->cfg.high_pass = cfg->high_pass;

    rc = lis2dw12_set_full_scale(itf, cfg->fs);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.fs = cfg->fs;

    rc = lis2dw12_set_rate(itf, cfg->rate);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.rate = cfg->rate;

    rc = lis2dw12_set_power_mode(itf, cfg->power_mode);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.power_mode = cfg->power_mode;

    rc = lis2dw12_set_low_noise(itf, cfg->low_noise_enable);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.low_noise_enable = cfg->low_noise_enable;

    rc = lis2dw12_set_fifo_cfg(itf, cfg->fifo_mode, cfg->fifo_threshold);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.fifo_mode = cfg->fifo_mode;
    lis2dw12->cfg.fifo_threshold = cfg->fifo_threshold;

    rc = lis2dw12_set_wake_up_ths(itf, cfg->wake_up_ths);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.wake_up_ths = cfg->wake_up_ths;

    rc = lis2dw12_set_wake_up_dur(itf, cfg->wake_up_dur);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.wake_up_dur = cfg->wake_up_dur;

    rc = lis2dw12_set_sleep_dur(itf, cfg->sleep_duration);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.sleep_duration = cfg->sleep_duration;

    rc = lis2dw12_set_stationary_en(itf, cfg->stationary_detection_enable);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.stationary_detection_enable = cfg->stationary_detection_enable;

    rc = lis2dw12_set_inactivity_sleep_en(itf, cfg->inactivity_sleep_enable);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.inactivity_sleep_enable = cfg->inactivity_sleep_enable;

    rc = lis2dw12_set_double_tap_event_en(itf, cfg->double_tap_event_enable);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.double_tap_event_enable = cfg->double_tap_event_enable;

    rc = lis2dw12_set_freefall(itf, cfg->freefall_dur, cfg->freefall_ths);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.freefall_dur = cfg->freefall_dur;
    lis2dw12->cfg.freefall_ths = cfg->freefall_ths;


    rc = lis2dw12_set_int_enable(itf, cfg->int_enable);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.int_enable = cfg->int_enable;

    rc = lis2dw12_set_int1_pin_cfg(itf, cfg->int1_pin_cfg);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.int1_pin_cfg = cfg->int1_pin_cfg;

    rc = lis2dw12_set_int2_pin_cfg(itf, cfg->int2_pin_cfg);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.int2_pin_cfg = cfg->int2_pin_cfg;

    rc = lis2dw12_set_tap_cfg(itf, &cfg->tap);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.tap = cfg->tap;

    rc = lis2dw12_set_int2_on_int1_map(itf, cfg->map_int2_to_int1);
    if(rc) {
        goto err;
    }
    lis2dw12->cfg.map_int2_to_int1 = cfg->map_int2_to_int1;

    return 0;
err:
    return rc;
}
