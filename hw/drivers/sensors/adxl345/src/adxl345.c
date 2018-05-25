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

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <math.h>

#include "os/mynewt.h"
#include "hal/hal_i2c.h"
#include "hal/hal_spi.h"
#include "hal/hal_gpio.h"
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "adxl345/adxl345.h"
#include "adxl345_priv.h"
#include "log/log.h"
#include "stats/stats.h"

static struct hal_spi_settings spi_adxl345_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE3,
    .baudrate   = 4000,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};


/* Define the stats section and records */
STATS_SECT_START(adxl345_stat_section)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(adxl345_stat_section)
    STATS_NAME(adxl345_stat_section, read_errors)
    STATS_NAME(adxl345_stat_section, write_errors)
STATS_NAME_END(adxl345_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(adxl345_stat_section) g_adxl345stats;

#define LOG_MODULE_ADXL345    (345)
#define ADXL345_INFO(...)     LOG_INFO(&_log, LOG_MODULE_ADXL345, __VA_ARGS__)
#define ADXL345_ERR(...)      LOG_ERROR(&_log, LOG_MODULE_ADXL345, __VA_ARGS__)
static struct log _log;

#define ADXL345_NOTIFY_MASK  0x01
#define ADXL345_READ_MASK    0x02

#if MYNEWT_VAL(ADXL345_INT_ENABLE)
static void interrupt_handler(void * arg);
static int init_intpin(struct adxl345 * adxl345, hal_gpio_irq_handler_t handler,
                       void * arg);
static int enable_interrupt(struct sensor * sensor, uint8_t ints_to_enable);
static int disable_interrupt(struct sensor * sensor, uint8_t ints_to_disable);
#endif

/* Exports for the sensor API */
static int adxl345_sensor_read(struct sensor *, sensor_type_t,
                               sensor_data_func_t, void *, uint32_t);
static int adxl345_sensor_get_config(struct sensor *, sensor_type_t,
                                     struct sensor_cfg *);
static int adxl345_sensor_set_trigger_thresh(struct sensor * sensor,
                                             sensor_type_t sensor_type,
                                             struct sensor_type_traits * stt);
static int adxl345_sensor_set_notification(struct sensor * sensor,
                                           sensor_event_type_t sensor_event_type);
static int adxl345_sensor_unset_notification(struct sensor * sensor,
                                             sensor_event_type_t sensor_event_type);
static int adxl345_sensor_handle_interrupt(struct sensor * sensor);
static int adxl345_sensor_clear_low_thresh(struct sensor *sensor,
                                           sensor_type_t type);
static int adxl345_sensor_clear_high_thresh(struct sensor *sensor,
                                            sensor_type_t type);


static const struct sensor_driver adxl345_sensor_driver = {
     .sd_read                      = adxl345_sensor_read,
     .sd_get_config                = adxl345_sensor_get_config,
     .sd_set_trigger_thresh        = adxl345_sensor_set_trigger_thresh,
     .sd_set_notification          = adxl345_sensor_set_notification,
     .sd_unset_notification        = adxl345_sensor_unset_notification,
     .sd_handle_interrupt          = adxl345_sensor_handle_interrupt,
     .sd_clear_low_trigger_thresh  = adxl345_sensor_clear_low_thresh,
     .sd_clear_high_trigger_thresh = adxl345_sensor_clear_high_thresh
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
adxl345_i2c_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value)
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
        ADXL345_ERR("Failed to write to 0x%02X:0x%02X with value 0x%02X\n",
                    itf->si_addr, reg, value);
        STATS_INC(g_adxl345stats, read_errors);
    }

    return rc;
}

/**
 * Reads a single byte from the specified register using i2c
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_i2c_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value)
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
        ADXL345_ERR("I2C access failed at address 0x%02X\n", itf->si_addr);
        STATS_INC(g_adxl345stats, write_errors);
        return rc;
    }
    
    /* Read one byte back */
    data_struct.buffer = value;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
        ADXL345_ERR("Failed to read from 0x%02X:0x%02X - %02X\n", itf->si_addr, reg, rc);
        STATS_INC(g_adxl345stats, read_errors);
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
adxl345_i2c_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len)
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
        ADXL345_ERR("I2C access failed at address 0x%02X\n", itf->si_addr);
        STATS_INC(g_adxl345stats, write_errors);
        return rc;
    }

    /* Read data */
    data_struct.len = len;
    data_struct.buffer = buffer;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
        ADXL345_ERR("Failed to read from 0x%02X:0x%02X\n", itf->si_addr, reg);
        STATS_INC(g_adxl345stats, read_errors);
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
adxl345_spi_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    rc = hal_spi_tx_val(itf->si_num, reg & ~ADXL345_SPI_READ_CMD_BIT);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        ADXL345_ERR("SPI_%u register write failed addr:0x%02X\n",
                   itf->si_num, reg);
        STATS_INC(g_adxl345stats, write_errors);
        goto err;
    }

    /* Read data */
    rc = hal_spi_tx_val(itf->si_num, value);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        ADXL345_ERR("SPI_%u write failed addr:0x%02X\n",
                   itf->si_num, reg);
        STATS_INC(g_adxl345stats, write_errors);
        goto err;
    }

    rc = 0;

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    return rc;
}


/**
 * Reads a single byte from the specified register using SPI
 *

 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_spi_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value)
{
    uint16_t retval;
    int rc = 0;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(itf->si_num, reg | ADXL345_SPI_READ_CMD_BIT);
    
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        ADXL345_ERR("SPI_%u register write failed addr:0x%02X\n",
                   itf->si_num, reg);
        STATS_INC(g_adxl345stats, read_errors);
        goto err;
    }

    /* Read data */
    retval = hal_spi_tx_val(itf->si_num, 0);
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        ADXL345_ERR("SPI_%u read failed addr:0x%02X\n",
                    itf->si_num, reg);
        STATS_INC(g_adxl345stats, read_errors);
        goto err;
    }
    *value = retval;
    
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
adxl345_spi_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
                    uint8_t len)
{
    int i;
    uint16_t retval;
    int rc = 0;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(itf->si_num, reg | ADXL345_SPI_READ_CMD_BIT
                            | ADXL345_SPI_MULTIBYTE_CMD_BIT);
    
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        ADXL345_ERR("SPI_%u register write failed addr:0x%02X\n",
                   itf->si_num, reg);
        STATS_INC(g_adxl345stats, read_errors);
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(itf->si_num, 0);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            ADXL345_ERR("SPI_%u read failed addr:0x%02X\n",
                       itf->si_num, reg);
            STATS_INC(g_adxl345stats, read_errors);
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
 * Write byte to ADXL345 sensor over different interfaces
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero on failure
 */
int
adxl345_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = adxl345_i2c_write8(itf, reg, value);
    } else {
        rc = adxl345_spi_write8(itf, reg, value);
    }

    return rc;
}

/**
 * Read byte data from ADXL345 sensor over different interfaces
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero on failure
 */
int
adxl345_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value)
{
    int rc;

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = adxl345_i2c_read8(itf, reg, value);
    } else {
        rc = adxl345_spi_read8(itf, reg, value);
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
adxl345_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
                uint8_t len)
{
    int rc;

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = adxl345_i2c_readlen(itf, reg, buffer, len);
    } else {
        rc = adxl345_spi_readlen(itf, reg, buffer, len);
    }

    return rc;
}

/**
 * Sets ADXL345 into new power mode
 *
 * @param The sensor interface
 * @param Power mode to set
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_set_power_mode(struct sensor_itf *itf, enum adxl345_power_mode state)
{
    uint8_t reg;
    int rc;
  
    rc = adxl345_read8(itf, ADXL345_POWER_CTL, &reg);
    if (rc) {
        return rc;
    }

    reg &= 0xF3;
    reg |= ((uint8_t)state) << 2;
  
    return adxl345_write8(itf, ADXL345_POWER_CTL, (uint8_t)reg);
}

/**
 * Gets power mode ADXL345 is currently in
 *
 * @param The sensor interface
 * @param Pointer to store current power mode
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_get_power_mode(struct sensor_itf *itf, enum adxl345_power_mode *state)
{
    uint8_t reg;
    int rc;

    rc = adxl345_read8(itf, ADXL345_POWER_CTL, &reg);
    if (rc) {
        return rc;
    }

    reg &= 0xC;
    reg >>= 2;

    *state = (enum adxl345_accel_range)reg;

    return 0;
}

/**
 * Sets whether low power mode is enabled. Low power mode acheives lower
 * power consumption at the expense of higher noise, but is only effective
 * at sample rates between 12.5Hz and 400Hz
 *
 * @param The sensor interface
 * @param Setting to use for low power mode
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_set_low_power_enable(struct sensor_itf *itf, uint8_t enable)
{
    uint8_t reg;
    int rc;
  
    rc = adxl345_read8(itf, ADXL345_BW_RATE, &reg);
    if (rc) {
        return rc;
    }

    reg &= 0x0F;
    if (enable) {
        reg |= 0x10;
    }
  
    return adxl345_write8(itf, ADXL345_BW_RATE, (uint8_t)reg);
}

/**
 * Gets current setting of low power mode. Low power mode acheives lower
 * power consumption at the expense of higher noise, but is only effective
 * at sample rates between 12.5Hz and 400Hz
 *
 * @param The sensor interface
 * @param Pointer to store value
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_get_low_power_enable(struct sensor_itf *itf, uint8_t *enable)
{
    uint8_t reg;
    int rc;

    rc = adxl345_read8(itf, ADXL345_BW_RATE, &reg);
    if (rc) {
        return rc;
    }

    *enable = (reg & 0x10) != 0;

    return 0;
}

/**
 * Sets the accelerometer range to specified setting
 *
 * @param The sensor interface
 * @param accelerometer range to set
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_set_accel_range(struct sensor_itf *itf, enum adxl345_accel_range range)
{
    uint8_t reg;
    int rc;
  
    rc = adxl345_read8(itf, ADXL345_DATA_FORMAT, &reg);
    if (rc) {
        return rc;
    }

    reg &= 0xFC;
    reg |= (uint8_t)range;
  
    return adxl345_write8(itf, ADXL345_DATA_FORMAT, (uint8_t)reg);
}

/**
 * Gets the accelerometer range currently set
 *
 * @param The sensor interface
 * @param Pointer to location to store accelerometer range
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_get_accel_range(struct sensor_itf *itf, enum adxl345_accel_range *range)
{
    uint8_t reg;
    int rc;

    rc = adxl345_read8(itf, ADXL345_DATA_FORMAT, &reg);
    if (rc) {
        return rc;
    }

    *range = (enum adxl345_accel_range)(reg & 0x3);

    return 0;
}

/**
 * Sets new offsets in ADXL345
 *
 * @param The sensor interface
 * @param X offset
 * @param Y offset
 * @param Z offset
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_set_offsets(struct sensor_itf *itf, int8_t offset_x,
                    int8_t offset_y, int8_t offset_z)
{
    int rc;
  
    rc = adxl345_write8(itf, ADXL345_OFSX, offset_x);
    if (rc){
        return rc;
    }

    rc = adxl345_write8(itf, ADXL345_OFSY, offset_y);
    if (rc){
        return rc;
    }

    return adxl345_write8(itf, ADXL345_OFSZ, offset_z);
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
adxl345_get_offsets(struct sensor_itf *itf, int8_t *offset_x,
                    int8_t *offset_y, int8_t *offset_z)
{

    int rc;
  
    rc = adxl345_read8(itf, ADXL345_OFSX, (uint8_t *)offset_x);
    if (rc){
        return rc;
    }

    rc = adxl345_read8(itf, ADXL345_OFSY, (uint8_t *)offset_y);
    if (rc){
        return rc;
    }

    return adxl345_read8(itf, ADXL345_OFSZ, (uint8_t *)offset_z);
}

/**
 * Sets new tap settings in ADXL345 sensor
 *
 * @param The sensor interface
 * @param Structure of new settings
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_set_tap_settings(struct sensor_itf *itf, struct adxl345_tap_settings settings)
{
    int rc;
    uint8_t enables = 0;
    
    rc = adxl345_write8(itf, ADXL345_THRESH_TAP, settings.threshold);
    if (rc) {
        return rc;
    }

    rc = adxl345_write8(itf, ADXL345_DUR, settings.duration);
    if (rc) {
        return rc;
    }

    rc = adxl345_write8(itf, ADXL345_LATENT, settings.latency);
    if (rc) {
        return rc;
    }

    rc = adxl345_write8(itf, ADXL345_WINDOW, settings.window);
    if (rc) {
        return rc;
    }

    enables |= settings.x_enable ? (1 << 2) : 0;
    enables |= settings.y_enable ? (1 << 1) : 0;
    enables |= settings.z_enable ? (1 << 0) : 0;
    enables |= settings.suppress ? (1 << 3) : 0;

    return adxl345_write8(itf, ADXL345_TAP_AXES, enables);
}

/**
 * Gets the current tap settings from ADXL345 sensor
 *
 * @param The sensor interface
 * @param Ponter to structure to store settings
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_get_tap_settings(struct sensor_itf *itf, struct adxl345_tap_settings *settings)
{
    int rc;
    uint8_t enables;
    
    rc = adxl345_read8(itf, ADXL345_THRESH_TAP, &(settings->threshold));
    if (rc) {
        return rc;
    }

    rc = adxl345_read8(itf, ADXL345_DUR, &(settings->duration));
    if (rc) {
        return rc;
    }

    rc = adxl345_read8(itf, ADXL345_LATENT, &(settings->latency));
    if (rc) {
        return rc;
    }

    rc = adxl345_read8(itf, ADXL345_WINDOW, &(settings->window));
    if (rc) {
        return rc;
    }

    rc = adxl345_read8(itf, ADXL345_TAP_AXES, &enables);
    if (rc) {
        return rc;
    }

    settings->x_enable = (enables & (1 << 2)) != 0;
    settings->x_enable = (enables & (1 << 1)) != 0;
    settings->x_enable = (enables & (1 << 0)) != 0;
    settings->suppress = (enables & (1 << 3)) != 0;

    return 0;
}

/**
 * Sets new threshold for triggering activity interrupt
 *
 * @param The sensor interface
 * @param threshold value (62.5mg/LSB)
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_set_active_threshold(struct sensor_itf *itf, uint8_t threshold)
{
    return adxl345_write8(itf, ADXL345_THRESH_ACT, threshold);
}

/**
 * Gets current threshold for triggering activity interrupt
 *
 * @param The sensor interface
 * @param Pointer to store threshold value (62.5mg/LSB)
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_get_active_threshold(struct sensor_itf *itf, uint8_t *threshold)
{
    return adxl345_read8(itf, ADXL345_THRESH_ACT, threshold);
}

/**
 * Sets new threshold and time for triggering inactivity interrupt
 *
 * @param The sensor interface
 * @param threshold value (62.5mg/LSB)
 * @param time value in seconds
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_set_inactive_settings(struct sensor_itf *itf, uint8_t threshold, uint8_t time)
{
    int rc;
  
    rc = adxl345_write8(itf, ADXL345_THRESH_INACT, threshold);
    if (rc){
        return rc;
    }

    return adxl345_write8(itf, ADXL345_TIME_INACT, time);
}

/**
 * Gets current threshold and time set for triggering inactivity interrupt
 *
 * @param The sensor interface
 * @param Pointer to store threshold value (62.5mg/LSB)
 * @param Ponter to store time value in seconds
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_get_inactive_settings(struct sensor_itf *itf, uint8_t *threshold, uint8_t *time)
{
    int rc;
  
    rc = adxl345_read8(itf, ADXL345_THRESH_INACT, threshold);
    if (rc){
        return rc;
    }

    return adxl345_read8(itf, ADXL345_TIME_INACT, time);
}

/**
 * Sets new activity/inactivity interrupt axis selections
 *
 * @param The sensor interface
 * @param Structure of new configuration
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_set_act_inact_enables(struct sensor_itf *itf, struct adxl345_act_inact_enables cfg)
{
    uint8_t reg = 0;

    reg |= cfg.inact_z     ? (1 << 0) : 0;
    reg |= cfg.inact_y     ? (1 << 1) : 0;
    reg |= cfg.inact_x     ? (1 << 2) : 0;
    reg |= cfg.inact_ac_dc ? (1 << 3) : 0;
    reg |= cfg.act_z       ? (1 << 4) : 0;
    reg |= cfg.act_y       ? (1 << 5) : 0;
    reg |= cfg.act_x       ? (1 << 6) : 0;
    reg |= cfg.act_ac_dc   ? (1 << 7) : 0;

    ADXL345_ERR("act_inact = 0x%x\n", reg);
    
    return adxl345_write8(itf, ADXL345_ACT_INACT_CTL, reg);
    
}

/**
 * Sets new activity/inactivity interrupt axis selections
 *
 * @param The sensor interface
 * @param Pointer to structure to store configuration
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_get_act_inact_enables(struct sensor_itf *itf, struct adxl345_act_inact_enables *cfg)
{
    int rc;
    uint8_t reg;

    rc = adxl345_read8(itf, ADXL345_ACT_INACT_CTL, &reg);
    if (rc) {
        return rc;
    }

    cfg->inact_z     = (reg & (1 << 0)) != 0;
    cfg->inact_y     = (reg & (1 << 1)) != 0;
    cfg->inact_x     = (reg & (1 << 2)) != 0;
    cfg->inact_ac_dc = (reg & (1 << 3)) != 0;
    cfg->act_z       = (reg & (1 << 4)) != 0;
    cfg->act_y       = (reg & (1 << 5)) != 0;
    cfg->act_x       = (reg & (1 << 6)) != 0;
    cfg->act_ac_dc   = (reg & (1 << 7)) != 0;

    return 0;
}

/**
 * Sets new threshold and time for triggering freefall interrupt
 *
 * @param The sensor interface
 * @param threshold value (62.5mg/LSB)
 * @param time value in seconds
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_set_freefall_settings(struct sensor_itf *itf, uint8_t threshold, uint8_t time)
{
    int rc;
  
    rc = adxl345_write8(itf, ADXL345_THRESH_FF, threshold);
    if (rc){
        return rc;
    }

    return adxl345_write8(itf, ADXL345_TIME_FF, time);
}

/**
 * Gets current threshold and time set for triggering freefall interrupt
 *
 * @param The sensor interface
 * @param Pointer to store threshold value (62.5mg/LSB)
 * @param Ponter to store time value in seconds
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_get_freefall_settings(struct sensor_itf *itf, uint8_t *threshold, uint8_t *time)
{
    int rc;
  
    rc = adxl345_read8(itf, ADXL345_THRESH_FF, threshold);
    if (rc){
        return rc;
    }

    return adxl345_read8(itf, ADXL345_TIME_FF, time);
}

/**
 * Sets new sample rate for measurements
 *
 * @param The sensor interface
 * @param sample rate value
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_set_sample_rate(struct sensor_itf *itf, enum adxl345_sample_rate rate)
{
    return adxl345_write8(itf, ADXL345_BW_RATE, (uint8_t) rate);
}

/**
 * Gets current sample rate settings
 *
 * @param The sensor interface
 * @param Pointer to store rate value
 *
 * @return 0 on success, non-zero error on failure.
 */
int
adxl345_get_sample_rate(struct sensor_itf *itf, enum adxl345_sample_rate *rate)
{
    int rc;
    uint8_t reg;
  
    rc = adxl345_read8(itf, ADXL345_BW_RATE, &reg);
    if (rc){
        return rc;
    }

    *rate = (enum adxl345_sample_rate)(reg & 0xF);    
    return 0;
}

/**
 * Configures which interrupts are enabled and which interrupt pins they are 
 * mapped to.
 *
 * @param The sensor interface
 * @param which interrupts are enabled
 * @param which interrupts are mapped to which pin
 *
 * @return 0 on success, non-zero error on failure
 */
int
adxl345_setup_interrupts(struct sensor_itf *itf, uint8_t enables, uint8_t mapping)
{
    int rc;

    rc = adxl345_write8(itf, ADXL345_INT_MAP, mapping);
    if (rc) {
        return rc;
    }

    return adxl345_write8(itf, ADXL345_INT_ENABLE, enables);
}

/**
 * Clears interrupts (except DATA_READY, Watermark & Overrun which need data to
 * be read to clear). Provides status as output to identify which interrupts have
 * triggered.
 * 
 * @param The sensor interface
 * @param Pointer to store interrupt status
 *
 * @return 0 on success, non-zero error on failure 
 */
int adxl345_clear_interrupts(struct sensor_itf *itf, uint8_t *int_status)
{
    return adxl345_read8(itf, ADXL345_INT_SOURCE, int_status);
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
adxl345_init(struct os_dev *dev, void *arg)
{
    struct adxl345 *adxl;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        return SYS_ENODEV;
    }

    adxl = (struct adxl345 *) dev;

    adxl->cfg.mask = SENSOR_TYPE_ALL;

    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);

    sensor = &adxl->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_adxl345stats),
        STATS_SIZE_INIT_PARMS(g_adxl345stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(adxl345_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_adxl345stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = sensor_init(sensor, dev);
    if (rc) {
        return rc;
    }

    /* Add the accelerometer/gyroscope driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_ACCELEROMETER,
                           (struct sensor_driver *) &adxl345_sensor_driver);
    if (rc) {
        return rc;
    }

    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        return rc;
    }

    rc = sensor_mgr_register(sensor);
    if (rc) {
        return rc;
    }

    if (sensor->s_itf.si_type == SENSOR_ITF_SPI) {
        rc = hal_spi_config(sensor->s_itf.si_num, &spi_adxl345_settings);
        if (rc == EINVAL) {
            return rc;
        }
        
        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            return rc;
        }
        
        rc = hal_gpio_init_out(sensor->s_itf.si_cs_pin, 1);
        if (rc) {
            return rc;
        }
    }

#if MYNEWT_VAL(ADXL345_INT_ENABLE)
    adxl->pdd.read_ctx.srec_sensor = sensor;
    adxl->pdd.notify_ctx.snec_sensor = sensor;

    rc = init_intpin(adxl, interrupt_handler, sensor);
    if (rc != 0) {
        return rc;
    }
#endif

    
    return 0;
}

/**
 * Configure ADXL345 sensor
 *
 * @param Sensor device ADXL345 structure
 * @param Sensor device ADXL345 config
 *
 * @return 0 on success, non-zero on failure
 */
int
adxl345_config(struct adxl345 *dev, struct adxl345_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(dev->sensor));

    /* Check device id is correct */
    uint8_t val = 0;
    rc = adxl345_read8(itf, ADXL345_DEVID, &val);
    if (rc) {
        return rc;
    }
    if (val != ADXL345_DEVID_VAL) {
        return SYS_EINVAL;
    }

    rc = adxl345_read8(itf, ADXL345_DATA_FORMAT, &val);
    if (rc) {
        return rc;
    }

    /* Set accelerometer into full resolution */
    val |= 0x08;

    /* Set interupt pin polarity to match local pin setting */
    if (dev->sensor.s_itf.si_ints[dev->pdd.int_num].active) {
        val &= ~0x20;
    } else {
        val |= 0x20;
    }

    rc = adxl345_write8(itf, ADXL345_DATA_FORMAT, val);
    if (rc) {
        return rc;
    } 
    
    /* Setup range as per config */
    rc = adxl345_set_accel_range(itf, cfg->accel_range);
    if (rc) {
        return rc;
    }
    dev->cfg.accel_range = cfg->accel_range;

    /* setup sample rate */
    rc = adxl345_set_sample_rate(itf, cfg->sample_rate);
    if (rc) {
        return rc;
    }
    dev->cfg.sample_rate = cfg->sample_rate;
        
    /* setup data offsets */
    rc = adxl345_set_offsets(itf, cfg->offset_x, cfg->offset_y, cfg->offset_z);
    if (rc) {
        return rc;
    }
    dev->cfg.offset_x = cfg->offset_x;
    dev->cfg.offset_y = cfg->offset_y;
    dev->cfg.offset_z = cfg->offset_z;

    /* setup tap detection settings */
    rc = adxl345_set_tap_settings(itf, cfg->tap_cfg);
    if (rc) {
        return rc;
    }
    dev->cfg.tap_cfg = cfg->tap_cfg;

    /* setup activity detection settings */
    rc = adxl345_set_active_threshold(itf, 0xFF);
    if (rc) {
        return rc;
    }
    
    rc = adxl345_set_inactive_settings(itf, 0, 0);
    if (rc) {
        return rc;
    }

    /* setup freefall settings */
    rc = adxl345_set_freefall_settings(itf, cfg->freefall_threshold, cfg->freefall_time);
    if (rc) {
        return rc;
    }
    dev->cfg.freefall_threshold = cfg->freefall_threshold;
    dev->cfg.freefall_time = cfg->freefall_time;

    /* setup low power mode */
    rc = adxl345_set_low_power_enable(itf, cfg->low_power_enable);
    if (rc) {
        return rc;
    }
    dev->cfg.low_power_enable = cfg->low_power_enable;
    
    /* setup default interrupt config */
    rc = adxl345_setup_interrupts(itf, 0, 0);
    if (rc) {
        return rc;
    }

    rc = adxl345_clear_interrupts(itf, &val);
    if (rc) {
        return rc;
    }
    
    /* Setup current power mode */
    rc = adxl345_set_power_mode(itf, cfg->power_mode);
    if (rc) {
        return rc;
    } 
    dev->cfg.power_mode = cfg->power_mode;

    
    rc = sensor_set_type_mask(&(dev->sensor), cfg->mask);
    if (rc) {
        return rc;
    }

    dev->cfg.mask = cfg->mask;

    return 0;
}

static float adxl345_convert_reg_to_ms2(int16_t val, float lsb_mg)
{
    float tmp = val * lsb_mg * 0.001; /* convert to g */
    return tmp * STANDARD_ACCEL_GRAVITY; /* convert to ms2 */
}

static uint8_t adxl345_convert_ms2_to_reg(float ms2, float lsb_mg)
{
    float tmp = (ms2 * 1000) / STANDARD_ACCEL_GRAVITY; /* convert to mg */
    tmp /= lsb_mg; /* convert to reg format */   
    return (uint8_t) tmp;
}

/**
 * Reads Accelerometer data from ADXL345 sensor
 *
 * @param Pointer to sensor interface
 * @param Pointer to structure to store result
 *
 * @return 0 on success, non-zero on failure
 */
int
adxl345_get_accel_data(struct sensor_itf *itf, struct sensor_accel_data *sad)
{
    int rc;

    uint8_t payload[6];
    int16_t x,y,z;

    float lsb_mg = 4; /* lsb is always 4mg when device is set to full resolution */

    /* Get a new accelerometer sample */
    rc = adxl345_readlen(itf, ADXL345_DATAX0, payload, 6);
    if (rc) {
        return rc;
    }

    x = (((int16_t)payload[1]) << 8) | payload[0];
    y = (((int16_t)payload[3]) << 8) | payload[2];
    z = (((int16_t)payload[5]) << 8) | payload[4];   

    /* calculate MS*2 values to provide to data_func */
    sad->sad_x = adxl345_convert_reg_to_ms2(x, lsb_mg);
    sad->sad_x_is_valid = 1;
    sad->sad_y = adxl345_convert_reg_to_ms2(y, lsb_mg);
    sad->sad_y_is_valid = 1;
    sad->sad_z = adxl345_convert_reg_to_ms2(z, lsb_mg);
    sad->sad_z_is_valid = 1;

    return 0;
}

/**
 * Read Accelerometer data from ADXL345 sensor
 *
 * @param Pointer to sensor structure
 * @param Type of sensor data to read
 * @param Pointer to function to call to output data
 * @param Pointer to data to pass to data_func
 * @param timeout value
 *
 * @return 0 on success, non-zero on failure
 */
static int
adxl345_sensor_read(struct sensor *sensor, sensor_type_t type,
                    sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    (void)timeout;
    int rc;

    struct sensor_itf *itf;
    struct sensor_accel_data sad;
  
    /* If the read isn't looking for accel don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER)) {
        return SYS_EINVAL;
    }
  
    itf = SENSOR_GET_ITF(sensor);
  
    rc = adxl345_get_accel_data(itf, &sad);
    if (rc) {
        return rc;
    }

    /* output data using data_func */
    rc = data_func(sensor, data_arg, &sad, SENSOR_TYPE_ACCELEROMETER);
    if (rc) {
        return rc;
    }
  
    return 0;
}

static int
adxl345_sensor_get_config(struct sensor *sensor, sensor_type_t type,
                          struct sensor_cfg *cfg)
{
    /* If the read isn't looking for accel, don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER)) {
        return SYS_EINVAL;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return 0;
}

#if MYNEWT_VAL(ADXL345_INT_ENABLE)
static void
interrupt_handler(void * arg)
{
    struct sensor *sensor = arg;
    sensor_mgr_put_interrupt_evt(sensor);
}

/**
 * Initialises the local interrupt pin 
 *
 * @param Pointer to device structure
 * @param interrupt handler to setup
 * @param Pointer to data to pass to irq
 *
 * @return 0 on success, non-zero on failure
 */
static int
init_intpin(struct adxl345 * adxl345, hal_gpio_irq_handler_t handler,
            void * arg)
{
    struct adxl345_private_driver_data *pdd = &adxl345->pdd;
    hal_gpio_irq_trig_t trig;
    int pin = -1;
    int rc;
    int i;

    for (i = 0; i < MYNEWT_VAL(SENSOR_MAX_INTERRUPTS_PINS); i++){
        pin = adxl345->sensor.s_itf.si_ints[i].host_pin;
        if (pin >= 0) {
            break;
        }
    }

    if (pin < 0) {
        ADXL345_ERR("Interrupt pin not configured\n");
        return SYS_EINVAL;
    }

    pdd->int_num = i;
    if (adxl345->sensor.s_itf.si_ints[pdd->int_num].active) {
        trig = HAL_GPIO_TRIG_RISING;
    } else {
        trig = HAL_GPIO_TRIG_FALLING;
    }
  
    if (adxl345->sensor.s_itf.si_ints[pdd->int_num].device_pin == 1) {
        pdd->int_route = 0x00;
    } else if (adxl345->sensor.s_itf.si_ints[pdd->int_num].device_pin == 2) {
        pdd->int_route = 0xFF;
    } else {
        ADXL345_ERR("Route not configured\n");
        return SYS_EINVAL;
    }

    rc = hal_gpio_irq_init(pin,
                           handler,
                           arg,
                           trig,
                           HAL_GPIO_PULL_NONE);
    if (rc != 0) {
        ADXL345_ERR("Failed to initialise interrupt pin %d\n", pin);
        return rc;
    } 
    
    return 0;
}

/**
 * Enables an interrupt in ADXL345 device
 *
 * @param Pointer to sensor structure
 * @param which interrupt(s) to enable
 *
 * @return 0 on success, non-zero on failure
 */
static int
enable_interrupt(struct sensor * sensor, uint8_t ints_to_enable)
{
    struct adxl345_private_driver_data *pdd;
    struct adxl345 *adxl345;
    struct sensor_itf *itf;

    if (ints_to_enable == 0) {
        return SYS_EINVAL;
    }

    adxl345 = (struct adxl345 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &adxl345->pdd;
 
    /* if no interrupts are currently in use enable int pin */
    if (pdd->int_enable == 0) {
        hal_gpio_irq_enable(adxl345->sensor.s_itf.si_ints[pdd->int_num].host_pin);
    }
    
    /* update which interrupts are enabled */
    pdd->int_enable |= ints_to_enable;

    /* enable interrupt in device */
    return adxl345_setup_interrupts(itf, pdd->int_enable, pdd->int_route);
}

/**
 * Disables an interrupt in ADXL345 device
 *
 * @param Pointer to sensor structure
 * @param which interrupt(s) to disable
 *
 * @return 0 on success, non-zero on failure
 */
static int
disable_interrupt(struct sensor * sensor, uint8_t ints_to_disable)
{
    struct adxl345_private_driver_data *pdd;
    struct adxl345 *adxl345;
    struct sensor_itf *itf;

    if (ints_to_disable == 0) {
        return SYS_EINVAL;
    }

    adxl345 = (struct adxl345 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &adxl345->pdd;
    
    /* update which interrupts are enabled */
    pdd->int_enable &= ~ints_to_disable;

    /* if no interrupts are now in use disable int pin */
    if (pdd->int_enable == 0) {
        hal_gpio_irq_disable(adxl345->sensor.s_itf.si_ints[pdd->int_num].host_pin);
    }

    /* update interrupt setup in device */
    return adxl345_setup_interrupts(itf, pdd->int_enable, pdd->int_route);
}
#endif

/**
 * Handles and interrupt, firing correct events
 *
 * @param Pointer to sensor structure
 *
 * @return 0 on success, non-zero on failure
 */
static int
adxl345_sensor_handle_interrupt(struct sensor * sensor)
{
#if MYNEWT_VAL(ADXL345_INT_ENABLE)
    struct adxl345 * adxl345;
    struct adxl345_private_driver_data *pdd;
    struct sensor_itf *itf;
    uint8_t int_status;
    
    int rc;

    adxl345 = (struct adxl345 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);

    pdd = &adxl345->pdd;

    rc = adxl345_clear_interrupts(itf, &int_status);
    if (rc != 0) {
        ADXL345_ERR("Cound not read int status err=0x%02x\n", rc);
        return rc;
    }

    if (pdd->registered_mask & ADXL345_NOTIFY_MASK) {
        if (int_status & ADXL345_INT_SINGLE_TAP_BIT) {
            sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_SINGLE_TAP);
        }

        if (int_status & ADXL345_INT_DOUBLE_TAP_BIT) {
            sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_DOUBLE_TAP);
        }
    }

    if ((pdd->registered_mask & ADXL345_READ_MASK) &&
        ((int_status & ADXL345_INT_ACTIVITY_BIT) ||
         (int_status & ADXL345_INT_INACTIVITY_BIT))) {
        ADXL345_ERR("READ EVT 0x%02x\n", int_status);
        sensor_mgr_put_read_evt(&pdd->read_ctx);
    }

    return 0;
#else
    return SYS_ENODEV;
#endif
}

/**
 * Sets up trigger thresholds and enables interrupts
 *
 * @param Pointer to sensor structure
 * @param type of sensor
 * @param threshold settings to configure
 *
 * @return 0 on success, non-zero on failure
 */
static int
adxl345_sensor_set_trigger_thresh(struct sensor * sensor,
                                  sensor_type_t sensor_type,
                                  struct sensor_type_traits * stt)
{
#if MYNEWT_VAL(ADXL345_INT_ENABLE)
    struct adxl345 * adxl345;
    struct sensor_itf *itf;
    int rc;
    const struct sensor_accel_data * low_thresh;
    const struct sensor_accel_data * high_thresh;
    uint8_t ints_to_enable = 0;
    float thresh;
    struct adxl345_private_driver_data *pdd;
    struct adxl345_act_inact_enables axis_enables = { 0 };

    if (sensor_type != SENSOR_TYPE_ACCELEROMETER) {
        return SYS_EINVAL;
    }

    adxl345 = (struct adxl345 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &adxl345->pdd;

    low_thresh  = stt->stt_low_thresh.sad;
    high_thresh = stt->stt_high_thresh.sad;

    if (low_thresh->sad_x_is_valid |
        low_thresh->sad_y_is_valid |
        low_thresh->sad_z_is_valid) {
        thresh = INFINITY;

        if (low_thresh->sad_x_is_valid) {
            axis_enables.inact_x = 1;
            if (thresh > low_thresh->sad_x) {
                thresh = low_thresh->sad_x;
            }
        }
        if (low_thresh->sad_y_is_valid) {
            axis_enables.inact_y = 1;
            if (thresh > low_thresh->sad_y) {
                thresh = low_thresh->sad_y;
            }
        }
        if (low_thresh->sad_z_is_valid) {
            axis_enables.inact_z = 1;
            if (thresh > low_thresh->sad_z) {
                thresh = low_thresh->sad_z;
            }
        }

        rc = adxl345_set_inactive_settings(itf,
                adxl345_convert_ms2_to_reg(thresh, 62.5), 2);
        
        if (rc) {
            return rc;
        }

        ints_to_enable |= ADXL345_INT_INACTIVITY_BIT;
    }

    if (high_thresh->sad_x_is_valid |
        high_thresh->sad_y_is_valid |
        high_thresh->sad_z_is_valid) {
        thresh = 0.0;

        if (high_thresh->sad_x_is_valid) {
            axis_enables.act_x = 1;
            if (thresh < high_thresh->sad_x) {
                thresh = high_thresh->sad_x;
            }
        }
        if (high_thresh->sad_y_is_valid) {
            axis_enables.act_y = 1;
            if (thresh < high_thresh->sad_y) {
                thresh = high_thresh->sad_y;
            }
        }
        if (high_thresh->sad_z_is_valid) {
            axis_enables.act_z = 1;
            if (thresh < high_thresh->sad_z) {
                thresh = high_thresh->sad_z;
            }
        }

        rc = adxl345_set_active_threshold(itf,
                   adxl345_convert_ms2_to_reg(thresh, 62.5));
        if (rc) {
            return rc;
        }

        ints_to_enable |= ADXL345_INT_ACTIVITY_BIT;
       
    }

    rc = enable_interrupt(sensor, ints_to_enable);
    if (rc) {
        return rc;
    }

    rc = adxl345_set_act_inact_enables(itf, axis_enables);
    if (rc) {
        return rc;
    }
    
    pdd->read_ctx.srec_type |= sensor_type;
    pdd->registered_mask |= ADXL345_READ_MASK;

    return 0;
#else
    return SYS_ENODEV;
#endif
}

/**
 * Disable the low threshold interrupt
 *
 * @param ptr to sensor
 * @param the Sensor type
 *
 * @return 0 on success, non-zero on failure
 */
static int
adxl345_sensor_clear_low_thresh(struct sensor *sensor,
                                 sensor_type_t type)
{
    struct adxl345 *adxl345;
    uint8_t ints_to_disable = ADXL345_INT_INACTIVITY_BIT;

    if (type != SENSOR_TYPE_ACCELEROMETER) {
        return SYS_EINVAL;
    }

    adxl345 = (struct adxl345 *)SENSOR_GET_DEVICE(sensor);

    /* if neither high or low threshs are now set disable read mask */
    if((adxl345->pdd.int_enable & ADXL345_INT_ACTIVITY_BIT) == 0) {
        adxl345->pdd.read_ctx.srec_type &= ~type;
        adxl345->pdd.registered_mask &= ~ADXL345_READ_MASK;
    }

    return disable_interrupt(sensor, ints_to_disable);
}


/**
 * Disable the high threshold interrupt
 *
 * @param ptr to sensor
 * @param the Sensor type
 *
 * @return 0 on success, non-zero on failure
 */
static int
adxl345_sensor_clear_high_thresh(struct sensor *sensor,
                                  sensor_type_t type)
{
    struct adxl345 *adxl345;
    uint8_t ints_to_disable = ADXL345_INT_ACTIVITY_BIT;

    if (type != SENSOR_TYPE_ACCELEROMETER) {
        return SYS_EINVAL;
    }

    adxl345 = (struct adxl345 *)SENSOR_GET_DEVICE(sensor);

    /* if neither high or low threshs are now set disable read mask */
    if((adxl345->pdd.int_enable & ADXL345_INT_INACTIVITY_BIT) == 0) {
        adxl345->pdd.read_ctx.srec_type &= ~type;
        adxl345->pdd.registered_mask &= ~ADXL345_READ_MASK;
    }

    return disable_interrupt(sensor, ints_to_disable);
}

/**
 * Enables notifications on tap or double tap events
 *
 * @param Pointer to sensor structure
 * @param which event to get notifications for
 *
 * @return 0 on success, non-zero on failure
 */
static int
adxl345_sensor_set_notification(struct sensor * sensor,
                                sensor_event_type_t sensor_event_type)
{
#if MYNEWT_VAL(ADXL345_INT_ENABLE)
    struct adxl345 * adxl345;
    uint8_t ints_to_enable = 0;
    struct adxl345_private_driver_data *pdd;
    int rc;

    ADXL345_ERR("Enabling notifications\n");
    
    if ((sensor_event_type & ~(SENSOR_EVENT_TYPE_DOUBLE_TAP |
                               SENSOR_EVENT_TYPE_SINGLE_TAP)) != 0) {
        return SYS_EINVAL;
    }

    /*XXX for now we do not support registering for both events */
    if (sensor_event_type == (SENSOR_EVENT_TYPE_DOUBLE_TAP |
                                        SENSOR_EVENT_TYPE_SINGLE_TAP)) {
        return SYS_EINVAL;
    }

    adxl345 = (struct adxl345 *)SENSOR_GET_DEVICE(sensor);
    pdd = &adxl345->pdd;

    if (pdd->registered_mask & ADXL345_NOTIFY_MASK) {
        return SYS_EBUSY;
    }

    /* Enable tap event*/
    if(sensor_event_type == SENSOR_EVENT_TYPE_DOUBLE_TAP) {
        ints_to_enable |= ADXL345_INT_DOUBLE_TAP_BIT;
    }
    if(sensor_event_type == SENSOR_EVENT_TYPE_SINGLE_TAP) {
        ints_to_enable |= ADXL345_INT_SINGLE_TAP_BIT;
    }

    rc = enable_interrupt(sensor, ints_to_enable);
    if (rc) {
        return rc;
    }

    pdd->notify_ctx.snec_evtype |= sensor_event_type;
    pdd->registered_mask |= ADXL345_NOTIFY_MASK;

    ADXL345_ERR("Enabled notifications\n");
    
    return 0;
#else
    return SYS_ENODEV;
#endif
}


/**
 * Disables notifications on tap or double tap events
 *
 * @param Pointer to sensor structure
 * @param which event to get notifications for
 *
 * @return 0 on success, non-zero on failure
 */
static int
adxl345_sensor_unset_notification(struct sensor * sensor,
                                  sensor_event_type_t sensor_event_type)
{
#if MYNEWT_VAL(ADXL345_INT_ENABLE)
    struct adxl345 * adxl345;
    uint8_t ints_to_disable = 0;

    if ((sensor_event_type & ~(SENSOR_EVENT_TYPE_DOUBLE_TAP |
                               SENSOR_EVENT_TYPE_SINGLE_TAP)) != 0) {
        return SYS_EINVAL;
    }

    /*XXX for now we do not support registering for both events */
    if (sensor_event_type == (SENSOR_EVENT_TYPE_DOUBLE_TAP |
                                        SENSOR_EVENT_TYPE_SINGLE_TAP)) {
        return SYS_EINVAL;
    }

    adxl345 = (struct adxl345 *)SENSOR_GET_DEVICE(sensor);

    adxl345->pdd.notify_ctx.snec_evtype &= ~sensor_event_type;
    adxl345->pdd.registered_mask &= ~ADXL345_NOTIFY_MASK;

    ints_to_disable |= ADXL345_INT_SINGLE_TAP_BIT;
    ints_to_disable |= ADXL345_INT_DOUBLE_TAP_BIT;
   
    return disable_interrupt(sensor, ints_to_disable);
    
#else
    return SYS_ENODEV;
#endif
}



