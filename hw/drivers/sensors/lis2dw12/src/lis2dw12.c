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
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "lis2dw12/lis2dw12.h"
#include "lis2dw12_priv.h"
#include "hal/hal_gpio.h"
#include "log/log.h"
#include "stats/stats.h"

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

#define LOG_MODULE_LIS2DW12    (212)
#define LIS2DW12_INFO(...)     LOG_INFO(&_log, LOG_MODULE_LIS2DW12, __VA_ARGS__)
#define LIS2DW12_ERR(...)      LOG_ERROR(&_log, LOG_MODULE_LIS2DW12, __VA_ARGS__)
static struct log _log;

#define LIS2DW12_NOTIFY_MASK 0x01

/* Exports for the sensor API */
static int lis2dw12_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int lis2dw12_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);
static int lis2dw12_sensor_set_notification(struct sensor *,
                                            sensor_event_type_t);
static int lis2dw12_sensor_unset_notification(struct sensor *,
                                              sensor_event_type_t);
static int lis2dw12_sensor_handle_interrupt(struct sensor *);
static int lis2dw12_sensor_set_config(struct sensor *, void *);

static const struct sensor_driver g_lis2dw12_sensor_driver = {
    .sd_read               = lis2dw12_sensor_read,
    .sd_set_config         = lis2dw12_sensor_set_config,
    .sd_get_config         = lis2dw12_sensor_get_config,
    .sd_set_notification   = lis2dw12_sensor_set_notification,
    .sd_unset_notification = lis2dw12_sensor_unset_notification,
    .sd_handle_interrupt   = lis2dw12_sensor_handle_interrupt

};

/**
 * Writes a single byte to the specified register using i2c
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lis2dw12_i2c_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;
    uint8_t payload[2] = { reg, value };
    
    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 2,
        .buffer = payload
    };

    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
        LIS2DW12_ERR("Failed to write to 0x%02X:0x%02X with value 0x%02X\n",
                    itf->si_addr, reg, value);
        STATS_INC(g_lis2dw12stats, read_errors);
    }

    return rc;
}

/**
 * Read multiple bytes starting from specified register over i2c
 *    
 * @param The sensor interface
 * @param The register address start reading from
 * @param Pointer to where the register value should be written
 * @param Number of bytes to read
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lis2dw12_i2c_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len)
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
        LIS2DW12_ERR("I2C access failed at address 0x%02X\n", itf->si_addr);
        STATS_INC(g_lis2dw12stats, write_errors);
        return rc;
    }

    /* Read data */
    data_struct.len = len;
    data_struct.buffer = buffer;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
        LIS2DW12_ERR("Failed to read from 0x%02X:0x%02X\n", itf->si_addr, reg);
        STATS_INC(g_lis2dw12stats, read_errors);
    }

    return rc;
}

/**
 * Writes a single byte to the specified register using SPI
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lis2dw12_spi_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    rc = hal_spi_tx_val(itf->si_num, reg & ~LIS2DW12_SPI_READ_CMD_BIT);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        LIS2DW12_ERR("SPI_%u register write failed addr:0x%02X\n",
                   itf->si_num, reg);
        STATS_INC(g_lis2dw12stats, write_errors);
        goto err;
    }

    /* Read data */
    rc = hal_spi_tx_val(itf->si_num, value);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        LIS2DW12_ERR("SPI_%u write failed addr:0x%02X:0x%02X\n",
                   itf->si_num, reg);
        STATS_INC(g_lis2dw12stats, write_errors);
        goto err;
    }

    rc = 0;

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    return rc;
}

/**
 * Read multiple bytes starting from specified register over SPI
 *
 * @param The sensor interface
 * @param The register address start reading from
 * @param Pointer to where the register value should be written
 * @param Number of bytes to read
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_spi_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
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
        LIS2DW12_ERR("SPI_%u register write failed addr:0x%02X\n",
                   itf->si_num, reg);
        STATS_INC(g_lis2dw12stats, read_errors);
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(itf->si_num, 0);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            LIS2DW12_ERR("SPI_%u read failed addr:0x%02X\n",
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
 * Write byte to sensor over different interfaces
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = lis2dw12_i2c_write8(itf, reg, value);
    } else {
        rc = lis2dw12_spi_write8(itf, reg, value);
    }

    return rc;
}

/**
 * Read byte data from sensor over different interfaces
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value)
{
    int rc;

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = lis2dw12_i2c_readlen(itf, reg, value, 1);
    } else {
        rc = lis2dw12_spi_readlen(itf, reg, value, 1);
    }

    return rc;
}

/**
 * Read multiple bytes starting from specified register over different interfaces
 *
 * @param The sensor interface
 * @param The register address start reading from
 * @param Pointer to where the register value should be written
 * @param Number of bytes to read
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
                uint8_t len)
{
    int rc;

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = lis2dw12_i2c_readlen(itf, reg, buffer, len);
    } else {
        rc = lis2dw12_spi_readlen(itf, reg, buffer, len);
    }

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
 * Reset lis2dw12
 *
 * @param The sensor interface
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_reset(struct sensor_itf *itf)
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
 * @param sensor interface
 * @param ptr to chip id to be filled up
 */
int
lis2dw12_get_chip_id(struct sensor_itf *itf, uint8_t *chip_id)
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
 * @param The sensor interface
 * @param The fs setting
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_full_scale(struct sensor_itf *itf, uint8_t fs)
{
    int rc;
    uint8_t reg;

    if (fs > LIS2DW12_FS_16G) {
        LIS2DW12_ERR("Invalid full scale value\n");
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG6, &reg);
    if (rc) {
        goto err;
    }

    reg = (reg & ~LIS2DW12_CTRL_REG6_FS) | fs;

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
 * @param The sensor interface
 * @param ptr to fs
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_full_scale(struct sensor_itf *itf, uint8_t *fs)
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
 * @param The sensor interface
 * @param The rate
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_rate(struct sensor_itf *itf, uint8_t rate)
{
    int rc;
    uint8_t reg;

    if (rate > LIS2DW12_DATA_RATE_1600HZ) {
        LIS2DW12_ERR("Invalid rate value\n");
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG1, &reg);
    if (rc) {
        goto err;
    }

    reg = (reg & ~LIS2DW12_CTRL_REG1_ODR) | rate;

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
 * @param The sensor ineterface
 * @param ptr to the rate
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_rate(struct sensor_itf *itf, uint8_t *rate)
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
 * @param The sensor interface
 * @param low noise enabled
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_low_noise(struct sensor_itf *itf, uint8_t en)
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
 * @param The sensor interface
 * @param ptr to low noise settings read from sensor
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_low_noise(struct sensor_itf *itf, uint8_t *en)
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
 * Sets the power mode of the sensor
 *
 * @param The sensor interface
 * @param power mode
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_power_mode(struct sensor_itf *itf, uint8_t mode)
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
 * Gets the power mode of the sensor
 *
 * @param The sensor interface
 * @param ptr to power mode setting read from sensor
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_power_mode(struct sensor_itf *itf, uint8_t *mode)
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
 * Sets the self test mode of the sensor
 *
 * @param The sensor interface
 * @param self test mode
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_self_test(struct sensor_itf *itf, uint8_t mode)
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
 * Gets the self test mode of the sensor
 *
 * @param The sensor interface
 * @param ptr to self test mode read from sensor
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_self_test(struct sensor_itf *itf, uint8_t *mode)
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
 * Set filter config
 *
 * @param the sensor interface
 * @param the filter bandwidth
 * @param filter type (1 = high pass, 0 = low pass)
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_filter_cfg(struct sensor_itf *itf, uint8_t bw, uint8_t type)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG6, &reg);
    if (rc) {
        goto err;
    }

    reg &= ~LIS2DW12_CTRL_REG6_BW_FILT;
    reg &= ~LIS2DW12_CTRL_REG6_FDS;
    reg |= (bw & LIS2DW12_CTRL_REG6_BW_FILT);
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
 * @param the sensor interface
 * @param ptr to the filter bandwidth
 * @param ptr to filter type (1 = high pass, 0 = low pass)
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_get_filter_cfg(struct sensor_itf *itf, uint8_t *bw, uint8_t *type)
{
    int rc;
    uint8_t reg;

    rc = lis2dw12_read8(itf, LIS2DW12_REG_CTRL_REG6, &reg);
    if (rc) {
        goto err;
    }

    *bw = reg & LIS2DW12_CTRL_REG6_BW_FILT;
    *type = (reg & LIS2DW12_CTRL_REG6_FDS) > 0;

    return 0;
err:
    return rc;
}

/**
 * Sets new offsets in sensor
 *
 * @param The sensor interface
 * @param X offset
 * @param Y offset
 * @param Z offset
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lis2dw12_set_offsets(struct sensor_itf *itf, int8_t offset_x,
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
 * @param The sensor interface
 * @param Pointer to location to store X offset
 * @param Pointer to location to store Y offset
 * @param Pointer to location to store Z offset
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lis2dw12_get_offsets(struct sensor_itf *itf, int8_t *offset_x,
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
 * @param The sensor interface
 * @param value to set (0 = disabled, 1 = enabled)
 *
 * @return 0 on success, non-zero error on failure.
 */
int lis2dw12_set_offset_enable(struct sensor_itf *itf, uint8_t enabled)
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
 * @param the sensor interface
 * @param the tap settings
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_tap_cfg(struct sensor_itf *itf, struct lis2dw12_tap_settings *cfg)
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
    reg |= cfg->tap_ths_z & LIS2DW12_REG_TAP_THS_Z;
    
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
 * @param the sensor interface
 * @param ptr to the tap settings
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_tap_cfg(struct sensor_itf *itf, struct lis2dw12_tap_settings *cfg)
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
 * @param the sensor interface
 * @param freefall duration (5 bits LSB = 1/ODR)
 * @param freefall threshold (3 bits)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_freefall(struct sensor_itf *itf, uint8_t dur, uint8_t ths)
{
    uint8_t reg;

    reg = 0;
    reg |= (dur & 0x1F) << 3;
    reg |= ths & LIS2DW12_FREEFALL_THS;

    return lis2dw12_write8(itf, LIS2DW12_REG_FREEFALL, reg);
}

/**
 * Get freefall detection config
 *
 * @param the sensor interface
 * @param ptr to freefall duration
 * @param ptr to freefall threshold
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_freefall(struct sensor_itf *itf, uint8_t *dur, uint8_t *ths)
{
    int rc;
    uint8_t reg;
    
    rc = lis2dw12_read8(itf, LIS2DW12_REG_FREEFALL, &reg);
    if (rc) {
        return rc;
    }

    *dur = (reg & LIS2DW12_FREEFALL_DUR) >> 3;
    *ths = reg & LIS2DW12_FREEFALL_THS;

    return 0;
}

/**
 * Setup FIFO
 *
 * @param the sensor interface
 * @param FIFO mode to setup
 * @patam Threshold to set for FIFO
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_fifo_cfg(struct sensor_itf *itf, enum lis2dw12_fifo_mode mode, uint8_t fifo_ths)
{
    uint8_t reg = 0;

    reg |= fifo_ths & LIS2DW12_FIFO_CTRL_FTH;
    reg |= (mode & 0x7) << 5;

    return lis2dw12_write8(itf, LIS2DW12_REG_FIFO_CTRL, reg);
}

/**
 * Get Number of Samples in FIFO
 *
 * @param the sensor interface
 * @patam Pointer to return number of samples in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_fifo_samples(struct sensor_itf *itf, uint8_t *samples)
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
 * Set interrupt pin configuration for interrupt 1
 *
 * @param the sensor interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_int1_pin_cfg(struct sensor_itf *itf, uint8_t cfg)
{
    return lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG4, cfg);
}

/**
 * Set interrupt pin configuration for interrupt 2
 *
 * @param the sensor interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int
lis2dw12_set_int2_pin_cfg(struct sensor_itf *itf, uint8_t cfg)
{
    return lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG5, cfg);
}

/**
 * Set Wake Up Threshold configuration
 *
 * @param the sensor interface
 * @param wake_up_ths value to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_wake_up_ths(struct sensor_itf *itf, uint8_t val)
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
 * @param the sensor interface
 * @param ptr to store wake_up_ths value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_ths(struct sensor_itf *itf, uint8_t *val)
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
 * @param the sensor interface
 * @param value to set (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_inactivity_sleep_en(struct sensor_itf *itf, uint8_t en)
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
 * @param the sensor interface
 * @param ptr to store read state (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_inactivity_sleep_en(struct sensor_itf *itf, uint8_t *en)
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
 * @param the sensor interface
 * @param value to set (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_double_tap_event_en(struct sensor_itf *itf, uint8_t en)
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
 * @param the sensor interface
 * @param ptr to store read state (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_double_tap_event_en(struct sensor_itf *itf, uint8_t *en)
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
 * @param the sensor interface
 * @param value to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_wake_up_dur(struct sensor_itf *itf, uint8_t val)
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
 * @param the sensor interface
 * @param ptr to store wake_up_dur value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_dur(struct sensor_itf *itf, uint8_t *val)
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
 * @param the sensor interface
 * @param value to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_sleep_dur(struct sensor_itf *itf, uint8_t val)
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
 * @param the sensor interface
 * @param ptr to store sleep_dur value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_sleep_dur(struct sensor_itf *itf, uint8_t *val)
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
 * @param the sensor interface
 * @param value to set
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_stationary_en(struct sensor_itf *itf, uint8_t en)
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
 * @param the sensor interface
 * @param ptr to store sleep_dur value
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_stationary_en(struct sensor_itf *itf, uint8_t *en)
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
 * @param the sensor interface
 */
int
lis2dw12_clear_int(struct sensor_itf *itf, uint8_t *src)
{
    return lis2dw12_read8(itf, LIS2DW12_REG_INT_SRC, src);  
}

/**
 * Get Interrupt Source
 *
 * @param the sensor interface
 * @param pointer to return interrupt source in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_int_src(struct sensor_itf *itf, uint8_t *status)
{
    return lis2dw12_read8(itf, LIS2DW12_REG_STATUS_REG, status);
}

/**
 * Get Wake Up Source
 *
 * @param the sensor interface
 * @param pointer to return wake_up_src in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_wake_up_src(struct sensor_itf *itf, uint8_t *status)
{
    return lis2dw12_read8(itf, LIS2DW12_REG_WAKE_UP_SRC, status);
}

/**
 * Get Tap Source
 *
 * @param the sensor interface
 * @param pointer to return tap_src in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_tap_src(struct sensor_itf *itf, uint8_t *status)
{
    return lis2dw12_read8(itf, LIS2DW12_REG_TAP_SRC, status);
}

/**
 * Get 6D Source
 *
 * @param the sensor interface
 * @param pointer to return sixd_src in
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_get_sixd_src(struct sensor_itf *itf, uint8_t *status)
{
    return lis2dw12_read8(itf, LIS2DW12_REG_SIXD_SRC, status);
}


/**
 * Set whether interrupts are enabled
 *
 * @param the sensor interface
 * @param value to set (0 = disabled, 1 = enabled)
 * @return 0 on success, non-zero on failure
 */
int lis2dw12_set_int_enable(struct sensor_itf *itf, uint8_t enabled)
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
 * Run Self test on sensor
 *
 * @param the sensor interface
 * @param pointer to return test result in (0 on pass, non-zero on failure)
 *
 * @return 0 on sucess, non-zero on failure
 */
int lis2dw12_run_self_test(struct sensor_itf *itf, int *result)
{
    int rc;
    int16_t base[3], pos[3];
    int i;
    int16_t change;
    
    /* ensure self test mode is disabled */
    rc = lis2dw12_set_self_test(itf, LIS2DW12_ST_MODE_DISABLE);
    if (rc) {
        return rc;
    }
    
    os_time_delay(OS_TICKS_PER_SEC / 10);
    
    /* take base reading */
    rc = lis2dw12_get_data(itf, &(base[0]), &(base[1]), &(base[2]));
    if (rc) {
        return rc;
    }

    /* set self test mode to positive self test */
    rc = lis2dw12_set_self_test(itf, LIS2DW12_ST_MODE_MODE1);
    if (rc) {
        return rc;
    }

    os_time_delay(OS_TICKS_PER_SEC / 10);

    /* take self test reading */
    rc = lis2dw12_get_data(itf, &(pos[0]), &(pos[1]), &(pos[2]));
    if (rc) {
        return rc;
    }

    /* disable self test mod */
    rc = lis2dw12_set_self_test(itf, LIS2DW12_ST_MODE_DISABLE);
    if (rc) {
        return rc;
    }

    /* calculate accel data difference */
    change = 0;
    for(i = 0; i < 3; i++) {
        change += pos[i] - base[i];
    }

    if ((change > 70) && (change < 1500)) {
        *result = 0;
    } else {
        *result = -1;
    }

    return 0;
}


static void
init_interrupt(struct lis2dw12_int * interrupt, struct sensor_int *ints)
{
    os_error_t error;

    error = os_sem_init(&interrupt->wait, 0);
    assert(error == OS_OK);

    interrupt->active = false;
    interrupt->asleep = false;
    interrupt->ints = ints;
}

static void
undo_interrupt(struct lis2dw12_int * interrupt)
{
    OS_ENTER_CRITICAL(interrupt->lock);
    interrupt->active = false;
    interrupt->asleep = false;
    OS_EXIT_CRITICAL(interrupt->lock);
}

static void
wait_interrupt(struct lis2dw12_int * interrupt, uint8_t int_num)
{
    bool wait;

    OS_ENTER_CRITICAL(interrupt->lock);

    /* Check if we did not missed interrupt */
    if (hal_gpio_read(interrupt->ints[int_num].host_pin) ==
                                            interrupt->ints[int_num].active) {
        OS_EXIT_CRITICAL(interrupt->lock);
        return;
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
        os_error_t error;

        error = os_sem_pend(&interrupt->wait, -1);
        assert(error == OS_OK);
    }
}

static void
wake_interrupt(struct lis2dw12_int * interrupt)
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
    struct sensor *sensor = arg;
    struct lis2dw12 *lis2dw12;

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);

    if(lis2dw12->pdd.interrupt) {
        wake_interrupt(lis2dw12->pdd.interrupt);
    }
    
    sensor_mgr_put_interrupt_evt(sensor);
}

static int
init_intpin(struct lis2dw12 * lis2dw12, hal_gpio_irq_handler_t handler,
            void * arg)
{
    struct lis2dw12_private_driver_data *pdd = &lis2dw12->pdd;
    hal_gpio_irq_trig_t trig;
    int pin = -1;
    int rc;
    int i;

    for (i = 0; i < MYNEWT_VAL(SENSOR_MAX_INTERRUPTS_PINS); i++){
        pin = lis2dw12->sensor.s_itf.si_ints[i].host_pin;
        if (pin >= 0) {
            break;
        }
    }

    if (pin < 0) {
        LIS2DW12_ERR("Interrupt pin not configured\n");
        return SYS_EINVAL;
    }

    pdd->int_num = i;
    if (lis2dw12->sensor.s_itf.si_ints[pdd->int_num].active) {
        trig = HAL_GPIO_TRIG_RISING;
    } else {
        trig = HAL_GPIO_TRIG_FALLING;
    }
  
    rc = hal_gpio_irq_init(pin,
                           handler,
                           arg,
                           trig,
                           HAL_GPIO_PULL_NONE);
    if (rc != 0) {
        LIS2DW12_ERR("Failed to initialise interrupt pin %d\n", pin);
        return rc;
    } 

    return 0;
}

static int
enable_interrupt(struct sensor * sensor, uint8_t int_to_enable)
{
    struct lis2dw12 *lis2dw12;
    struct lis2dw12_private_driver_data *pdd;
    struct sensor_itf *itf;
    uint8_t reg;
    int rc;

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lis2dw12->pdd;

    rc = lis2dw12_clear_int(itf, &reg);
    if (rc) {
        return rc;
    }
    
    /* if no interrupts are currently in use enable int pin */
    if (pdd->int_enable == 0) {
        hal_gpio_irq_enable(itf->si_ints[pdd->int_num].host_pin);

        rc = lis2dw12_set_int_enable(itf, 1);
        if (rc) {
            return rc;
        }
    }

    /*update which interrupts are enabled */
    pdd->int_enable |= int_to_enable;
    
    /* enable interrupt in device */
    rc = lis2dw12_set_int1_pin_cfg(itf, pdd->int_enable);
    if (rc) {
        return rc;
    }

    return 0;
    
}

static int
disable_interrupt(struct sensor * sensor, uint8_t int_to_disable)
{
    struct lis2dw12 *lis2dw12;
    struct lis2dw12_private_driver_data *pdd;
    struct sensor_itf *itf;
    int rc;

    if (int_to_disable == 0) {
        return SYS_EINVAL;
    }
    
    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lis2dw12->pdd;

    pdd->int_enable &= ~int_to_disable;
    
    /* disable int pin */
    if (pdd->int_enable == 0) {
        hal_gpio_irq_disable(itf->si_ints[pdd->int_num].host_pin);
           
        rc = lis2dw12_set_int_enable(itf, 0);
        if (rc) {
            return rc;
        }
    }
    
    /* update interrupt setup in device */
    return lis2dw12_set_int1_pin_cfg(itf, pdd->int_enable);
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
lis2dw12_get_data(struct sensor_itf *itf, int16_t *x, int16_t *y, int16_t *z)
{
    int rc;
    uint8_t payload[6] = {0};
    uint8_t fs;

    *x = *y = *z = 0;

    rc = lis2dw12_readlen(itf, LIS2DW12_REG_OUT_X_L, payload, 6);
    if (rc) {
        goto err;
    }

    *x = payload[0] | (payload[1] << 8);
    *y = payload[2] | (payload[3] << 8);
    *z = payload[4] | (payload[5] << 8);

    rc = lis2dw12_get_full_scale(itf, &fs);
    if (rc) {
        goto err;
    }

    if (fs == LIS2DW12_FS_2G) {
        fs = 2;
    } else if (fs == LIS2DW12_FS_4G) {
        fs = 4;
    } else if (fs == LIS2DW12_FS_8G) {
        fs = 8;
    } else if (fs == LIS2DW12_FS_16G) {
        fs = 16;
    } else {
        rc = SYS_EINVAL;
        goto err;
    }

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

static int lis2dw12_do_read(struct sensor *sensor, sensor_data_func_t data_func,
                            void * data_arg)
{
    struct sensor_accel_data sad;
    struct sensor_itf *itf;
    int16_t x, y ,z;
    float fx, fy ,fz;
    int rc;

    itf = SENSOR_GET_ITF(sensor);

    x = y = z = 0;

    rc = lis2dw12_get_data(itf, &x, &y, &z);
    if (rc) {
        goto err;
    }

    /* converting values from mg to ms^2 */
    lis2dw12_calc_acc_ms2(x, &fx);
    lis2dw12_calc_acc_ms2(y, &fy);
    lis2dw12_calc_acc_ms2(z, &fz);

    sad.sad_x = fx;
    sad.sad_y = fy;
    sad.sad_z = fz;

    sad.sad_x_is_valid = 1;
    sad.sad_y_is_valid = 1;
    sad.sad_z_is_valid = 1;

    /* Call data function */
    rc = data_func(sensor, data_arg, &sad, SENSOR_TYPE_ACCELEROMETER);
    if (rc != 0) {
        goto err;
    }

    return 0;
err:
    return rc;  
}

/**
 * Do accelerometer polling reads
 *
 * @param The sensor ptr
 * @param The sensor type
 * @param The function pointer to invoke for each accelerometer reading.
 * @param The opaque pointer that will be passed in to the function.
 * @param If non-zero, how long the stream should run in milliseconds.
 *
 * @return 0 on success, non-zero on failure.
 */
int
lis2dw12_poll_read(struct sensor * sensor, sensor_type_t sensor_type,
                 sensor_data_func_t data_func, void * data_arg,
                 uint32_t timeout)
{
    /* If the read isn't looking for accel data, don't do anything. */
    if (!(sensor_type & SENSOR_TYPE_ACCELEROMETER)) {
        return SYS_EINVAL;
    }

    return lis2dw12_do_read(sensor, data_func, data_arg);
}

int
lis2dw12_stream_read(struct sensor *sensor,
                   sensor_type_t sensor_type,
                   sensor_data_func_t read_func,
                   void *read_arg,
                   uint32_t time_ms)
{
    struct lis2dw12 *lis2dw12;
    struct sensor_itf *itf;
    int rc;
    os_time_t time_ticks;
    os_time_t stop_ticks = 0;
    struct lis2dw12_private_driver_data *pdd;
    uint8_t fifo_samples;

    /* If the read isn't looking for accel data, don't do anything. */
    if (!(sensor_type & SENSOR_TYPE_ACCELEROMETER)) {
        return SYS_EINVAL;
    }

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lis2dw12->pdd;

    undo_interrupt(&lis2dw12->intr);

    if (pdd->interrupt) {
        return SYS_EBUSY;
    }

    /* enable interrupt */
    pdd->interrupt = &lis2dw12->intr;

    rc = enable_interrupt(sensor, lis2dw12->cfg.stream_read_interrupt);
    if (rc) {
        goto done;
    }

    if (time_ms != 0) {
        rc = os_time_ms_to_ticks(time_ms, &time_ticks);
        if (rc) {
            goto done;
        }
        stop_ticks = os_time_get() + time_ticks;
    }

    for (;;) {
        wait_interrupt(&lis2dw12->intr, pdd->int_num);
        fifo_samples = 1;
        
        while(fifo_samples > 0) {
            rc = lis2dw12_do_read(sensor, read_func, read_arg);
            if (rc) {
                goto done;
            }

            rc = lis2dw12_get_fifo_samples(itf, &fifo_samples);
            if (rc) {
                goto done;
            }
        }
        
        if (time_ms != 0 && OS_TIME_TICK_GT(os_time_get(), stop_ticks)) {
                break;
        }
    }

done:
    /* disable interrupt */
    pdd->interrupt = NULL;
    rc = disable_interrupt(sensor, lis2dw12->cfg.stream_read_interrupt);

    return rc;
}

static int
lis2dw12_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int rc;
    const struct lis2dw12_cfg *cfg;
    struct lis2dw12 *lis2dw12;
    struct sensor_itf *itf;

    /* If the read isn't looking for accel data, don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER)) {
        rc = SYS_EINVAL;
        goto err;
    }

    itf = SENSOR_GET_ITF(sensor);

    if (itf->si_type == SENSOR_ITF_SPI) {
        
        rc = hal_spi_disable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_lis2dw12_settings);
        if (rc == EINVAL) {
            /* If spi is already enabled, for nrf52, it returns -1, We should not
             * fail if the spi is already enabled
             */
            goto err;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }
    }
    
    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    cfg = &lis2dw12->cfg;

    if (cfg->read_mode == LIS2DW12_READ_M_POLL) {
        rc = lis2dw12_poll_read(sensor, type, data_func, data_arg, timeout);
    } else {
        rc = lis2dw12_stream_read(sensor, type, data_func, data_arg, timeout);
    }

err:
    return rc;
}

static int
lis2dw12_sensor_set_notification(struct sensor *sensor, sensor_event_type_t type)
{
    struct lis2dw12 * lis2dw12;
    struct sensor_itf *itf;
    uint8_t int_cfg = 0;
    struct lis2dw12_private_driver_data *pdd;
    int rc;

    if ((type & ~(SENSOR_EVENT_TYPE_DOUBLE_TAP |
                  SENSOR_EVENT_TYPE_SINGLE_TAP)) != 0) {
        return SYS_EINVAL;
    }

    /*XXX for now we do not support registering for both events */
    if (type == (SENSOR_EVENT_TYPE_DOUBLE_TAP |
                 SENSOR_EVENT_TYPE_SINGLE_TAP)) {
        return SYS_EINVAL;
    }

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lis2dw12->pdd;

    if (pdd->registered_mask & LIS2DW12_NOTIFY_MASK) {
        return SYS_EBUSY;
    }

    /* Enable tap interrupt */
    if(type == SENSOR_EVENT_TYPE_DOUBLE_TAP) {
        int_cfg |= LIS2DW12_INT1_CFG_DOUBLE_TAP;
    }
    if(type == SENSOR_EVENT_TYPE_SINGLE_TAP) {
        int_cfg |= LIS2DW12_INT1_CFG_SINGLE_TAP;
    }

    rc = enable_interrupt(sensor, int_cfg);
    if (rc) {
        return rc;
    }

    /* enable double tap detection in wake_up_ths */
    if(type == SENSOR_EVENT_TYPE_DOUBLE_TAP) {
        rc = lis2dw12_set_double_tap_event_en(itf, 1);
        if (rc) {
            return rc;
        }
    }

    pdd->notify_ctx.snec_evtype |= type;
    pdd->registered_mask |= LIS2DW12_NOTIFY_MASK;

    return 0;
}

static int
lis2dw12_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct lis2dw12 *lis2dw12;

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);

    return lis2dw12_config(lis2dw12, (struct lis2dw12_cfg*)cfg);
}

static int
lis2dw12_sensor_unset_notification(struct sensor *sensor, sensor_event_type_t type)
{
    struct lis2dw12 * lis2dw12;
    struct sensor_itf *itf;
    int rc;
    
    if ((type & ~(SENSOR_EVENT_TYPE_DOUBLE_TAP |
                  SENSOR_EVENT_TYPE_SINGLE_TAP)) != 0) {
        return SYS_EINVAL;
    }
    
    /*XXX for now we do not support registering for both events */
    if (type == (SENSOR_EVENT_TYPE_DOUBLE_TAP |
                 SENSOR_EVENT_TYPE_SINGLE_TAP)) {
        return SYS_EINVAL;
    }
    
    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);

    lis2dw12->pdd.notify_ctx.snec_evtype &= ~type;
    lis2dw12->pdd.registered_mask &= ~LIS2DW12_NOTIFY_MASK;

    rc = lis2dw12_set_double_tap_event_en(itf, lis2dw12->cfg.double_tap_event_enable);
    if (rc) {
        return rc;
    }
    
    return disable_interrupt(sensor, 0);
}

static int
lis2dw12_sensor_handle_interrupt(struct sensor * sensor)
{
    struct lis2dw12 * lis2dw12;
    struct lis2dw12_private_driver_data *pdd;
    struct sensor_itf *itf;
    uint8_t int_src;
    
    int rc;

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);

    pdd = &lis2dw12->pdd;

    rc = lis2dw12_clear_int(itf, &int_src);
    if (rc) {
        LIS2DW12_ERR("Cound not read int status err=0x%02x\n", rc);
        return rc;
    }

    if ((pdd->registered_mask & LIS2DW12_NOTIFY_MASK) &&
        ((int_src & LIS2DW12_INT_SRC_STAP) ||
         (int_src & LIS2DW12_INT_SRC_DTAP))) {
        sensor_mgr_put_notify_evt(&pdd->notify_ctx);
    }

    return 0;
}

static int
lis2dw12_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if (type != SENSOR_TYPE_ACCELEROMETER) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return 0;
err:
    return rc;
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
    struct lis2dw12 *lis2dw12;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    lis2dw12 = (struct lis2dw12 *) dev;

    lis2dw12->cfg.mask = SENSOR_TYPE_ALL;

    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);

    sensor = &lis2dw12->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lis2dw12stats),
        STATS_SIZE_INIT_PARMS(g_lis2dw12stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lis2dw12_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_lis2dw12stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = sensor_init(sensor, dev);
    if (rc) {
        goto err;
    }

    /* Add the light driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_ACCELEROMETER,
            (struct sensor_driver *) &g_lis2dw12_sensor_driver);
    if (rc) {
        goto err;
    }

    /* Set the interface */
    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        goto err;
    }

    rc = sensor_mgr_register(sensor);
    if (rc) {
        goto err;
    }

    if (sensor->s_itf.si_type == SENSOR_ITF_SPI) {

        rc = hal_spi_disable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_lis2dw12_settings);
        if (rc == EINVAL) {
            /* If spi is already enabled, for nrf52, it returns -1, We should not
             * fail if the spi is already enabled
             */
            goto err;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_gpio_init_out(sensor->s_itf.si_cs_pin, 1);
        if (rc) {
            goto err;
        }
    }


    init_interrupt(&lis2dw12->intr, lis2dw12->sensor.s_itf.si_ints);
    
    lis2dw12->pdd.notify_ctx.snec_sensor = sensor;
    lis2dw12->pdd.registered_mask = 0;
    lis2dw12->pdd.interrupt = NULL;

    rc = init_intpin(lis2dw12, lis2dw12_int_irq_handler, sensor);
    if (rc) {
        return rc;
    }

    return 0;
err:
    return rc;

}

/**
 * Configure the sensor
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 */
int
lis2dw12_config(struct lis2dw12 *lis2dw12, struct lis2dw12_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;
    uint8_t chip_id;
    struct sensor *sensor;

    itf = SENSOR_GET_ITF(&(lis2dw12->sensor));
    sensor = &(lis2dw12->sensor);

    if (itf->si_type == SENSOR_ITF_SPI) {

        rc = hal_spi_disable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_lis2dw12_settings);
        if (rc == EINVAL) {
            /* If spi is already enabled, for nrf52, it returns -1, We should not
             * fail if the spi is already enabled
             */
            goto err;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }
    }

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

    rc = lis2dw12_write8(itf, LIS2DW12_REG_CTRL_REG3, LIS2DW12_CTRL_REG3_LIR);
    if (rc) {
        goto err;
    }
    
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

    rc = lis2dw12_set_self_test(itf, LIS2DW12_ST_MODE_DISABLE);
    if (rc) {
        goto err;
    }

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

    rc = lis2dw12_set_tap_cfg(itf, &cfg->tap_cfg);
    if (rc) {
        goto err;
    }
    lis2dw12->cfg.tap_cfg = cfg->tap_cfg;
    
    rc = sensor_set_type_mask(&(lis2dw12->sensor), cfg->mask);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.stream_read_interrupt = cfg->stream_read_interrupt;
    lis2dw12->cfg.read_mode = cfg->read_mode;    
    lis2dw12->cfg.mask = cfg->mask;

    return 0;
err:
    return rc;
}
